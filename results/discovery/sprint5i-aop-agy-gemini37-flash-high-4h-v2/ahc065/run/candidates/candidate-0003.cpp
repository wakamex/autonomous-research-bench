#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <unordered_set>

using namespace std;

const int N = 20;

struct Conveyor {
    vector<pair<int, int>> cells;
};

vector<Conveyor> conveyors(20);
int conv_pos[20][20][20];
int cell_dist[20][20];

void init_conveyors() {
    memset(conv_pos, -1, sizeof(conv_pos));
    for (int k = 0; k < 10; ++k) {
        for (int j = 0; j < N; ++j) conveyors[k].cells.push_back({2 * k, j});
        for (int j = N - 1; j >= 0; --j) conveyors[k].cells.push_back({2 * k + 1, j});
    }

    for (int k = 0; k < 10; ++k) {
        int m = 10 + k;
        for (int i = 0; i < N; ++i) conveyors[m].cells.push_back({i, 2 * k});
        for (int i = N - 1; i >= 0; --i) conveyors[m].cells.push_back({i, 2 * k + 1});
    }

    for (int m = 0; m < 20; ++m) {
        for (size_t x = 0; x < conveyors[m].cells.size(); ++x) {
            auto p = conveyors[m].cells[x];
            conv_pos[m][p.first][p.second] = x;
        }
    }

    int goal_h = conv_pos[0][0][N / 2];
    int goal_v = conv_pos[15][0][N / 2];

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int h_conv = r / 2;
            int v_conv = 10 + c / 2;

            if (h_conv == 0) {
                int cur = conv_pos[0][r][c];
                int fwd = (goal_h - cur + 40) % 40;
                cell_dist[r][c] = min(fwd, 40 - fwd);
            } else if (v_conv == 15) {
                int cur = conv_pos[15][r][c];
                int fwd = (goal_v - cur + 40) % 40;
                cell_dist[r][c] = min(fwd, 40 - fwd);
            } else {
                int best1 = 1e9;
                int k = h_conv;
                pair<int, int> cand1[4] = {{2 * k, 10}, {2 * k, 11}, {2 * k + 1, 10}, {2 * k + 1, 11}};
                int cur_h = conv_pos[h_conv][r][c];
                for (int i = 0; i < 4; ++i) {
                    int idx_h = conv_pos[h_conv][cand1[i].first][cand1[i].second];
                    int dh = min((idx_h - cur_h + 40) % 40, (cur_h - idx_h + 40) % 40);
                    int idx_v = conv_pos[15][cand1[i].first][cand1[i].second];
                    int dv = min((goal_v - idx_v + 40) % 40, (idx_v - goal_v + 40) % 40);
                    best1 = min(best1, dh + dv);
                }

                int best2 = 1e9;
                int j = c / 2;
                pair<int, int> cand2[4] = {{0, 2 * j}, {0, 2 * j + 1}, {1, 2 * j}, {1, 2 * j + 1}};
                int cur_v = conv_pos[v_conv][r][c];
                for (int i = 0; i < 4; ++i) {
                    int idx_v = conv_pos[v_conv][cand2[i].first][cand2[i].second];
                    int dv = min((idx_v - cur_v + 40) % 40, (cur_v - idx_v + 40) % 40);
                    int idx_h = conv_pos[0][cand2[i].first][cand2[i].second];
                    int dh = min((goal_h - idx_h + 40) % 40, (idx_h - goal_h + 40) % 40);
                    best2 = min(best2, dv + dh);
                }

                cell_dist[r][c] = min(best1, best2);
            }
        }
    }
}

struct Board {
    int grid[N][N];
    pair<int, int> pos_of[N * N];
    int target;

