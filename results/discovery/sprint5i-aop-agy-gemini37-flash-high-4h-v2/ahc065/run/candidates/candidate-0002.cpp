#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;

const int N = 20;

struct Conveyor {
    vector<pair<int, int>> cells;
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    int n_in;
    if (!(cin >> n_in)) return 0;

    vector<vector<int>> grid(N, vector<int>(N));
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            cin >> grid[i][j];
        }
    }

    // Define 10 horizontal conveyors (0..9)
    // and 10 vertical conveyors (10..19)
    vector<Conveyor> conveyors(20);

    for (int k = 0; k < 10; ++k) {
        // H_k: rows 2k and 2k+1
        for (int j = 0; j < N; ++j) conveyors[k].cells.push_back({2 * k, j});
        for (int j = N - 1; j >= 0; --j) conveyors[k].cells.push_back({2 * k + 1, j});
    }

    for (int k = 0; k < 10; ++k) {
        // V_k: cols 2k and 2k+1
        int m = 10 + k;
        for (int i = 0; i < N; ++i) conveyors[m].cells.push_back({i, 2 * k});
        for (int i = N - 1; i >= 0; --i) conveyors[m].cells.push_back({i, 2 * k + 1});
    }

    // Output conveyors
    cout << conveyors.size() << "\n";
    for (const auto& conv : conveyors) {
        cout << conv.cells.size();
        for (const auto& p : conv.cells) {
            cout << " " << p.first << " " << p.second;
        }
        cout << "\n";
    }

    // Simulation state
    vector<pair<int, int>> ops;
    int target = 0;

    auto apply_op = [&](int m, int d) {
        ops.push_back({m, d});
        const auto& ij = conveyors[m].cells;
        int L = ij.size();
        if (d > 0) {
            int v = grid[ij[0].first][ij[0].second];
            for (int x = 1; x < L; ++x) {
                swap(grid[ij[x].first][ij[x].second], v);
            }
            grid[ij[0].first][ij[0].second] = v;
        } else {
            int v = grid[ij[0].first][ij[0].second];
            for (int x = 0; x < L - 1; ++x) {
                grid[ij[x].first][ij[x].second] = grid[ij[x + 1].first][ij[x + 1].second];
            }
            grid[ij[L - 1].first][ij[L - 1].second] = v;
        }
        if (grid[0][N / 2] == target) {
            grid[0][N / 2] = -1;
            target++;
        }
    };

    if (grid[0][N / 2] == target) {
        grid[0][N / 2] = -1;
        target++;
    }

    auto find_box = [&](int val) -> pair<int, int> {
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                if (grid[i][j] == val) return {i, j};
            }
        }
        return {-1, -1};
    };

    auto get_conv_pos = [&](int m, pair<int, int> cell) -> int {
        const auto& ij = conveyors[m].cells;
        for (size_t x = 0; x < ij.size(); ++x) {
            if (ij[x] == cell) return (int)x;
        }
        return -1;
    };

    int exit_col_conv = 10 + (N / 2) / 2; // V_5 is conveyor 15

    while (target < N * N) {
        pair<int, int> pos = find_box(target);
        if (pos.first == -1) {
            target++;
            continue;
        }

        int r = pos.first;
        int c = pos.second;
        int h_conv = r / 2;

        if (h_conv == 0) {
            // Target is already on H_0
            int cur_idx = get_conv_pos(0, pos);
            int goal_idx = get_conv_pos(0, {0, N / 2});
            int L = conveyors[0].cells.size();
            int fwd = (goal_idx - cur_idx + L) % L;
            int bwd = (cur_idx - goal_idx + L) % L;
            if (fwd <= bwd) {
                for (int s = 0; s < fwd; ++s) apply_op(0, 1);
            } else {
                for (int s = 0; s < bwd; ++s) apply_op(0, -1);
            }
        } else {
            // Find best intersection point between H_{r/2} and V_5
            int k = h_conv;
            vector<pair<int, int>> cand = {
                {2 * k, 10}, {2 * k, 11}, {2 * k + 1, 10}, {2 * k + 1, 11}
            };

            int best_cost = 1e9;
            int best_h_dir = 1, best_h_steps = 0;
            int best_v_dir = 1, best_v_steps = 0;

            int cur_h_idx = get_conv_pos(h_conv, pos);
            int L_h = conveyors[h_conv].cells.size();
            int L_v = conveyors[exit_col_conv].cells.size();
            int goal_v_idx = get_conv_pos(exit_col_conv, {0, N / 2});

            for (auto cell : cand) {
                int cell_h_idx = get_conv_pos(h_conv, cell);
                int fwd_h = (cell_h_idx - cur_h_idx + L_h) % L_h;
                int bwd_h = (cur_h_idx - cell_h_idx + L_h) % L_h;
                int h_steps = min(fwd_h, bwd_h);
                int h_dir = (fwd_h <= bwd_h) ? 1 : -1;

                int cell_v_idx = get_conv_pos(exit_col_conv, cell);
                int fwd_v = (goal_v_idx - cell_v_idx + L_v) % L_v;
                int bwd_v = (cell_v_idx - goal_v_idx + L_v) % L_v;
                int v_steps = min(fwd_v, bwd_v);
                int v_dir = (fwd_v <= bwd_v) ? 1 : -1;

                int total_cost = h_steps + v_steps;
                if (total_cost < best_cost) {
                    best_cost = total_cost;
                    best_h_dir = h_dir;
                    best_h_steps = h_steps;
                    best_v_dir = v_dir;
                    best_v_steps = v_steps;
                }
            }

            // Apply H steps
            for (int s = 0; s < best_h_steps; ++s) apply_op(h_conv, best_h_dir);

            // Apply V steps
            for (int s = 0; s < best_v_steps; ++s) apply_op(exit_col_conv, best_v_dir);
        }
    }

    // Output operations
    cout << ops.size() << "\n";
    for (const auto& op : ops) {
        cout << op.first << " " << op.second << "\n";
    }

    return 0;
}

