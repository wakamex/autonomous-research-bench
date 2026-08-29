#include <iostream>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <random>

using namespace std;

// Beta quantile approximation
double beta_ppf(double p, double a, double b) {
    double mean = a / (a + b);
    double var = (a * b) / ((a + b) * (a + b) * (a + b + 1.0));
    double sd = sqrt(var);
    double z = -3.2905267; // 0.0005 quantile
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
        double q = beta_ppf(0.0005, k + 1, m - k);
        A[k] = l + (unsigned long long)(w * q);
    }

    // 2. Redundant delta cards (450 cards)
    double r = 1.35;
    double max_val = 2.0e12;
    vector<unsigned long long> denoms;
    double cur_val = max_val;
    while (cur_val >= 10.0 && denoms.size() < 40) {
        denoms.push_back((unsigned long long)cur_val);
        cur_val /= r;
    }

    int delta_cards = n - m; // 450
    int num_denoms = denoms.size();
    vector<int> counts(num_denoms, 0);
    int assigned = 0;
    for (int i = 0; i < num_denoms && assigned < delta_cards; ++i) {
        int c = 14;
        if (i < 3) c = 10;
        if (assigned + c > delta_cards) c = delta_cards - assigned;
        counts[i] = c;
        assigned += c;
    }
    while (assigned < delta_cards) {
        for (int i = 0; i < num_denoms && assigned < delta_cards; ++i) {
            counts[i]++;
            assigned++;
        }
    }

    int card_idx = m;
    for (int i = 0; i < num_denoms; ++i) {
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

    // Initial assignment: base cards
    vector<int> X(n, 0);
    vector<unsigned long long> S(m + 1, 0);
    for (int j = 1; j <= m; ++j) {
        X[j - 1] = j;
        S[j] = A[j - 1];
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

    // Multi-neighborhood Local Search
    mt19937_64 rng(42);
    int ls_iters = 1200000;
    uniform_int_distribution<int> card_dist(m, n - 1);
    uniform_int_distribution<int> pile_dist(0, m);
    uniform_int_distribution<int> move_dist(0, 1);

    for (int iter = 0; iter < ls_iters; ++iter) {
        int move_type = move_dist(rng);
        if (move_type == 0) {
            // Single card transfer
            int c = card_dist(rng);
            int old_p = X[c];
            int new_p = pile_dist(rng);
            if (old_p == new_p) continue;

            unsigned long long val = A[c];
            unsigned long long old_cost = 0, new_cost = 0;
            if (old_p > 0) {
                old_cost += calc_pile_err(old_p, S[old_p]);
                new_cost += calc_pile_err(old_p, S[old_p] - val);
            }
            if (new_p > 0) {
                old_cost += calc_pile_err(new_p, S[new_p]);
                new_cost += calc_pile_err(new_p, S[new_p] + val);
            }
            if (new_cost <= old_cost) {
                if (old_p > 0) S[old_p] -= val;
                if (new_p > 0) S[new_p] += val;
                X[c] = new_p;
            }
        } else {
            // Card swap
            int c1 = card_dist(rng);
            int c2 = card_dist(rng);
            if (c1 == c2) continue;
            int p1 = X[c1];
            int p2 = X[c2];
            if (p1 == p2) continue;

            unsigned long long v1 = A[c1];
            unsigned long long v2 = A[c2];

            unsigned long long old_cost = 0, new_cost = 0;
            if (p1 > 0) {
                old_cost += calc_pile_err(p1, S[p1]);
                new_cost += calc_pile_err(p1, S[p1] - v1 + v2);
            }
            if (p2 > 0) {
                old_cost += calc_pile_err(p2, S[p2]);
                new_cost += calc_pile_err(p2, S[p2] - v2 + v1);
            }
            if (new_cost <= old_cost) {
                if (p1 > 0) S[p1] = S[p1] - v1 + v2;
                if (p2 > 0) S[p2] = S[p2] - v2 + v1;
                X[c1] = p2;
                X[c2] = p1;
            }
        }
    }

    // Output X
    for (int i = 0; i < n; ++i) {
        cout << X[i] << (i + 1 == n ? '\n' : ' ');
    }
    cout.flush();

    return 0;
}
