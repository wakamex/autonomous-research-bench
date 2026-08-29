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

struct DivQuery {
    vector<int> cells;
    int d;
    int y;
    double mu_const;
    double mu_coeff;
    double sigma;
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
    vector<DivQuery> div_queries;

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

    mt19937 rng(1337);

    auto compute_div_log_lik = [&](const DivQuery& dq, int sum_v) -> double {
        if (sum_v >= 0 && sum_v < (int)dq.log_lik.size()) return dq.log_lik[sum_v];
        return -50.0;
    };

    auto init_random_state = [&]() -> State {
        State st;
        st.p.resize(m);
        st.v.assign(total_cells, 0);
        st.drill_violations = 0;
        st.div_log_lik = 0.0;

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

        for (const auto& dq : div_queries) {
            int sv = 0;
            for (int idx : dq.cells) sv += st.v[idx];
            st.div_log_lik += compute_div_log_lik(dq, sv);
        }

        return st;
    };

    // Metropolis-Hastings MCMC to sample valid states
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
            // Temporarily apply removal
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
            double new_div_log_lik = 0.0;
            // For now, if we have div queries
            if (!div_queries.empty()) {
                for (const auto& dq : div_queries) {
                    int sv = 0;
                    for (int idx : dq.cells) sv += st.v[idx];
                    new_div_log_lik += compute_div_log_lik(dq, sv);
                }
            }
            double delta_log_lik = new_div_log_lik - st.div_log_lik;

            // Prioritize reducing drill violations (infinitely high weight)
            bool accept = false;
            if (st.drill_violations + delta_violations < st.drill_violations) {
                accept = true;
            } else if (st.drill_violations + delta_violations == st.drill_violations) {
                if (delta_violations == 0 && delta_log_lik >= 0) {
                    accept = true;
                } else if (delta_violations == 0) {
                    double p_acc = exp(delta_log_lik);
                    if (real_dist(rng) < p_acc) accept = true;
                }
            }

            if (accept) {
                st.p[shape] = new_p;
                st.drill_violations += delta_violations;
                st.div_log_lik = new_div_log_lik;
            } else {
                // Revert
                for (int idx : new_pl.cell_indices) st.v[idx]--;
                for (int idx : old_pl.cell_indices) st.v[idx]++;
            }
        }
    };

    // Strategy loop:
    // Initial drills: probe diagonally / spaced grid to find oil fields
    int probe_step = (n >= 15 ? 2 : 2);
    vector<Point> probe_order;
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            if ((r + c) % probe_step == 0) {
                probe_order.push_back({r, c});
            }
        }
    }
    // Shuffle probe order slightly while maintaining spatial coverage
    shuffle(probe_order.begin(), probe_order.end(), rng);

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

    // Probe initial cells
    for (const auto& pt : probe_order) {
        do_drill(pt.r, pt.c);
    }

    // Number of particles
    const int NUM_PARTICLES = 100;
    vector<State> particles(NUM_PARTICLES);
    for (int i = 0; i < NUM_PARTICLES; ++i) {
        particles[i] = init_random_state();
        run_mcmc(particles[i], 2000);
    }

    // Main active learning / drill refinement loop
    while (true) {
        // Run MCMC on all particles
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            run_mcmc(particles[i], 1000);
        }

        // Collect valid particles (0 drill violations)
        vector<int> valid_indices;
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            if (particles[i].drill_violations == 0) {
                valid_indices.push_back(i);
            }
        }

        // If not enough valid particles, reseed and burn in
        if (valid_indices.size() < 20) {
            for (int i = 0; i < NUM_PARTICLES; ++i) {
                if (particles[i].drill_violations > 0) {
                    particles[i] = init_random_state();
                    run_mcmc(particles[i], 3000);
                    if (particles[i].drill_violations == 0) {
                        valid_indices.push_back(i);
                    }
                }
            }
        }

        // Calculate marginal oil probability for each cell
        vector<double> prob_oil(total_cells, 0.0);
        int num_valid = valid_indices.size();
        if (num_valid == 0) {
            // Fallback: drill any unvisited cell
            int best_idx = -1;
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] == -1) {
                    best_idx = idx;
                    break;
                }
            }
            if (best_idx == -1) break;
            do_drill(best_idx / n, best_idx % n);
            continue;
        }

        for (int idx : valid_indices) {
            const auto& st = particles[idx];
            for (int c = 0; c < total_cells; ++c) {
                if (st.v[c] > 0) prob_oil[c] += 1.0 / num_valid;
            }
        }

        // Check if all particles agree on the oil mask
        bool all_determined = true;
        for (int idx = 0; idx < total_cells; ++idx) {
            if (prob_oil[idx] > 1e-4 && prob_oil[idx] < 1.0 - 1e-4) {
                all_determined = false;
                break;
            }
        }

        if (all_determined && num_valid >= 30) {
            // Answer!
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
            // If answer was rejected (wrong guess), continue drilling
        }

        // Find cell with maximum uncertainty: prob closest to 0.5 among undrilled cells
        int best_cell = -1;
        double best_score = -1.0;
        for (int idx = 0; idx < total_cells; ++idx) {
            if (drill_res[idx] != -1) continue;
            double p = prob_oil[idx];
            double score = 1.0 - 2.0 * abs(p - 0.5); // score = 1 at p=0.5, 0 at p=0 or 1
            // Also prioritize cells with p > 0.05
            if (score > best_score) {
                best_score = score;
                best_cell = idx;
            }
        }

        if (best_cell == -1 || best_score <= 0.0) {
            // If no uncertain cell found among valid particles, find any undrilled cell where p > 0
            for (int idx = 0; idx < total_cells; ++idx) {
                if (drill_res[idx] == -1) {
                    best_cell = idx;
                    break;
                }
            }
            if (best_cell == -1) {
                // All cells drilled, submit answer
                vector<Point> ans;
                for (int idx = 0; idx < total_cells; ++idx) {
                    if (drill_res[idx] > 0) {
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
                return 0;
            }
        }

        do_drill(best_cell / n, best_cell % n);
    }

    return 0;
}
