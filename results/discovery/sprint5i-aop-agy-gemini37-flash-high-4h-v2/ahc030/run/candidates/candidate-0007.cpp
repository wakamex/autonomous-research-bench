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

    // Valid placements per shape
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

    mt19937 rng(42);

    // Exact Backtracking Solver with Upper and Lower Bound Pruning
    struct ExactConfig {
        vector<int> p;
    };

    vector<ExactConfig> exact_solutions;
    const int MAX_EXACT_SOLUTIONS = 2000;

    auto find_exact_solutions = [&]() -> bool {
        exact_solutions.clear();

        // Sort shapes by number of valid placements
        vector<int> shape_order(m);
        iota(shape_order.begin(), shape_order.end(), 0);
        sort(shape_order.begin(), shape_order.end(), [&](int a, int b) {
            return valid_placements[a].size() < valid_placements[b].size();
        });

        // Compute max remaining oil per drilled cell
        // rem_max[depth][cell_idx]
        vector<vector<int>> rem_max(m + 1, vector<int>(total_cells, 0));
        for (int d = m - 1; d >= 0; --d) {
            int s = shape_order[d];
            rem_max[d] = rem_max[d + 1];
            for (int p_idx : valid_placements[s]) {
                for (int c_idx : all_placements[s][p_idx].cell_indices) {
                    // Each shape can add at most 1 to a cell
                }
            }
            // For shape s, for any cell c, it can add at most 1
            vector<bool> can_cover(total_cells, false);
            for (int p_idx : valid_placements[s]) {
                for (int c_idx : all_placements[s][p_idx].cell_indices) {
                    can_cover[c_idx] = true;
                }
            }
            for (int c = 0; c < total_cells; ++c) {
                if (can_cover[c]) rem_max[d][c] += 1;
            }
        }

        vector<int> cur_p(m, -1);
        vector<int> cur_v(total_cells, 0);
        int64_t node_count = 0;

        auto dfs = [&](auto& self, int depth) -> void {
            if (exact_solutions.size() >= MAX_EXACT_SOLUTIONS) return;
            if (++node_count > 100000) return; // limit search effort

            if (depth == m) {
                // Verify all drilled cells
                bool ok = true;
                for (int u : drilled_cells_list) {
                    if (cur_v[u] != drill_res[u]) { ok = false; break; }
                }
                if (ok) {
                    ExactConfig cfg;
                    cfg.p.resize(m);
                    for (int d = 0; d < m; ++d) cfg.p[shape_order[d]] = cur_p[d];
                    exact_solutions.push_back(cfg);
                }
                return;
            }

            int s = shape_order[depth];
            for (int p_idx : valid_placements[s]) {
                const auto& pl = all_placements[s][p_idx];

                // Pruning check: upper bound
                bool prune = false;
                for (int u : pl.cell_indices) {
                    if (drill_res[u] != -1 && cur_v[u] + 1 > drill_res[u]) {
                        prune = true;
                        break;
                    }
                }
                if (prune) continue;

                // Apply placement
                for (int u : pl.cell_indices) cur_v[u]++;
                cur_p[depth] = p_idx;

                // Lower bound check on drilled cells
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

                // Revert
                for (int u : pl.cell_indices) cur_v[u]--;
                if (exact_solutions.size() >= MAX_EXACT_SOLUTIONS) return;
            }
        };

        dfs(dfs, 0);
        return !exact_solutions.empty() && node_count <= 100000;
    };

    // Main solver loop
    while (true) {
        if (get_elapsed_ms() > 2200) {
            // Time limit safety: answer with known drilled positive cells
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

        // Try exact branch and bound
        find_exact_solutions();

        if (!exact_solutions.empty()) {
            // Calculate oil probability for each cell across exact solutions
            vector<double> prob_oil(total_cells, 0.0);
            int n_sol = exact_solutions.size();

            for (const auto& cfg : exact_solutions) {
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

            // Check if all exact solutions agree on the oil mask
            bool all_determined = true;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (prob_oil[idx] > 1e-4 && prob_oil[idx] < 1.0 - 1e-4) {
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

            // Pick the undrilled cell that maximizes information entropy: p * (1 - p)
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

        // If exact search didn't find solutions or all undrilled cells have p=0:
        // Use space-filling probe with priority near positive hits
        int best_cell = -1;
        double best_score = -1e9;

        // Check if there are undrilled neighbors of positive drilled cells
        vector<int> positive_drills;
        for (int u : drilled_cells_list) {
            if (drill_res[u] > 0) positive_drills.push_back(u);
        }

        for (int idx = 0; idx < total_cells; ++idx) {
            if (drill_res[idx] != -1) continue;
            int r = idx / n;
            int c = idx % n;

            double score = 0.0;
            if (!positive_drills.empty()) {
                // Favor cells close to positive drills
                int min_pos_dist = 100000;
                for (int u : positive_drills) {
                    int dr = abs(r - u / n);
                    int dc = abs(c - u % n);
                    min_pos_dist = min(min_pos_dist, dr + dc);
                }
                // Score higher for distance 1 or 2 to a positive hit
                if (min_pos_dist == 1) score += 500.0;
                else if (min_pos_dist == 2) score += 200.0;
                else score -= min_pos_dist * 2.0;
            } else {
                // No positive hits yet: space-filling probe
                int min_dist = 100000;
                for (int u : drilled_cells_list) {
                    int dr = abs(r - u / n);
                    int dc = abs(c - u % n);
                    min_dist = min(min_dist, dr * dr + dc * dc);
                }
                score = (drilled_cells_list.empty() ? -(abs(r - n / 2) + abs(c - n / 2)) : min_dist);
            }

            if (score > best_score) {
                best_score = score;
                best_cell = idx;
            }
        }

        if (best_cell == -1) {
            // All drilled, submit answer
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
    }

    return 0;
}
