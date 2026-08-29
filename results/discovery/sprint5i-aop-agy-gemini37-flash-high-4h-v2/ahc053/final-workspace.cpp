#include <iostream>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>

using namespace std;

// Beta quantile approximation for order statistics
double beta_ppf(double p, double a, double b) {
    double mean = a / (a + b);
    double var = (a * b) / ((a + b) * (a + b) * (a + b + 1.0));
    double sd = sqrt(var);
    double z = -4.2648907; // 0.00001 quantile
    double q = mean + z * sd;
    if (q < 0.0) q = 0.0;
    if (q > 1.0) q = 1.0;
    return q;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    int n, m;
    unsigned long long l, u;
    if (!(cin >> n >> m >> l >> u)) return 0;

    unsigned long long w = u - l;

    // 1. Base cards (50 cards)
    vector<unsigned long long> A(n);
    for (int k = 0; k < m; ++k) {
        double q = beta_ppf(0.00001, k + 1, m - k);
        A[k] = l + (unsigned long long)(w * q);
    }

    // 2. Demand-tailored redundant delta cards (450 cards)
    double r = 1.33;
    int num_denoms = 42;
    double max_val = 1.6e12;
    vector<unsigned long long> denoms;
    double cur_val = max_val;
    while (cur_val >= 1.0 && (int)denoms.size() < num_denoms) {
        denoms.push_back((unsigned long long)cur_val);
        cur_val /= r;
    }

    int delta_cards = n - m; // 450
    int nd = denoms.size();
    vector<int> counts(nd, 0);

    // Exact empirical demand profile
    counts[0] = 5;
    counts[1] = 22;
    counts[2] = 24;
    counts[3] = 18;
    counts[4] = 10;
    counts[5] = 8;
    counts[6] = 8;
    int high_sum = 5 + 22 + 24 + 18 + 10 + 8 + 8; // 95
    int rem_cards = delta_cards - high_sum; // 355
    int rem_denoms = nd - 7; // 35
    for (int i = 7; i < nd; ++i) {
        counts[i] = rem_cards / rem_denoms;
    }
    int assigned = high_sum + (rem_cards / rem_denoms) * rem_denoms;
    for (int i = 7; i < nd && assigned < delta_cards; ++i) {
        counts[i]++;
        assigned++;
    }

    int card_idx = m;
    for (int i = 0; i < nd; ++i) {
        for (int c = 0; c < counts[i] && card_idx < n; ++c) {
            A[card_idx++] = denoms[i];
        }
    }
    while (card_idx < n) A[card_idx++] = 1;

    // Output A
    for (int i = 0; i < n; ++i) {
        cout << A[i] << (i + 1 == n ? '\n' : ' ');
    }
    cout.flush();

    // Read B
    vector<unsigned long long> B(m);
    for (int i = 0; i < m; ++i) {
        cin >> B[i];
    }

    auto start_time = chrono::high_resolution_clock::now();

    auto calc_pile_err = [&](int p, unsigned long long sum_val) -> unsigned long long {
        if (sum_val > B[p - 1]) return sum_val - B[p - 1];
        return B[p - 1] - sum_val;
    };

    unsigned long long global_best_err = 0ULL - 1ULL;
    vector<int> global_best_X(n, 0);

    int restart = 0;
    int sa_iters_per_restart = 6000000;

    while (true) {
        auto now = chrono::high_resolution_clock::now();
        double elapsed = chrono::duration<double>(now - start_time).count();
        if (elapsed > 1.70 && restart >= 1) break;

        mt19937_64 restart_rng(42 + restart * 10007);

        // Base card matching
        vector<int> X(n, 0);
        vector<unsigned long long> S(m + 1, 0);
        vector<bool> base_used(m, false);

        bool identity_ok = true;
        for (int j = 0; j < m; ++j) {
            if (A[j] > B[j]) {
                identity_ok = false;
                break;
            }
        }

        if (identity_ok) {
            for (int j = 1; j <= m; ++j) {
                X[j - 1] = j;
                S[j] = A[j - 1];
            }
        } else {
            for (int j = m - 1; j >= 0; --j) {
                int matched_k = -1;
                for (int k = m - 1; k >= 0; --k) {
                    if (!base_used[k] && A[k] <= B[j]) {
                        matched_k = k;
                        break;
                    }
                }
                if (matched_k != -1) {
                    base_used[matched_k] = true;
                    X[matched_k] = j + 1;
                    S[j + 1] = A[matched_k];
                } else {
                    for (int k = 0; k < m; ++k) {
                        if (!base_used[k]) {
                            base_used[k] = true;
                            X[k] = j + 1;
                            S[j + 1] = A[k];
                            break;
                        }
                    }
                }
            }
        }

        // Global Greedy for delta cards
        vector<int> delta_indices;
        for (int i = m; i < n; ++i) delta_indices.push_back(i);
        sort(delta_indices.begin(), delta_indices.end(), [&](int i, int j) {
            return A[i] > A[j];
        });

        for (int idx : delta_indices) {
            unsigned long long val = A[idx];
            int best_j = -1;
            unsigned long long min_rem = 0ULL - 1ULL;
            for (int j = 1; j <= m; ++j) {
                if (S[j] + val <= B[j - 1]) {
                    unsigned long long rem = B[j - 1] - (S[j] + val);
                    if (best_j == -1 || rem < min_rem) {
                        min_rem = rem;
                        best_j = j;
                    }
                }
            }
            if (best_j != -1) {
                S[best_j] += val;
                X[idx] = best_j;
            }
        }

        unsigned long long current_err = 0;
        for (int j = 1; j <= m; ++j) {
            current_err += calc_pile_err(j, S[j]);
        }

        unsigned long long best_err = current_err;
        vector<int> best_X = X;

        double t_start = 2.0e7;
        double t_end = 0.1;
        double t_decay = pow(t_end / t_start, 1.0 / sa_iters_per_restart);
        double temp = t_start;

        uniform_int_distribution<int> card_dist(m, n - 1);
        uniform_int_distribution<int> pile_dist(0, m);
        uniform_int_distribution<int> move_dist(0, 4);
        uniform_real_distribution<double> prob_dist(0.0, 1.0);

        for (int iter = 0; iter < sa_iters_per_restart; ++iter) {
            temp *= t_decay;
            int move_type = move_dist(restart_rng);

            if (move_type <= 1) {
                int c = card_dist(restart_rng);
                int old_p = X[c];
                int new_p = pile_dist(restart_rng);
                if (old_p == new_p) continue;

                unsigned long long val = A[c];
                long long delta_cost = 0;
                if (old_p > 0) {
                    delta_cost -= calc_pile_err(old_p, S[old_p]);
                    delta_cost += calc_pile_err(old_p, S[old_p] - val);
                }
                if (new_p > 0) {
                    delta_cost -= calc_pile_err(new_p, S[new_p]);
                    delta_cost += calc_pile_err(new_p, S[new_p] + val);
                }

                if (delta_cost <= 0 || (temp > 1.0 && prob_dist(restart_rng) < exp(-(double)delta_cost / temp))) {
                    if (old_p > 0) S[old_p] -= val;
                    if (new_p > 0) S[new_p] += val;
                    X[c] = new_p;
                    current_err += delta_cost;
                    if (current_err < best_err) {
                        best_err = current_err;
                        best_X = X;
                    }
                }
            } else if (move_type == 2) {
                int c1 = card_dist(restart_rng);
                int c2 = card_dist(restart_rng);
                if (c1 == c2) continue;
                int p1 = X[c1];
                int p2 = X[c2];
                if (p1 == p2) continue;

                unsigned long long v1 = A[c1];
                unsigned long long v2 = A[c2];

                long long delta_cost = 0;
                if (p1 > 0) {
                    delta_cost -= calc_pile_err(p1, S[p1]);
                    delta_cost += calc_pile_err(p1, S[p1] - v1 + v2);
                }
                if (p2 > 0) {
                    delta_cost -= calc_pile_err(p2, S[p2]);
                    delta_cost += calc_pile_err(p2, S[p2] - v2 + v1);
                }

                if (delta_cost <= 0 || (temp > 1.0 && prob_dist(restart_rng) < exp(-(double)delta_cost / temp))) {
                    if (p1 > 0) S[p1] = S[p1] - v1 + v2;
                    if (p2 > 0) S[p2] = S[p2] - v2 + v1;
                    X[c1] = p2;
                    X[c2] = p1;
                    current_err += delta_cost;
                    if (current_err < best_err) {
                        best_err = current_err;
                        best_X = X;
                    }
                }
            } else if (move_type == 3) {
                int c1 = card_dist(restart_rng);
                int c2 = card_dist(restart_rng);
                if (c1 == c2) continue;
                int p1 = X[c1];
                if (p1 == 0 || p1 != X[c2]) continue;

                int c3 = card_dist(restart_rng);
                int p2 = X[c3];
                if (p1 == p2) continue;

                unsigned long long v12 = A[c1] + A[c2];
                unsigned long long v3 = A[c3];

                long long delta_cost = 0;
                delta_cost -= calc_pile_err(p1, S[p1]);
                delta_cost += calc_pile_err(p1, S[p1] - v12 + v3);
                if (p2 > 0) {
                    delta_cost -= calc_pile_err(p2, S[p2]);
                    delta_cost += calc_pile_err(p2, S[p2] - v3 + v12);
                }

                if (delta_cost <= 0 || (temp > 1.0 && prob_dist(restart_rng) < exp(-(double)delta_cost / temp))) {
                    S[p1] = S[p1] - v12 + v3;
                    if (p2 > 0) S[p2] = S[p2] - v3 + v12;
                    X[c1] = p2;
                    X[c2] = p2;
                    X[c3] = p1;
                    current_err += delta_cost;
                    if (current_err < best_err) {
                        best_err = current_err;
                        best_X = X;
                    }
                }
            } else {
                int c1 = card_dist(restart_rng);
                int c2 = card_dist(restart_rng);
                if (c1 == c2) continue;
                int p1 = X[c1];
                if (p1 == 0 || p1 != X[c2]) continue;

                int c3 = card_dist(restart_rng);
                int c4 = card_dist(restart_rng);
                if (c3 == c4) continue;
                int p2 = X[c3];
                if (p2 == 0 || p1 == p2 || p2 != X[c4]) continue;

                unsigned long long v12 = A[c1] + A[c2];
                unsigned long long v34 = A[c3] + A[c4];

                long long delta_cost = 0;
                delta_cost -= calc_pile_err(p1, S[p1]);
                delta_cost += calc_pile_err(p1, S[p1] - v12 + v34);
                delta_cost -= calc_pile_err(p2, S[p2]);
                delta_cost += calc_pile_err(p2, S[p2] - v34 + v12);

                if (delta_cost <= 0 || (temp > 1.0 && prob_dist(restart_rng) < exp(-(double)delta_cost / temp))) {
                    S[p1] = S[p1] - v12 + v34;
                    S[p2] = S[p2] - v34 + v12;
                    X[c1] = p2;
                    X[c2] = p2;
                    X[c3] = p1;
                    X[c4] = p1;
                    current_err += delta_cost;
                    if (current_err < best_err) {
                        best_err = current_err;
                        best_X = X;
                    }
                }
            }
        }

        if (best_err < global_best_err) {
            global_best_err = best_err;
            global_best_X = best_X;
        }

        restart++;
    }

    // Output global_best_X
    for (int i = 0; i < n; ++i) {
        cout << global_best_X[i] << (i + 1 == n ? '\n' : ' ');
    }
    cout.flush();

    return 0;
}
