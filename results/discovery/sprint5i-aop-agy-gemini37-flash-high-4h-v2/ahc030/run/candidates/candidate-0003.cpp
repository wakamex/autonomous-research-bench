#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <string>

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
    vector<int> p; // placement index for each shape
    vector<int> v; // sum at each cell (size N*N)
    int drill_violations;
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m;
    double epsilon;
    if (!(cin >> n >> m >> epsilon)) return 0;

    int total_cells = n * n;

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

            bool accept = false;
            if (delta_violations <= 0) {
                accept = true;
            } else {
                // Soft acceptance with low probability to escape local minima
                double p_acc = exp(-delta_violations * 2.0);
                if (real_dist(rng) < p_acc) accept = true;
            }

            if (accept) {
                st.p[shape] = new_p;
                st.drill_violations += delta_violations;
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

    const int NUM_PARTICLES = 80;
    vector<State> particles(NUM_PARTICLES);
    for (int i = 0; i < NUM_PARTICLES; ++i) {
        particles[i] = init_random_state();
    }

    // Adaptive Active Learning Loop
    while (true) {
        // Run MCMC on all particles
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            // If particle violates valid placements after zero prune, repair it
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
            // Recalculate violations
            particles[i].drill_violations = 0;
            for (int idx : drilled_cells_list) {
                particles[i].drill_violations += abs(particles[i].v[idx] - drill_res[idx]);
            }

            run_mcmc(particles[i], 400);
        }

        // Filter particles with zero violations
        vector<int> valid_indices;
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            if (particles[i].drill_violations == 0) {
                valid_indices.push_back(i);
            }
        }

        // If too few valid particles, reseed invalid ones and burn in
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

        // Calculate marginal oil probability for each cell
        vector<double> prob_oil(total_cells, 0.0);
        int num_valid = valid_indices.size();

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
                for (const auto& pt : ans) {
                    cout << ' ' << pt.r << ' ' << pt.c;
                }
                cout << endl;

                int accepted;
                cin >> accepted;
                if (accepted == 1) {
                    return 0;
                }
            }
        }

        // Choose next cell to drill
        int best_cell = -1;
        double best_uncertainty = -1.0;

        if (num_valid >= 10) {
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] != -1) continue;
                double p = prob_oil[idx];
                if (p > 0.05 && p < 0.95) {
                    double uncertainty = 1.0 - 2.0 * abs(p - 0.5);
                    if (uncertainty > best_uncertainty) {
                        best_uncertainty = uncertainty;
                        best_cell = idx;
                    }
                }
            }
        }

        // If no uncertain cell among valid particles (e.g. initial exploration or unprobed regions):
        // Use space-filling distance to already drilled cells
        if (best_cell == -1) {
            double best_dist = -1.0;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] != -1) continue;
                int r = idx / n;
                int c = idx % n;

                if (drilled_cells_list.empty()) {
                    // Start near center
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
        }

        if (best_cell == -1) {
            // All cells drilled
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