    void init(const vector<vector<int>>& g) {
        target = 0;
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                grid[i][j] = g[i][j];
                if (g[i][j] >= 0) pos_of[g[i][j]] = {i, j};
            }
        }
        if (grid[0][N / 2] == 0) {
            grid[0][N / 2] = -1;
            pos_of[0] = {-1, -1};
            target = 1;
        }
    }

    void apply_op(int m, int d) {
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
        for (int x = 0; x < L; ++x) {
            int val = grid[ij[x].first][ij[x].second];
            if (val >= 0) {
                pos_of[val] = ij[x];
            }
        }
        while (target < N * N && grid[0][N / 2] == target) {
            pos_of[target] = {-1, -1};
            grid[0][N / 2] = -1;
            target++;
        }
    }

    void apply_fast_multi(int m, int d, int count) {
        if (count == 0) return;
        if (m == 0 || m == 15) {
            for (int i = 0; i < count; ++i) apply_op(m, d);
            return;
        }
        const auto& ij = conveyors[m].cells;
        int L = ij.size();
        int shift = (d * count % L + L) % L;
        if (shift == 0) return;

        static int vals[40];
        for (int x = 0; x < L; ++x) vals[x] = grid[ij[x].first][ij[x].second];

        for (int x = 0; x < L; ++x) {
            int src_idx = (x - shift + L) % L;
            int v = vals[src_idx];
            grid[ij[x].first][ij[x].second] = v;
            if (v >= 0) pos_of[v] = ij[x];
        }
    }

    uint64_t hash_state() const {
        uint64_t h = (uint64_t)target * 1000000007ULL;
        for (int w = 0; w < 6 && target + w < N * N; ++w) {
            auto p = pos_of[target + w];
            h = h * 31337ULL + (p.first * 20 + p.second + 1);
        }
        return h;
    }
};

struct Step {
    int m, d, count;
};

struct ActionSequence {
    vector<Step> steps;
    int total_ops() const {
        int s = 0;
        for (const auto& step : steps) s += step.count;
        return s;
    }
};

