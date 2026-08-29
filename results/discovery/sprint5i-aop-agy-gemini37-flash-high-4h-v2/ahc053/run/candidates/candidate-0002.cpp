#include <iostream>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

using namespace std;

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    int n, m;
    unsigned long long l, u;
    if (!(cin >> n >> m >> l >> u)) return 0;

    // We have N = 500 cards, M = 50 piles.
    // 50 base cards of value L
    // 450 delta cards
    vector<unsigned long long> A(n, 1);
    for (int i = 0; i < m; ++i) {
        A[i] = l;
    }

    // For remaining 450 cards, let's put powers of 2 or multiple copies of powers of 2
    // Max delta is 4 * 10^12 < 2^42.
    // 42 bits: 0 .. 41.
    // 450 / 42 = 10 full sets of 2^0 .. 2^41 (uses 420 cards), plus 30 cards of higher bits
    int card_idx = m;
    for (int rep = 0; rep < 10; ++rep) {
        for (int b = 0; b < 42 && card_idx < n; ++b) {
            A[card_idx++] = (1ULL << b);
        }
    }
    // remaining 30 cards: powers of 2 from 20 to 41
    for (int b = 41; b >= 0 && card_idx < n; --b) {
        A[card_idx++] = (1ULL << b);
    }

    // Output A
    for (int i = 0; i < n; ++i) {
        cout << A[i] << (i + 1 == n ? '\n' : ' ');
    }
    cout.flush();

    vector<unsigned long long> B(m);
    for (int i = 0; i < m; ++i) {
        cin >> B[i];
    }

    // Piles assignment: X[i] is pile 1..M (or 0 for discard)
    vector<int> X(n, 0);
    // Base cards: card i goes to pile i + 1
    for (int i = 0; i < m; ++i) {
        X[i] = i + 1;
    }

    // Remaining targets for each pile
    vector<unsigned long long> R(m);
    for (int i = 0; i < m; ++i) {
        R[i] = B[i] - l;
    }

    // Greedy assignment of delta cards from largest to smallest
    // Gather all delta card indices sorted by A[i] descending
    vector<int> delta_indices;
    for (int i = m; i < n; ++i) {
        delta_indices.push_back(i);
    }
    sort(delta_indices.begin(), delta_indices.end(), [&](int i, int j) {
        return A[i] > A[j];
    });

    vector<unsigned long long> current_sum(m, 0);
    for (int idx : delta_indices) {
        unsigned long long val = A[idx];
        // Find pile j where current_sum[j] + val <= R[j] and R[j] - (current_sum[j] + val) is minimized
        int best_j = -1;
        unsigned long long min_rem = 0ULL - 1ULL;
        for (int j = 0; j < m; ++j) {
            if (current_sum[j] + val <= R[j]) {
                unsigned long long rem = R[j] - (current_sum[j] + val);
                if (best_j == -1 || rem < min_rem) {
                    min_rem = rem;
                    best_j = j;
                }
            }
        }
        if (best_j != -1) {
            current_sum[best_j] += val;
            X[idx] = best_j + 1;
        }
    }

    // Output X
    for (int i = 0; i < n; ++i) {
        cout << X[i] << (i + 1 == n ? '\n' : ' ');
    }
    cout.flush();

    return 0;
}
