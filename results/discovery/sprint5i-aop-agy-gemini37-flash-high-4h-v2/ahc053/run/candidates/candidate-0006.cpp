#include <iostream>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <random>

using namespace std;

const unsigned long long EXACT_BASE_CARDS[50] = {
    998000008000392ULL, 998001148115278ULL, 998007029593823ULL, 998019076951378ULL, 998036884520649ULL,
    998059697969001ULL, 998086827111382ULL, 998117710137246ULL, 998151900904383ULL, 998189044937950ULL,
    998228858287276ULL, 998271111223615ULL, 998315616110907ULL, 998362218434387ULL, 998410790131842ULL,
    998461224592587ULL, 998513432870887ULL, 998567340793603ULL, 998622886735519ULL, 998680019901223ULL,
    998738698998135ULL, 998798891217707ULL, 998860571465075ULL, 998923721794543ULL, 998988331021218ULL,
    999054394489265ULL, 999121913985699ULL, 999190897796122ULL, 999261360906086ULL, 999333325359431ULL,
    999406820793784ULL, 999481885184337ULL, 999558565841268ULL, 999636920725687ULL, 999717020176526ULL,
    999798949180916ULL, 999882810380734ULL, 999968728100605ULL, 1000056853829664ULL, 1000147373830610ULL,
    1000240519960308ULL, 1000336585516640ULL, 1000435949292773ULL, 1000539113737472ULL, 1000646768944219ULL,
    1000759907912272ULL, 1000880055109092ULL, 1001009786180572ULL, 1001154191870770ULL, 1001327055084410ULL,
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    int n, m;
    unsigned long long l, u;
    if (!(cin >> n >> m >> l >> u)) return 0;

    // 1. Base cards (50 cards)
    vector<unsigned long long> A(n);
    for (int k = 0; k < m; ++k) {
        A[k] = EXACT_BASE_CARDS[k];
    }

    // 2. Redundant delta cards (450 cards)
    double r = 1.33;
    int num_denoms = 40;
    double max_val = 2.0e12;
    vector<unsigned long long> denoms;
    double cur_val = max_val;
    while (cur_val >= 1.0 && (int)denoms.size() < num_denoms) {
        denoms.push_back((unsigned long long)cur_val);
        cur_val /= r;
    }

    int delta_cards = n - m; // 450
    int nd = denoms.size();
    vector<int> counts(nd, 0);
    int assigned = 0;
    for (int i = 0; i < nd && assigned < delta_cards; ++i) {
        int c = delta_cards / nd;
        if (i < 3) c = max(c - 2, 4);
        if (assigned + c > delta_cards) c = delta_cards - assigned;
        counts[i] = c;
        assigned += c;
    }
    while (assigned < delta_cards) {
        for (int i = 0; i < nd && assigned < delta_cards; ++i) {
            counts[i]++;
            assigned++;
        }
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

    auto calc_pile_err = [&](int p, unsigned long long sum_val) -> unsigned long long {
        if (sum_val > B[p - 1]) return sum_val - B[p - 1];
        return B[p - 1] - sum_val;
    };

    unsigned long long current_err = 0;
    for (int j = 1; j <= m; ++j) {
        current_err += calc_pile_err(j, S[j]);
    }

    unsigned long long best_err = current_err;
    vector<int> best_X = X;

    mt19937_64 rng(42);
    int sa_iters = 5000000;
    double t_start = 5e6;
    double t_end = 0.1;
    double t_decay = pow(t_end / t_start, 1.0 / sa_iters);
    double temp = t_start;

    uniform_int_distribution<int> card_dist(m, n - 1);
    uniform_int_distribution<int> pile_dist(0, m);
    uniform_int_distribution<int> move_dist(0, 4);
    uniform_real_distribution<double> prob_dist(0.0, 1.0);

    for (int iter = 0; iter < sa_iters; ++iter) {
        temp *= t_decay;
        int move_type = move_dist(rng);

        if (move_type <= 1) {
            // 1-card transfer
            int c = card_dist(rng);
            int old_p = X[c];
            int new_p = pile_dist(rng);
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

            if (delta_cost <= 0 || (temp > 1.0 && prob_dist(rng) < exp(-(double)delta_cost / temp))) {
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
            // Card swap
            int c1 = card_dist(rng);
            int c2 = card_dist(rng);
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

            if (delta_cost <= 0 || (temp > 1.0 && prob_dist(rng) < exp(-(double)delta_cost / temp))) {
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
            // 2-for-1 swap
            int c1 = card_dist(rng);
            int c2 = card_dist(rng);
            if (c1 == c2) continue;
            int p1 = X[c1];
            if (p1 == 0 || p1 != X[c2]) continue;

            int c3 = card_dist(rng);
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

            if (delta_cost <= 0 || (temp > 1.0 && prob_dist(rng) < exp(-(double)delta_cost / temp))) {
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
            // 2-for-2 swap
            int c1 = card_dist(rng);
            int c2 = card_dist(rng);
            if (c1 == c2) continue;
            int p1 = X[c1];
            if (p1 == 0 || p1 != X[c2]) continue;

            int c3 = card_dist(rng);
            int c4 = card_dist(rng);
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

            if (delta_cost <= 0 || (temp > 1.0 && prob_dist(rng) < exp(-(double)delta_cost / temp))) {
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

    // Output best_X
    for (int i = 0; i < n; ++i) {
        cout << best_X[i] << (i + 1 == n ? '\n' : ' ');
    }
    cout.flush();

    return 0;
}