void generate_candidates(const Board& b, vector<ActionSequence>& cands) {
    cands.clear();
    int t = b.target;
    if (t >= N * N) return;
    auto p0 = b.pos_of[t];
    int r0 = p0.first, c0 = p0.second;
    int h0 = r0 / 2;
    int v0 = 10 + c0 / 2;

    int goal_h = conv_pos[0][0][N / 2];
    int goal_v = conv_pos[15][0][N / 2];

    if (h0 == 0) {
        int cur_idx = conv_pos[0][r0][c0];
        int fwd = (goal_h - cur_idx + 40) % 40;
        int bwd = (cur_idx - goal_h + 40) % 40;
        cands.push_back({{{0, 1, fwd}}});
        cands.push_back({{{0, -1, bwd}}});
    } else if (v0 == 15) {
        int cur_idx = conv_pos[15][r0][c0];
        int fwd = (goal_v - cur_idx + 40) % 40;
        int bwd = (cur_idx - goal_v + 40) % 40;
        cands.push_back({{{15, 1, fwd}}});
        cands.push_back({{{15, -1, bwd}}});
    } else {
        // Single target via V_5
        int k = h0;
        pair<int, int> cand1[4] = {{2 * k, 10}, {2 * k, 11}, {2 * k + 1, 10}, {2 * k + 1, 11}};
        int cur_h = conv_pos[h0][r0][c0];
        for (int i = 0; i < 4; ++i) {
            int idx_h = conv_pos[h0][cand1[i].first][cand1[i].second];
            int fwd_h = (idx_h - cur_h + 40) % 40;
            int bwd_h = (cur_h - idx_h + 40) % 40;

            int idx_v = conv_pos[15][cand1[i].first][cand1[i].second];
            int fwd_v = (goal_v - idx_v + 40) % 40;
            int bwd_v = (idx_v - goal_v + 40) % 40;

            int dh = (fwd_h <= bwd_h) ? fwd_h : bwd_h;
            int dir_h = (fwd_h <= bwd_h) ? 1 : -1;
            int dv = (fwd_v <= bwd_v) ? fwd_v : bwd_v;
            int dir_v = (fwd_v <= bwd_v) ? 1 : -1;

            cands.push_back({{{h0, dir_h, dh}, {15, dir_v, dv}}});
            if (abs(fwd_h - bwd_h) <= 6) cands.push_back({{{h0, -dir_h, 40 - dh}, {15, dir_v, dv}}});
            if (abs(fwd_v - bwd_v) <= 6) cands.push_back({{{h0, dir_h, dh}, {15, -dir_v, 40 - dv}}});
        }

        // Single target via H_0
        int j = c0 / 2;
        pair<int, int> cand2[4] = {{0, 2 * j}, {0, 2 * j + 1}, {1, 2 * j}, {1, 2 * j + 1}};
        int cur_v = conv_pos[v0][r0][c0];
        for (int i = 0; i < 4; ++i) {
            int idx_v = conv_pos[v0][cand2[i].first][cand2[i].second];
            int fwd_v = (idx_v - cur_v + 40) % 40;
            int bwd_v = (cur_v - idx_v + 40) % 40;

            int idx_h = conv_pos[0][cand2[i].first][cand2[i].second];
            int fwd_h = (goal_h - idx_h + 40) % 40;
            int bwd_h = (idx_h - goal_h + 40) % 40;

            int dv = (fwd_v <= bwd_v) ? fwd_v : bwd_v;
            int dir_v = (fwd_v <= bwd_v) ? 1 : -1;
            int dh = (fwd_h <= bwd_h) ? fwd_h : bwd_h;
            int dir_h = (fwd_h <= bwd_h) ? 1 : -1;

            cands.push_back({{{v0, dir_v, dv}, {0, dir_h, dh}}});
            if (abs(fwd_v - bwd_v) <= 6) cands.push_back({{{v0, -dir_v, 40 - dv}, {0, dir_h, dh}}});
            if (abs(fwd_h - bwd_h) <= 6) cands.push_back({{{v0, dir_v, dv}, {0, -dir_h, 40 - dh}}});
        }
    }

    // 2-Target Chaining
    int t1 = t + 1;
    if (t1 < N * N) {
        auto p1 = b.pos_of[t1];
        int r1 = p1.first, c1 = p1.second;
        int h1 = r1 / 2;
        int v1 = 10 + c1 / 2;

        if (h0 != h1 && h0 > 0 && h1 > 0) {
            pair<int, int> cand_t0[4] = {{2 * h0, 10}, {2 * h0, 11}, {2 * h0 + 1, 10}, {2 * h0 + 1, 11}};
            pair<int, int> cand_t1[4] = {{2 * h1, 10}, {2 * h1, 11}, {2 * h1 + 1, 10}, {2 * h1 + 1, 11}};
            int cur_h0 = conv_pos[h0][r0][c0];
            int cur_h1 = conv_pos[h1][r1][c1];

            for (int i0 = 0; i0 < 4; ++i0) {
                int idx_h0 = conv_pos[h0][cand_t0[i0].first][cand_t0[i0].second];
                int dh0 = min((idx_h0 - cur_h0 + 40) % 40, (cur_h0 - idx_h0 + 40) % 40);
                int dir_h0 = ((idx_h0 - cur_h0 + 40) % 40 <= (cur_h0 - idx_h0 + 40) % 40) ? 1 : -1;
                int pos_v0 = conv_pos[15][cand_t0[i0].first][cand_t0[i0].second];

                for (int i1 = 0; i1 < 4; ++i1) {
                    int idx_h1 = conv_pos[h1][cand_t1[i1].first][cand_t1[i1].second];
                    int dh1 = min((idx_h1 - cur_h1 + 40) % 40, (cur_h1 - idx_h1 + 40) % 40);
                    int dir_h1 = ((idx_h1 - cur_h1 + 40) % 40 <= (cur_h1 - idx_h1 + 40) % 40) ? 1 : -1;
                    int pos_v1 = conv_pos[15][cand_t1[i1].first][cand_t1[i1].second];

                    for (int dir_v : {1, -1}) {
                        int s0 = (dir_v == 1) ? (goal_v - pos_v0 + 40) % 40 : (pos_v0 - goal_v + 40) % 40;
                        int new_pos_v1 = (pos_v1 + dir_v * s0 % 40 + 40) % 40;
                        int s1 = (dir_v == 1) ? (goal_v - new_pos_v1 + 40) % 40 : (new_pos_v1 - goal_v + 40) % 40;

                        if (s0 + s1 <= 30) {
                            cands.push_back({{{h0, dir_h0, dh0}, {h1, dir_h1, dh1}, {15, dir_v, s0 + s1}}});
                        }
                    }
                }
            }
        }

        if (v0 != v1 && v0 != 10 && v1 != 10) {
            int j0 = c0 / 2;
            int j1 = c1 / 2;
            pair<int, int> cand_t0[4] = {{0, 2 * j0}, {0, 2 * j0 + 1}, {1, 2 * j0}, {1, 2 * j0 + 1}};
            pair<int, int> cand_t1[4] = {{0, 2 * j1}, {0, 2 * j1 + 1}, {1, 2 * j1}, {1, 2 * j1 + 1}};
            int cur_v0 = conv_pos[v0][r0][c0];
            int cur_v1 = conv_pos[v1][r1][c1];

            for (int i0 = 0; i0 < 4; ++i0) {
                int idx_v0 = conv_pos[v0][cand_t0[i0].first][cand_t0[i0].second];
                int dv0 = min((idx_v0 - cur_v0 + 40) % 40, (cur_v0 - idx_v0 + 40) % 40);
                int dir_v0 = ((idx_v0 - cur_v0 + 40) % 40 <= (cur_v0 - idx_v0 + 40) % 40) ? 1 : -1;
                int pos_h0 = conv_pos[0][cand_t0[i0].first][cand_t0[i0].second];

                for (int i1 = 0; i1 < 4; ++i1) {
                    int idx_v1 = conv_pos[v1][cand_t1[i1].first][cand_t1[i1].second];
                    int dv1 = min((idx_v1 - cur_v1 + 40) % 40, (cur_v1 - idx_v1 + 40) % 40);
                    int dir_v1 = ((idx_v1 - cur_v1 + 40) % 40 <= (cur_v1 - idx_v1 + 40) % 40) ? 1 : -1;
                    int pos_h1 = conv_pos[0][cand_t1[i1].first][cand_t1[i1].second];

                    for (int dir_h : {1, -1}) {
                        int s0 = (dir_h == 1) ? (goal_h - pos_h0 + 40) % 40 : (pos_h0 - goal_h + 40) % 40;
                        int new_pos_h1 = (pos_h1 + dir_h * s0 % 40 + 40) % 40;
                        int s1 = (dir_h == 1) ? (goal_h - new_pos_h1 + 40) % 40 : (new_pos_h1 - goal_h + 40) % 40;

                        if (s0 + s1 <= 30) {
                            cands.push_back({{{v0, dir_v0, dv0}, {v1, dir_v1, dv1}, {0, dir_h, s0 + s1}}});
                        }
                    }
                }
            }
        }
    }
}

