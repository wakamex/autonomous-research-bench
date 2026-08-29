#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <string>
#include <numeric>

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

struct State {
    vector<int> p;
    vector<int> v;
    int drill_violations;
};

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
    int total_poly_area = 0;
    for (int i = 0; i < m; ++i) {
        polyominoes[i].id = i;
        cin >> polyominoes[i].area;
        total_poly_area += polyominoes[i].area;
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

    // Identify identical shapes for symmetry breaking
    vector<int> shape_group(m, -1);
    int group_count = 0;
    for (int i = 0; i < m; ++i) {
        if (shape_group[i] != -1) continue;
        shape_group[i] = group_count;
        for (int j = i + 1; j < m; ++j) {
            if (polyominoes[i].area == polyominoes[j].area &&
                polyominoes[i].h == polyominoes[j].h &&
                polyominoes[i].w == polyominoes[j].w) {
                bool same = true;
                for (size_t k = 0; k < polyominoes[i].cells.size(); ++k) {
                    if (polyominoes[i].cells[k].r != polyominoes[j].cells[k].r ||
                        polyominoes[i].cells[k].c != polyominoes[j].cells[k].c) {
                        same = false;
                        break;
                    }
                }
                if (same) shape_group[j] = group_count;
            }
        }
        group_count++;
    }

    // Precompute placements for each polyomino
    vector<vector<Placement>> all_placements(m);
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
    int total_oil_found = 0;

    // Valid placements per shape
    vector<vector<int>> valid_placements(m);
    for (int i = 0; i < m; ++i) {
        for (int p_idx = 0; p_idx < (int)all_placements[i].size(); ++p_idx) {
            valid_placements[i].push_back(p_idx);
        }
    }

    // Full Arc Consistency Engine (FAC)
    auto propagate_constraints = [&]() {
        bool changed = true;
        while (changed) {
            changed = false;

            vector<vector<bool>> is_core(m, vector<bool>(total_cells, false));
            vector<vector<bool>> can_cover(m, vector<bool>(total_cells, false));
            vector<int> total_cores(total_cells, 0);
            vector<int> total_can_cover(total_cells, 0);

            for (int s = 0; s < m; ++s) {
                if (valid_placements[s].empty()) continue;
                vector<int> cover_cnt(total_cells, 0);
                int num_vp = valid_placements[s].size();
                for (int p_idx : valid_placements[s]) {
                    for (int idx : all_placements[s][p_idx].cell_indices) {
                        cover_cnt[idx]++;
                    }
                }
                for (int c = 0; c < total_cells; ++c) {
                    if (cover_cnt[c] == num_vp) {
                        is_core[s][c] = true;
                        total_cores[c]++;
                    }
                    if (cover_cnt[c] > 0) {
                        can_cover[s][c] = true;
                        total_can_cover[c]++;
                    }
                }
            }

            // Per-placement validation against drilled cell bounds
            for (int s = 0; s < m; ++s) {
                if (valid_placements[s].size() <= 1) continue;

                vector<int> next_valid;
                for (int p_idx : valid_placements[s]) {
                    const auto& pl = all_placements[s][p_idx];
                    bool invalid = false;

                    // 1. Upper bound check: other cores + this placement cannot exceed target
                    for (int u : pl.cell_indices) {
                        int target = drill_res[u];
                        if (target != -1) {
                            int other_cores = total_cores[u] - (is_core[s][u] ? 1 : 0);
                            if (other_cores + 1 > target) {
                                invalid = true;
                                break;
                            }
                        }
                    }
                    if (invalid) continue;

                    // 2. Lower bound check: other max + this placement must be able to reach target
                    for (int u : drilled_cells_list) {
                        int target = drill_res[u];
                        if (target > 0) {
                            int this_in = ((pl.mask[u >> 6] >> (u & 63)) & 1ULL);
                            int other_max = total_can_cover[u] - (can_cover[s][u] ? 1 : 0);
                            if (other_max + this_in < target) {
                                invalid = true;
                                break;
                            }
                        }
                    }
                    if (invalid) continue;

                    next_valid.push_back(p_idx);
                }

                if (next_valid.size() < valid_placements[s].size()) {
                    valid_placements[s] = move(next_valid);
                    changed = true;
                }
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
        total_oil_found += amt;

        if (amt == 0) {
            for (int i = 0; i < m; ++i) {
                vector<int> next_valid;
                for (int p_idx : valid_placements[i]) {
                    const auto& pl = all_placements[i][p_idx];
                    if (((pl.mask[idx >> 6] >> (idx & 63)) & 1ULL) == 0) {
                        next_valid.push_back(p_idx);
                    }
                }
                valid_placements[i] = move(next_valid);
            }
        }

        propagate_constraints();
        return amt;
    };

    auto submit_answer = [&](const vector<int>& oil_cells) -> bool {
        vector<bool> in_ans(total_cells, false);
        for (int idx : oil_cells) in_ans[idx] = true;
        for (int u : drilled_cells_list) {
            if (drill_res[u] > 0) in_ans[u] = true;
        }

        vector<Point> ans;
        for (int idx = 0; idx < total_cells; ++idx) {
            if (in_ans[idx]) ans.push_back({idx / n, idx % n});
        }

        cout << "a " << ans.size();
        for (const auto& pt : ans) cout << ' ' << pt.r << ' ' << pt.c;
        cout << endl;

        int accepted;
        if (!(cin >> accepted)) return true;
        return (accepted == 1);
    };

    mt19937 rng(42);

    // Exact Backtracking with Symmetry Breaking
    struct ExactConfig {
        vector<int> p;
    };

    auto try_exact_dfs = [&]() -> pair<bool, vector<ExactConfig>> {
        int64_t total_configs = 1;
        for (int s = 0; s < m; ++s) {
            total_configs = total_configs * valid_placements[s].size();
            if (total_configs > 800000) return {false, {}};
        }

        vector<int> shape_order(m);
        iota(shape_order.begin(), shape_order.end(), 0);
        sort(shape_order.begin(), shape_order.end(), [&](int a, int b) {
            if (shape_group[a] != shape_group[b]) {
                return valid_placements[a].size() < valid_placements[b].size();
            }
            return a < b;
        });

        vector<vector<int>> rem_max(m + 1, vector<int>(total_cells, 0));
        for (int d = m - 1; d >= 0; --d) {
            int s = shape_order[d];
            rem_max[d] = rem_max[d + 1];
            vector<bool> can_cover(total_cells, false);
            for (int p_idx : valid_placements[s]) {
                for (int c_idx : all_placements[s][p_idx].cell_indices) can_cover[c_idx] = true;
            }
            for (int c = 0; c < total_cells; ++c) {
                if (can_cover[c]) rem_max[d][c] += 1;
            }
        }

        vector<ExactConfig> solutions;
        vector<int> cur_p(m, -1);
        vector<int> cur_v(total_cells, 0);

        auto dfs = [&](auto& self, int depth) -> void {
            if (depth == m) {
                for (int u : drilled_cells_list) {
                    if (cur_v[u] != drill_res[u]) return;
                }
                ExactConfig cfg;
                cfg.p.resize(m);
                for (int d = 0; d < m; ++d) cfg.p[shape_order[d]] = cur_p[d];
                solutions.push_back(cfg);
                return;
            }

            int s = shape_order[depth];
            int prev_p_limit = -1;
            if (depth > 0 && shape_group[s] == shape_group[shape_order[depth - 1]]) {
                prev_p_limit = cur_p[depth - 1];
            }

            for (int p_idx : valid_placements[s]) {
                if (p_idx < prev_p_limit) continue;

                const auto& pl = all_placements[s][p_idx];

                bool prune = false;
                for (int u : pl.cell_indices) {
                    if (drill_res[u] != -1 && cur_v[u] + 1 > drill_res[u]) {
                        prune = true;
                        break;
                    }
                }
                if (prune) continue;

                for (int u : pl.cell_indices) cur_v[u]++;
                cur_p[depth] = p_idx;

                bool feasible = true;
                for (int u : drilled_cells_list) {
                    if (cur_v[u] + rem_max[depth + 1][u] < drill_res[u]) {
                        feasible = false;
                        break;
                    }
                }

                if (feasible) {
                    self(self, depth + 1);
                }

                for (int u : pl.cell_indices) cur_v[u]--;
            }
        };

        dfs(dfs, 0);
        return {true, solutions};
    };

    // MCMC Setup
    const int NUM_PARTICLES = 50;
    vector<State> particles(NUM_PARTICLES);

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

        return st;
    };

    for (int i = 0; i < NUM_PARTICLES; ++i) {
        particles[i] = init_random_state();
    }

    auto run_mcmc = [&](State& st, int steps) {
        uniform_int_distribution<int> shape_dist(0, m - 1);
        uniform_real_distribution<double> real_dist(0.0, 1.0);

        for (int step = 0; step < steps; ++step) {
            int shape = -1;
            int new_p = -1;

            bool do_targeted = (st.drill_violations > 0 && !drilled_cells_list.empty() && (real_dist(rng) < 0.6));
            if (do_targeted) {
                int u = drilled_cells_list[uniform_int_distribution<int>(0, (int)drilled_cells_list.size() - 1)(rng)];
                int diff = st.v[u] - drill_res[u];
                if (diff < 0) {
                    shape = shape_dist(rng);
                    const auto& c_pls = cell_placements[shape][u];
                    if (!c_pls.empty()) {
                        new_p = c_pls[uniform_int_distribution<int>(0, (int)c_pls.size() - 1)(rng)];
                    }
                } else if (diff > 0) {
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

            bool accept = false;
            int new_viol = st.drill_violations + delta_violations;
            if (new_viol <= st.drill_violations) {
                accept = true;
            } else {
                double p_acc = exp(-delta_violations * 4.0);
                if (real_dist(rng) < p_acc) accept = true;
            }

            if (accept) {
                st.p[shape] = new_p;
                st.drill_violations = new_viol;
            } else {
                for (int idx : new_pl.cell_indices) st.v[idx]--;
                for (int idx : old_pl.cell_indices) st.v[idx]++;
            }
        }
    };

    // Main Active Learning Loop
    while (true) {
        // Global conservation check: all oil accounted for
        if (total_oil_found == total_poly_area) {
            vector<int> oil;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] > 0) oil.push_back(idx);
            }
            if (submit_answer(oil)) return 0;
        }

        // Time safety check
        if (get_elapsed_ms() > 2200) {
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] == -1) do_drill(idx / n, idx % n);
            }
            vector<int> oil;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] > 0) oil.push_back(idx);
            }
            submit_answer(oil);
            return 0;
        }

        // 1. Try Exact DFS
        auto [is_exact, exact_sols] = try_exact_dfs();
        if (is_exact && !exact_sols.empty()) {
            vector<double> prob_oil(total_cells, 0.0);
            int n_sol = exact_sols.size();

            for (const auto& cfg : exact_sols) {
                vector<int> mask(total_cells, 0);
                for (int s = 0; s < m; ++s) {
                    for (int idx : all_placements[s][cfg.p[s]].cell_indices) {
                        mask[idx] = 1;
                    }
                }
                for (int c = 0; c < total_cells; ++c) {
                    if (mask[c]) prob_oil[c] += 1.0 / n_sol;
                }
            }

            bool all_determined = true;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (prob_oil[idx] > 1e-4 && prob_oil[idx] < 1.0 - 1e-4) {
                    all_determined = false;
                    break;
                }
            }

            if (all_determined) {
                vector<int> ans_cells;
                for (int idx = 0; idx < total_cells; ++idx) {
                    if (prob_oil[idx] >= 0.5) ans_cells.push_back(idx);
                }
                if (submit_answer(ans_cells)) return 0;
            }

            // Information-optimal split among exact solutions
            int best_cell = -1;
            double best_score = -1.0;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] != -1) continue;
                double p = prob_oil[idx];
                double score = p * (1.0 - p);
                if (score > best_score) {
                    best_score = score;
                    best_cell = idx;
                }
            }

            if (best_cell != -1 && best_score > 1e-4) {
                do_drill(best_cell / n, best_cell % n);
                continue;
            }
        }

        // 2. MCMC sampling for larger state spaces
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

            particles[i].drill_violations = 0;
            for (int idx : drilled_cells_list) {
                particles[i].drill_violations += abs(particles[i].v[idx] - drill_res[idx]);
            }
            run_mcmc(particles[i], 300);
        }

        vector<int> valid_indices;
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            if (particles[i].drill_violations == 0) valid_indices.push_back(i);
        }

        if (valid_indices.size() < 15) {
            for (int i = 0; i < NUM_PARTICLES; ++i) {
                if (particles[i].drill_violations > 0) {
                    particles[i] = init_random_state();
                    run_mcmc(particles[i], 800);
                    if (particles[i].drill_violations == 0) valid_indices.push_back(i);
                }
            }
        }

        int num_valid = valid_indices.size();
        vector<double> prob_oil(total_cells, 0.0);

        if (num_valid >= 15) {
            for (int idx : valid_indices) {
                const auto& st = particles[idx];
                for (int c = 0; c < total_cells; ++c) {
                    if (st.v[c] > 0) prob_oil[c] += 1.0 / num_valid;
                }
            }

            bool all_determined = true;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (prob_oil[idx] > 0.02 && prob_oil[idx] < 0.98) {
                    all_determined = false;
                    break;
                }
            }

            if (all_determined) {
                vector<int> ans_cells;
                for (int idx = 0; idx < total_cells; ++idx) {
                    if (prob_oil[idx] >= 0.5) ans_cells.push_back(idx);
                }
                if (submit_answer(ans_cells)) return 0;
            }

            int best_cell = -1;
            double best_score = -1.0;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] != -1) continue;
                double p = prob_oil[idx];
                if (p > 0.05 && p < 0.95) {
                    double score = p * (1.0 - p);
                    if (score > best_score) {
                        best_score = score;
                        best_cell = idx;
                    }
                }
            }

            if (best_cell != -1 && best_score > 1e-4) {
                do_drill(best_cell / n, best_cell % n);
                continue;
            }
        }

        // 3. Strategic exploration using Expected Coverage Prior with Staggered Repulsion
        int best_cell = -1;
        double best_score = -1e9;

        bool has_positive_drill = false;
        for (int u : drilled_cells_list) {
            if (drill_res[u] > 0) { has_positive_drill = true; break; }
        }

        vector<double> expected_coverage(total_cells, 0.0);
        for (int s = 0; s < m; ++s) {
            int num_vp = valid_placements[s].size();
            if (num_vp <= 1) continue;
            double weight = 1.0 / num_vp;
            for (int p_idx : valid_placements[s]) {
                for (int idx : all_placements[s][p_idx].cell_indices) {
                    expected_coverage[idx] += weight;
                }
            }
        }

        vector<bool> is_pos_adj(total_cells, false);
        for (int u : drilled_cells_list) {
            if (drill_res[u] > 0) {
                int ur = u / n, uc = u % n;
                if (ur > 0) is_pos_adj[(ur - 1) * n + uc] = true;
                if (ur + 1 < n) is_pos_adj[(ur + 1) * n + uc] = true;
                if (uc > 0) is_pos_adj[ur * n + (uc - 1)] = true;
                if (uc + 1 < n) is_pos_adj[ur * n + (uc + 1)] = true;
            }
        }

        for (int idx = 0; idx < total_cells; ++idx) {
            if (drill_res[idx] != -1) continue;
            double score = expected_coverage[idx];
            if (is_pos_adj[idx]) {
                score += 0.50;
            } else if (!has_positive_drill) {
                int min_zero_dist = 1000;
                int r = idx / n, c = idx % n;
                for (int u : drilled_cells_list) {
                    int dr = abs(r - u / n);
                    int dc = abs(c - u % n);
                    min_zero_dist = min(min_zero_dist, dr + dc);
                }
                if (min_zero_dist == 1) score *= 0.35;
            }

            if (score > best_score) {
                best_score = score;
                best_cell = idx;
            }
        }

        if (best_cell == -1 || best_score <= 1e-4) {
            double best_dist = -1.0;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] != -1) continue;
                int r = idx / n;
                int c = idx % n;
                int min_dist = 100000;
                for (int u : drilled_cells_list) {
                    int dr = abs(r - u / n);
                    int dc = abs(c - u % n);
                    min_dist = min(min_dist, dr * dr + dc * dc);
                }
                double score = (drilled_cells_list.empty() ? -(abs(r - n / 2) + abs(c - n / 2)) : min_dist);
                if (score > best_dist) {
                    best_dist = score;
                    best_cell = idx;
                }
            }
        }

        if (best_cell == -1) {
            vector<int> ans_cells;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] > 0) ans_cells.push_back(idx);
            }
            if (submit_answer(ans_cells)) return 0;
            break;
        }

        do_drill(best_cell / n, best_cell % n);
    }

    return 0;
}
