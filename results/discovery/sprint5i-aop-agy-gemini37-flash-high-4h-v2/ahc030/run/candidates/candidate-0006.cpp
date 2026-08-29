#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <string>
#include <set>

using namespace std;

struct Point {
    int r, c;
};

struct Polyomino {
    int id;
    int area;
    int h, w;
    vector<Point> cells;
};

struct Placement {
    int r, c;
    uint64_t mask[7];
    vector<int> cell_indices;
};

struct DivQuery {
    vector<int> cells;
    uint64_t mask[7];
    int d;
    int y;
    vector<double> log_lik; // log_lik[v]
    vector<vector<uint8_t>> shape_pl_overlap; // [shape][pl_idx]
};

struct State {
    vector<int> p; // placement index for each shape
    vector<int> v; // sum at each cell (size N*N)
    vector<int> div_sums; // sum for each div query
    int drill_violations;
    double div_log_lik;
};

static inline double normal_cdf(double x) {
    return 0.5 * std::erfc(-x * 0.70710678118654752440);
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    auto t_start = chrono::steady_clock::now();
    auto get_elapsed_ms = [&]() -> int64_t {
        return chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t_start).count();
    };

    int n, m;
    double epsilon;
    if (!(cin >> n >> m >> epsilon)) return 0;

    int total_cells = n * n;
    int words = (total_cells + 63) / 64;

    vector<Polyomino> polyominoes(m);
    for (int i = 0; i < m; ++i) {
        polyominoes[i].id = i;
        cin >> polyominoes[i].area;
        polyominoes[i].cells.resize(polyominoes[i].area);
        int max_r = 0, max_c = 0;
        for (int j = 0; j < polyominoes[i].area; ++j) {
            cin >> polyominoes[i].cells[j].r >> polyominoes[i].cells[j].c;
            max_r = max(max_r, polyominoes[i].cells[j].r);
            max_c = max(max_c, polyominoes[i].cells[j].c);
        }
        polyominoes[i].h = max_r + 1;
        polyominoes[i].w = max_c + 1;
    }

    // Precompute placements for each polyomino
    vector<vector<Placement>> all_placements(m);
    // Index placements covering each cell: cell_placements[shape][cell_idx] -> vector of pl_idx
    vector<vector<vector<int>>> cell_placements(m, vector<vector<int>>(total_cells));

    for (int i = 0; i < m; ++i) {
        int max_top_r = n - polyominoes[i].h;
        int max_top_c = n - polyominoes[i].w;
        for (int r = 0; r <= max_top_r; ++r) {
            for (int c = 0; c <= max_top_c; ++c) {
                int pl_idx = all_placements[i].size();
                Placement pl;
                pl.r = r;
                pl.c = c;
                for (int w = 0; w < 7; ++w) pl.mask[w] = 0;
                for (const auto& cell : polyominoes[i].cells) {
                    int cr = r + cell.r;
                    int cc = c + cell.c;
                    int idx = cr * n + cc;
                    pl.mask[idx >> 6] |= (1ULL << (idx & 63));
                    pl.cell_indices.push_back(idx);
                    cell_placements[i][idx].push_back(pl_idx);
                }
                all_placements[i].push_back(pl);
            }
        }
    }

    vector<int> drill_res(total_cells, -1);
    vector<int> drilled_cells_list;
    vector<DivQuery> div_queries;
    set<vector<int>> queried_div_sets;

    // Filter valid placements
    vector<vector<int>> valid_placements(m);
    for (int i = 0; i < m; ++i) {
        for (int p_idx = 0; p_idx < (int)all_placements[i].size(); ++p_idx) {
            valid_placements[i].push_back(p_idx);
        }
    }

    auto update_valid_placements_for_zero = [&](int cell_idx) {
        for (int i = 0; i < m; ++i) {
            vector<int> next_valid;
            for (int p_idx : valid_placements[i]) {
                const auto& pl = all_placements[i][p_idx];
                if (((pl.mask[cell_idx >> 6] >> (cell_idx & 63)) & 1ULL) == 0) {
                    next_valid.push_back(p_idx);
                }
            }
            valid_placements[i] = move(next_valid);
        }
    };

    mt19937 rng(42);

    auto init_random_state = [&]() -> State {
        State st;
        st.p.resize(m);
        st.v.assign(total_cells, 0);
        st.drill_violations = 0;
        st.div_log_lik = 0.0;
        st.div_sums.assign(div_queries.size(), 0);

        for (int i = 0; i < m; ++i) {
            if (valid_placements[i].empty()) {
                st.p[i] = uniform_int_distribution<int>(0, (int)all_placements[i].size() - 1)(rng);
            } else {
                int r_idx = uniform_int_distribution<int>(0, (int)valid_placements[i].size() - 1)(rng);
                st.p[i] = valid_placements[i][r_idx];
            }
            for (int idx : all_placements[i][st.p[i]].cell_indices) {
                st.v[idx]++;
            }
        }

        for (int idx : drilled_cells_list) {
            st.drill_violations += abs(st.v[idx] - drill_res[idx]);
        }

        for (size_t q = 0; q < div_queries.size(); ++q) {
            int sv = 0;
            for (int i = 0; i < m; ++i) {
                sv += div_queries[q].shape_pl_overlap[i][st.p[i]];
            }
            st.div_sums[q] = sv;
            if (sv >= 0 && sv < (int)div_queries[q].log_lik.size()) {
                st.div_log_lik += div_queries[q].log_lik[sv];
            } else {
                st.div_log_lik += -50.0;
            }
        }

        return st;
    };

    auto run_mcmc = [&](State& st, int steps) {
        uniform_int_distribution<int> shape_dist(0, m - 1);
        uniform_real_distribution<double> real_dist(0.0, 1.0);
        int num_queries = div_queries.size();

        for (int step = 0; step < steps; ++step) {
            int shape = -1;
            int new_p = -1;

            // Targeted proposal with 50% probability if there are violations
            bool do_targeted = (st.drill_violations > 0 && !drilled_cells_list.empty() && (real_dist(rng) < 0.5));
            if (do_targeted) {
                int u = drilled_cells_list[uniform_int_distribution<int>(0, (int)drilled_cells_list.size() - 1)(rng)];
                int diff = st.v[u] - drill_res[u];
                if (diff < 0) { // need more oil at u
                    shape = shape_dist(rng);
                    const auto& c_pls = cell_placements[shape][u];
                    if (!c_pls.empty()) {
                        new_p = c_pls[uniform_int_distribution<int>(0, (int)c_pls.size() - 1)(rng)];
                    }
                } else if (diff > 0) { // need less oil at u
                    // Find a shape covering u
                    vector<int> covering_shapes;
                    for (int s = 0; s < m; ++s) {
                        if ((all_placements[s][st.p[s]].mask[u >> 6] >> (u & 63)) & 1ULL) {
                            covering_shapes.push_back(s);
                        }
                    }
                    if (!covering_shapes.empty()) {
                        shape = covering_shapes[uniform_int_distribution<int>(0, (int)covering_shapes.size() - 1)(rng)];
                        if (!valid_placements[shape].empty()) {
                            new_p = valid_placements[shape][uniform_int_distribution<int>(0, (int)valid_placements[shape].size() - 1)(rng)];
                        }
                    }
                }
            }

            if (shape == -1 || new_p == -1) {
                shape = shape_dist(rng);
                if (valid_placements[shape].size() <= 1) continue;
                int r_idx = uniform_int_distribution<int>(0, (int)valid_placements[shape].size() - 1)(rng);
                new_p = valid_placements[shape][r_idx];
            }

            int old_p = st.p[shape];
            if (old_p == new_p) continue;

            const auto& old_pl = all_placements[shape][old_p];
            const auto& new_pl = all_placements[shape][new_p];

            // Compute delta violations on drilled cells
            int delta_violations = 0;
            for (int idx : old_pl.cell_indices) {
                if (drill_res[idx] != -1) {
                    int old_diff = abs(st.v[idx] - drill_res[idx]);
                    int new_diff = abs((st.v[idx] - 1) - drill_res[idx]);
                    delta_violations += (new_diff - old_diff);
                }
            }
            for (int idx : old_pl.cell_indices) st.v[idx]--;
            for (int idx : new_pl.cell_indices) {
                if (drill_res[idx] != -1) {
                    int cur_diff = abs(st.v[idx] - drill_res[idx]);
                    int next_diff = abs((st.v[idx] + 1) - drill_res[idx]);
                    delta_violations += (next_diff - cur_diff);
                }
            }
            for (int idx : new_pl.cell_indices) st.v[idx]++;

            // Compute delta div log-likelihood using precomputed overlap
            double delta_log_lik = 0.0;
            for (int q = 0; q < num_queries; ++q) {
                int old_ov = div_queries[q].shape_pl_overlap[shape][old_p];
                int new_ov = div_queries[q].shape_pl_overlap[shape][new_p];
                int d_sv = new_ov - old_ov;
                if (d_sv != 0) {
                    int old_sv = st.div_sums[q];
                    int next_sv = old_sv + d_sv;
                    double old_ll = (old_sv >= 0 && old_sv < (int)div_queries[q].log_lik.size()) ? div_queries[q].log_lik[old_sv] : -50.0;
                    double next_ll = (next_sv >= 0 && next_sv < (int)div_queries[q].log_lik.size()) ? div_queries[q].log_lik[next_sv] : -50.0;
                    delta_log_lik += (next_ll - old_ll);
                }
            }

            bool accept = false;
            int new_viol = st.drill_violations + delta_violations;
            if (new_viol < st.drill_violations) {
                accept = true;
            } else if (new_viol == st.drill_violations) {
                if (delta_violations == 0) {
                    if (delta_log_lik >= 0.0) {
                        accept = true;
                    } else {
                        double p_acc = exp(delta_log_lik);
                        if (real_dist(rng) < p_acc) accept = true;
                    }
                }
            } else {
                double p_acc = exp(-delta_violations * 4.0 + delta_log_lik);
                if (real_dist(rng) < p_acc) accept = true;
            }

            if (accept) {
                for (int q = 0; q < num_queries; ++q) {
                    int old_ov = div_queries[q].shape_pl_overlap[shape][old_p];
                    int new_ov = div_queries[q].shape_pl_overlap[shape][new_p];
                    st.div_sums[q] += (new_ov - old_ov);
                }
                st.p[shape] = new_p;
                st.drill_violations = new_viol;
                st.div_log_lik += delta_log_lik;
            } else {
                // Revert
                for (int idx : new_pl.cell_indices) st.v[idx]--;
                for (int idx : old_pl.cell_indices) st.v[idx]++;
            }
        }
    };

    auto do_drill = [&](int r, int c) -> int {
        int idx = r * n + c;
        if (drill_res[idx] != -1) return drill_res[idx];

        cout << "q 1 " << r << ' ' << c << endl;
        int amt;
        cin >> amt;
        drill_res[idx] = amt;
        drilled_cells_list.push_back(idx);

        if (amt == 0) {
            update_valid_placements_for_zero(idx);
        }
        return amt;
    };

    auto do_divine = [&](const vector<int>& cells) -> int {
        int d = cells.size();
        cout << "q " << d;
        for (int idx : cells) {
            cout << ' ' << idx / n << ' ' << idx % n;
        }
        cout << endl;

        int y;
        cin >> y;

        DivQuery dq;
        dq.cells = cells;
        dq.d = d;
        dq.y = y;
        for (int w = 0; w < 7; ++w) dq.mask[w] = 0;
        for (int idx : cells) dq.mask[idx >> 6] |= (1ULL << (idx & 63));

        double mu_const = d * epsilon;
        double mu_coeff = 1.0 - 2.0 * epsilon;
        double sigma = sqrt(d * epsilon * (1.0 - epsilon));

        int max_possible_v = m * d;
        dq.log_lik.resize(max_possible_v + 1);
        for (int v = 0; v <= max_possible_v; ++v) {
            double mu = mu_const + v * mu_coeff;
            double p = 0.0;
            if (y == 0) {
                double z = (0.5 - mu) / sigma;
                p = normal_cdf(z);
            } else {
                double z_high = (y + 0.5 - mu) / sigma;
                double z_low = (y - 0.5 - mu) / sigma;
                p = normal_cdf(z_high) - normal_cdf(z_low);
            }
            if (p < 1e-12) p = 1e-12;
            dq.log_lik[v] = log(p);
        }

        // Precompute overlap table for each shape and placement
        dq.shape_pl_overlap.resize(m);
        for (int i = 0; i < m; ++i) {
            int num_pl = all_placements[i].size();
            dq.shape_pl_overlap[i].resize(num_pl);
            for (int p_idx = 0; p_idx < num_pl; ++p_idx) {
                int ov = 0;
                for (int w = 0; w < words; ++w) {
                    ov += __builtin_popcountll(all_placements[i][p_idx].mask[w] & dq.mask[w]);
                }
                dq.shape_pl_overlap[i][p_idx] = (uint8_t)ov;
            }
        }

        div_queries.push_back(dq);
        queried_div_sets.insert(cells);
        return y;
    };

    const int NUM_PARTICLES = 40;
    vector<State> particles(NUM_PARTICLES);
    for (int i = 0; i < NUM_PARTICLES; ++i) {
        particles[i] = init_random_state();
    }

    struct QueryCand {
        bool is_drill;
        int cell_idx;
        vector<int> cells;
    };

    auto generate_candidate_queries = [&]() -> vector<QueryCand> {
        vector<QueryCand> cands;
        // 1. Single cell drills for undrilled cells
        for (int idx = 0; idx < total_cells; ++idx) {
            if (drill_res[idx] == -1) {
                cands.push_back({true, idx, {idx}});
            }
        }
        // 2. Coarse block divinations
        for (int bs : {3, 4}) {
            if (bs > n) continue;
            for (int r = 0; r <= n - bs; r += bs) {
                for (int c = 0; c <= n - bs; c += bs) {
                    vector<int> block;
                    for (int dr = 0; dr < bs; ++dr) {
                        for (int dc = 0; dc < bs; ++dc) {
                            block.push_back((r + dr) * n + (c + dc));
                        }
                    }
                    sort(block.begin(), block.end());
                    if (queried_div_sets.find(block) == queried_div_sets.end()) {
                        cands.push_back({false, -1, block});
                    }
                }
            }
        }
        // 3. Quadrants
        int half = n / 2;
        vector<vector<int>> quads(4);
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n; ++c) {
                int q = (r >= half ? 2 : 0) + (c >= half ? 1 : 0);
                quads[q].push_back(r * n + c);
            }
        }
        for (auto& quad : quads) {
            sort(quad.begin(), quad.end());
            if (queried_div_sets.find(quad) == queried_div_sets.end()) {
                cands.push_back({false, -1, quad});
            }
        }
        return cands;
    };

    // Active learning loop
    while (true) {
        // Time safety check: if reaching 2.0s, answer immediately
        if (get_elapsed_ms() > 2000) {
            vector<Point> ans;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] > 0) ans.push_back({idx / n, idx % n});
            }
            cout << "a " << ans.size();
            for (const auto& pt : ans) cout << ' ' << pt.r << ' ' << pt.c;
            cout << endl;
            int accepted;
            cin >> accepted;
            return 0;
        }

        // Sync particle div_sums with new queries if any
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            while (particles[i].div_sums.size() < div_queries.size()) {
                int q = particles[i].div_sums.size();
                int sv = 0;
                for (int s = 0; s < m; ++s) {
                    sv += div_queries[q].shape_pl_overlap[s][particles[i].p[s]];
                }
                particles[i].div_sums.push_back(sv);
                if (sv >= 0 && sv < (int)div_queries[q].log_lik.size()) {
                    particles[i].div_log_lik += div_queries[q].log_lik[sv];
                } else {
                    particles[i].div_log_lik += -50.0;
                }
            }
        }

        // Run fast MCMC on all particles
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            // Repair invalid placements after zero pruning
            for (int s = 0; s < m; ++s) {
                bool is_valid = false;
                for (int vp : valid_placements[s]) {
                    if (particles[i].p[s] == vp) { is_valid = true; break; }
                }
                if (!is_valid && !valid_placements[s].empty()) {
                    int old_p = particles[i].p[s];
                    int r_idx = uniform_int_distribution<int>(0, (int)valid_placements[s].size() - 1)(rng);
                    int new_p = valid_placements[s][r_idx];
                    for (int idx : all_placements[s][old_p].cell_indices) particles[i].v[idx]--;
                    for (int idx : all_placements[s][new_p].cell_indices) particles[i].v[idx]++;
                    for (size_t q = 0; q < div_queries.size(); ++q) {
                        int d_ov = div_queries[q].shape_pl_overlap[s][new_p] - div_queries[q].shape_pl_overlap[s][old_p];
                        particles[i].div_sums[q] += d_ov;
                    }
                    particles[i].p[s] = new_p;
                }
            }

            // Recalculate violations
            particles[i].drill_violations = 0;
            for (int idx : drilled_cells_list) {
                particles[i].drill_violations += abs(particles[i].v[idx] - drill_res[idx]);
            }

            run_mcmc(particles[i], 150);
        }

        // Sort particles by violations
        vector<int> sorted_indices(NUM_PARTICLES);
        iota(sorted_indices.begin(), sorted_indices.end(), 0);
        sort(sorted_indices.begin(), sorted_indices.end(), [&](int a, int b) {
            return particles[a].drill_violations < particles[b].drill_violations;
        });

        int min_violations = particles[sorted_indices[0]].drill_violations;
        vector<int> best_indices;
        for (int idx : sorted_indices) {
            if (particles[idx].drill_violations == min_violations) {
                best_indices.push_back(idx);
            }
        }

        int num_best = best_indices.size();
        vector<double> prob_oil(total_cells, 0.0);

        if (min_violations == 0 && num_best >= 10) {
            for (int idx : best_indices) {
                const auto& st = particles[idx];
                for (int c = 0; c < total_cells; ++c) {
                    if (st.v[c] > 0) prob_oil[c] += 1.0 / num_best;
                }
            }

            // Check if all best particles agree on the oil mask
            bool all_determined = true;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (prob_oil[idx] > 0.03 && prob_oil[idx] < 0.97) {
                    all_determined = false;
                    break;
                }
            }

            if (all_determined) {
                vector<Point> ans;
                for (int idx = 0; idx < total_cells; ++idx) {
                    if (prob_oil[idx] >= 0.5) {
                        ans.push_back({idx / n, idx % n});
                    }
                }
                cout << "a " << ans.size();
                for (const auto& pt : ans) cout << ' ' << pt.r << ' ' << pt.c;
                cout << endl;

                int accepted;
                cin >> accepted;
                if (accepted == 1) return 0;
            }
        }

        // Information gain query selection using best particles
        auto cands = generate_candidate_queries();
        int best_cand_idx = -1;
        double best_gain_score = -1.0;

        if (num_best >= 5) {
            for (int c_idx = 0; c_idx < (int)cands.size(); ++c_idx) {
                const auto& cand = cands[c_idx];
                int d = cand.cells.size();
                double cost = (cand.is_drill ? 1.0 : 1.0 / sqrt(d));

                double mean_v = 0.0;
                for (int p_idx : best_indices) {
                    int sv = 0;
                    for (int idx : cand.cells) sv += particles[p_idx].v[idx];
                    mean_v += sv;
                }
                mean_v /= num_best;

                double var_v = 0.0;
                for (int p_idx : best_indices) {
                    int sv = 0;
                    for (int idx : cand.cells) sv += particles[p_idx].v[idx];
                    var_v += (sv - mean_v) * (sv - mean_v);
                }
                var_v /= num_best;

                if (var_v < 1e-4) continue;

                double gain_score = 0.0;
                if (cand.is_drill) {
                    gain_score = var_v / cost;
                } else {
                    double noise_var = (d * epsilon * (1.0 - epsilon)) / ((1.0 - 2.0 * epsilon) * (1.0 - 2.0 * epsilon) + 1e-6);
                    gain_score = (var_v / (var_v + noise_var + 0.1)) / cost;
                }

                if (gain_score > best_gain_score) {
                    best_gain_score = gain_score;
                    best_cand_idx = c_idx;
                }
            }
        }

        // Fallback: space-filling drill
        if (best_cand_idx == -1 || best_gain_score < 1e-3) {
            double best_dist = -1.0;
            int best_cell = -1;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] != -1) continue;
                int r = idx / n;
                int c = idx % n;
                if (drilled_cells_list.empty()) {
                    double d_center = abs(r - n / 2) + abs(c - n / 2);
                    double score = 100.0 - d_center;
                    if (score > best_dist) {
                        best_dist = score;
                        best_cell = idx;
                    }
                } else {
                    int min_dist = 100000;
                    for (int d_idx : drilled_cells_list) {
                        int dr = abs(r - d_idx / n);
                        int dc = abs(c - d_idx % n);
                        min_dist = min(min_dist, dr * dr + dc * dc);
                    }
                    if (min_dist > best_dist) {
                        best_dist = min_dist;
                        best_cell = idx;
                    }
                }
            }
            if (best_cell == -1) {
                vector<Point> ans;
                for (int idx = 0; idx < total_cells; ++idx) {
                    if (drill_res[idx] > 0) ans.push_back({idx / n, idx % n});
                }
                cout << "a " << ans.size();
                for (const auto& pt : ans) cout << ' ' << pt.r << ' ' << pt.c;
                cout << endl;
                int accepted;
                cin >> accepted;
                return 0;
            }
            do_drill(best_cell / n, best_cell % n);
        } else {
            const auto& cand = cands[best_cand_idx];
            if (cand.is_drill) {
                do_drill(cand.cell_idx / n, cand.cell_idx % n);
            } else {
                do_divine(cand.cells);
            }
        }
    }

    return 0;
}