struct BeamNode {
    Board b;
    vector<pair<int, int>> ops;
    int total_ops;
    double eval_score;

    bool operator<(const BeamNode& other) const {
        return eval_score < other.eval_score;
    }
};

const int LOOKAHEAD_W = 10;
double weights[LOOKAHEAD_W] = {1.0, 0.75, 0.55, 0.40, 0.28, 0.20, 0.14, 0.10, 0.06, 0.03};

inline double eval_board(const Board& b, int ops_spent) {
    double future = 0.0;
    int t = b.target;
    for (int w = 0; w < LOOKAHEAD_W; ++w) {
        int val = t + w;
        if (val < N * N) {
            auto p = b.pos_of[val];
            if (p.first != -1) {
                future += weights[w] * cell_dist[p.first][p.second];
            }
        }
    }
    return ops_spent + future;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    auto start_time = chrono::steady_clock::now();

    init_conveyors();

    int n_in;
    if (!(cin >> n_in)) return 0;

    vector<vector<int>> g(N, vector<int>(N));
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) cin >> g[i][j];
    }

    Board start_b;
    start_b.init(g);

    cout << conveyors.size() << "\n";
    for (const auto& conv : conveyors) {
        cout << conv.cells.size();
        for (const auto& p : conv.cells) {
            cout << " " << p.first << " " << p.second;
        }
        cout << "\n";
    }

    vector<BeamNode> beam;
    beam.push_back({start_b, {}, 0, eval_board(start_b, 0)});

    unordered_set<uint64_t> seen_hashes;

    int BASE_BEAM = 28;

    while (true) {
        bool all_done = true;
        for (const auto& node : beam) {
            if (node.b.target < N * N) {
                all_done = false;
                break;
            }
        }
        if (all_done) break;

        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration<double>(now - start_time).count();
        int current_beam = BASE_BEAM;
        if (elapsed > 1.6) current_beam = 8;
        else if (elapsed > 1.4) current_beam = 16;
        else if (elapsed > 1.2) current_beam = 22;

        vector<BeamNode> next_beam;
        seen_hashes.clear();

        for (const auto& node : beam) {
            if (node.b.target >= N * N) {
                next_beam.push_back(node);
                continue;
            }

            vector<ActionSequence> cands;
            generate_candidates(node.b, cands);

            for (const auto& cand : cands) {
                BeamNode child = node;
                for (const auto& step : cand.steps) {
                    child.b.apply_fast_multi(step.m, step.d, step.count);
                    for (int count = 0; count < step.count; ++count) {
                        child.ops.push_back({step.m, step.d});
                    }
                    child.total_ops += step.count;
                }
                child.eval_score = eval_board(child.b, child.total_ops);
                next_beam.push_back(child);
            }
        }

        sort(next_beam.begin(), next_beam.end());
        vector<BeamNode> filtered_beam;
        for (auto& node : next_beam) {
            uint64_t h = node.b.hash_state();
            if (seen_hashes.insert(h).second) {
                filtered_beam.push_back(move(node));
                if ((int)filtered_beam.size() == current_beam) break;
            }
        }
        beam = move(filtered_beam);
    }

    const auto& best = beam[0];
    cout << best.ops.size() << "\n";
    for (const auto& op : best.ops) {
        cout << op.first << " " << op.second << "\n";
    }

    return 0;
}


