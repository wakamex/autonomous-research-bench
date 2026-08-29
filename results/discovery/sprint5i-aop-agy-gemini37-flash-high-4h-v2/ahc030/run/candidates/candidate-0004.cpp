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
    int d;
    int y;
    vector<double> log_lik; // log_lik[v]
};

struct State {
    vector<int> p; // placement index for each shape
    vector<int> v; // sum at each cell (size N*N)
    int drill_violations;
    double div_log_lik;
};

static inline double normal_cdf(double x) {
    return 0.5 * std::erfc(-x * 0.70710678118654752440);
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m;
    double epsilon;
    if (!(cin >> n >> m >> epsilon)) return 0;

    int total_cells = n * n;

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
    for (int i = 0; i < m; ++i) {
        int max_top_r = n - polyominoes[i].h;
        int max_top_c = n - polyominoes[i].w;
        for (int r = 0; r <= max_top_r; ++r) {
            for (int c = 0; c <= max_top_c; ++c) {
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

    auto compute_single_div_log_lik = [&](const DivQuery& dq, int sum_v) -> double {
        if (sum_v >= 0 && sum_v < (int)dq.log_lik.size()) return dq.log_lik[sum_v];
        return -50.0;
    };

    auto compute_full_div_log_lik = [&](const vector<int>& grid_v) -> double {
        double total = 0.0;
        for (const auto& dq : div_queries) {
            int sv = 0;
            for (int idx : dq.cells) sv += grid_v[idx];
            total += compute_single_div_log_lik(dq, sv);
        }
        return total;
    };

    auto init_random_state = [&]() -> State {
        State st;
        st.p.resize(m);
        st.v.assign(total_cells, 0);
        st.drill_violations = 0;

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

        st.div_log_lik = compute_full_div_log_lik(st.v);
        return st;
    };

    auto run_mcmc = [&](State& st, int steps) {
        uniform_int_distribution<int> shape_dist(0, m - 1);
        uniform_real_distribution<double> real_dist(0.0, 1.0);

        for (int step = 0; step < steps; ++step) {
            int shape = shape_dist(rng);
            if (valid_placements[shape].size() <= 1) continue;

            int old_p = st.p[shape];
            int r_idx = uniform_int_distribution<int>(0, (int)valid_placements[shape].size() - 1)(rng);
            int new_p = valid_placements[shape][r_idx];
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

            // Compute delta div log-likelihood
            double new_div_log_lik = st.div_log_lik;
            if (!div_queries.empty()) {
                for (const auto& dq : div_queries) {
                    int old_sv = 0, new_sv = 0;
                    // Check if dq intersects old_pl or new_pl
                    bool affects = false;
                    for (int idx : dq.cells) {
                        // Quick check
                        if (((old_pl.mask[idx >> 6] >> (idx & 63)) & 1ULL) ||
                            ((new_pl.mask[idx >> 6] >> (idx & 63)) & 1ULL)) {
                            affects = true;
                            break;
                        }
                    }
                    if (affects) {
                        for (int idx : dq.cells) {
                            new_sv += st.v[idx];
                            // old_sv is new_sv - (idx in new_pl) + (idx in old_pl)
                            int in_new = (new_pl.mask[idx >> 6] >> (idx & 63)) & 1ULL;
                            int in_old = (old_pl.mask[idx >> 6] >> (idx & 63)) & 1ULL;
                            old_sv += st.v[idx] - in_new + in_old;
                        }
                        new_div_log_lik += compute_single_div_log_lik(dq, new_sv) - compute_single_div_log_lik(dq, old_sv);
                    }
                }
            }
            double delta_log_lik = new_div_log_lik - st.div_log_lik;

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
                // Violations increased
                double p_acc = exp(-delta_violations * 3.0 + delta_log_lik);
                if (real_dist(rng) < p_acc) accept = true;
            }

            if (accept) {
                st.p[shape] = new_p;
                st.drill_violations = new_viol;
                st.div_log_lik = new_div_log_lik;
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

        div_queries.push_back(dq);
        queried_div_sets.insert(cells);
        return y;
    };

    const int NUM_PARTICLES = 80;
    vector<State> particles(NUM_PARTICLES);
    for (int i = 0; i < NUM_PARTICLES; ++i) {
        particles[i] = init_random_state();
    }

    // Candidate query templates
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
        // 2. Block divinations (2x2, 3x3, 4x4, 5x5)
        for (int bs : {2, 3, 4, 5}) {
            if (bs > n) continue;
            for (int r = 0; r <= n - bs; r += max(1, bs - 1)) {
                for (int c = 0; c <= n - bs; c += max(1, bs - 1)) {
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
        // 3. Rows and Columns
        for (int r = 0; r < n; ++r) {
            vector<int> row;
            for (int c = 0; c < n; ++c) row.push_back(r * n + c);
            if (queried_div_sets.find(row) == queried_div_sets.end()) {
                cands.push_back({false, -1, row});
            }
        }
        for (int c = 0; c < n; ++c) {
            vector<int> col;
            for (int r = 0; r < n; ++r) col.push_back(r * n + c);
            if (queried_div_sets.find(col) == queried_div_sets.end()) {
                cands.push_back({false, -1, col});
            }
        }
        // 4. Quadrants
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
        // Run MCMC on all particles
        for (int i = 0; i < NUM_PARTICLES; ++i) {
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
                    particles[i].p[s] = new_p;
                }
            }
            // Recalculate violations and div log-lik
            particles[i].drill_violations = 0;
            for (int idx : drilled_cells_list) {
                particles[i].drill_violations += abs(particles[i].v[idx] - drill_res[idx]);
            }
            particles[i].div_log_lik = compute_full_div_log_lik(particles[i].v);

            run_mcmc(particles[i], 400);
        }

        // Filter valid particles (zero drill violations)
        vector<int> valid_indices;
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            if (particles[i].drill_violations == 0) {
                valid_indices.push_back(i);
            }
        }

        // Reseed if too few valid particles
        if (valid_indices.size() < 25) {
            for (int i = 0; i < NUM_PARTICLES; ++i) {
                if (particles[i].drill_violations > 0) {
                    particles[i] = init_random_state();
                    run_mcmc(particles[i], 1200);
                    if (particles[i].drill_violations == 0) {
                        valid_indices.push_back(i);
                    }
                }
            }
        }

        int num_valid = valid_indices.size();
        vector<double> prob_oil(total_cells, 0.0);

        if (num_valid >= 20) {
            for (int idx : valid_indices) {
                const auto& st = particles[idx];
                for (int c = 0; c < total_cells; ++c) {
                    if (st.v[c] > 0) prob_oil[c] += 1.0 / num_valid;
                }
            }

            // Check if all particles agree on the oil mask
            bool all_determined = true;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (prob_oil[idx] > 0.02 && prob_oil[idx] < 0.98) {
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

        // Information gain query selection
        auto cands = generate_candidate_queries();
        int best_cand_idx = -1;
        double best_gain_score = -1.0;

        if (num_valid >= 10) {
            for (int c_idx = 0; c_idx < (int)cands.size(); ++c_idx) {
                const auto& cand = cands[c_idx];
                int d = cand.cells.size();
                double cost = (cand.is_drill ? 1.0 : 1.0 / sqrt(d));

                // Calculate variance of sum across valid particles
                double mean_v = 0.0;
                for (int p_idx : valid_indices) {
                    int sv = 0;
                    for (int idx : cand.cells) sv += particles[p_idx].v[idx];
                    mean_v += sv;
                }
                mean_v /= num_valid;

                double var_v = 0.0;
                for (int p_idx : valid_indices) {
                    int sv = 0;
                    for (int idx : cand.cells) sv += particles[p_idx].v[idx];
                    var_v += (sv - mean_v) * (sv - mean_v);
                }
                var_v /= num_valid;

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
                // All drilled, answer
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
