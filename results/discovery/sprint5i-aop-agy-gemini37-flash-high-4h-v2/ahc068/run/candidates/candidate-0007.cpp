#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <cassert>
#include <cmath>

using namespace std;

struct Op {
    char d;
    int r, c, h, w;
};

int N;
int orig_A[20][20];
bool V_wall[20][20];
bool H_wall[20][20];

int cur_A[20][20];
bool locked_cell[20][20];
pair<int, int> pos_of[400];
vector<Op> cur_ops;

const int dr[4] = {-1, 1, 0, 0};
const int dc[4] = {0, 0, -1, 1};

bool can_step(int r1, int c1, int r2, int c2) {
    if (r2 < 0 || r2 >= N || c2 < 0 || c2 >= N) return false;
    if (locked_cell[r2][c2]) return false;
    if (r1 == r2) {
        int min_c = min(c1, c2);
        return !V_wall[r1][min_c];
    } else {
        int min_r = min(r1, r2);
        return !H_wall[min_r][c1];
    }
}

void apply_operation(const Op& op) {
    cur_ops.push_back(op);
    if (op.d == 'V') {
        int half = op.h / 2;
        for (int x = 0; x < half; ++x) {
            for (int y = 0; y < op.w; ++y) {
                int r1 = op.r + x, c1 = op.c + y;
                int r2 = op.r + half + x, c2 = op.c + y;
                int v1 = cur_A[r1][c1], v2 = cur_A[r2][c2];
                cur_A[r1][c1] = v2;
                cur_A[r2][c2] = v1;
                pos_of[v2] = {r1, c1};
                pos_of[v1] = {r2, c2};
            }
        }
    } else {
        int half = op.w / 2;
        for (int x = 0; x < op.h; ++x) {
            for (int y = 0; y < half; ++y) {
                int r1 = op.r + x, c1 = op.c + y;
                int r2 = op.r + x, c2 = op.c + half + y;
                int v1 = cur_A[r1][c1], v2 = cur_A[r2][c2];
                cur_A[r1][c1] = v2;
                cur_A[r2][c2] = v1;
                pos_of[v2] = {r1, c1};
                pos_of[v1] = {r2, c2};
            }
        }
    }
}

int tin[20][20], low[20][20], timer;
bool is_cut[20][20];

void dfs_cut(int r, int c, int pr, int pc) {
    tin[r][c] = low[r][c] = ++timer;
    int children = 0;
    for (int d = 0; d < 4; ++d) {
        int nr = r + dr[d], nc = c + dc[d];
        if (!can_step(r, c, nr, nc)) continue;
        if (nr == pr && nc == pc) continue;
        if (tin[nr][nc] > 0) {
            low[r][c] = min(low[r][c], tin[nr][nc]);
        } else {
            dfs_cut(nr, nc, r, c);
            low[r][c] = min(low[r][c], low[nr][nc]);
            if (low[nr][nc] >= tin[r][c] && pr != -1) {
                is_cut[r][c] = true;
            }
            ++children;
        }
    }
    if (pr == -1 && children > 1) {
        is_cut[r][c] = true;
    }
}

void find_articulation_points(vector<pair<int, int>>& non_cut) {
    timer = 0;
    int start_r = -1, start_c = -1;
    int unlocked_count = 0;
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            tin[r][c] = 0;
            low[r][c] = 0;
            is_cut[r][c] = false;
            if (!locked_cell[r][c]) {
                ++unlocked_count;
                if (start_r == -1) {
                    start_r = r;
                    start_c = c;
                }
            }
        }
    }

    if (unlocked_count <= 1) return;

    dfs_cut(start_r, start_c, -1, -1);

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (!locked_cell[r][c] && !is_cut[r][c]) {
                non_cut.push_back({r, c});
            }
        }
    }
}

// Compute side effect penalty for horizontal jump window
int eval_h_window_penalty(int r, int c0, int w, int target_c) {
    int k = w / 2;
    int penalty = 0;
    for (int y = 0; y < k; ++y) {
        int c1 = c0 + y, c2 = c0 + k + y;
        if (c1 == target_c || c2 == target_c) continue; // target card itself
        int v1 = cur_A[r][c1], v2 = cur_A[r][c2];
        int tr1 = v1 / N, tc1 = v1 % N;
        int tr2 = v2 / N, tc2 = v2 % N;
        int old_d = abs(c1 - tc1) + abs(c2 - tc2);
        int new_d = abs(c2 - tc1) + abs(c1 - tc2);
        penalty += (new_d - old_d);
    }
    return penalty;
}

// Compute side effect penalty for vertical jump window
int eval_v_window_penalty(int r0, int c, int h, int target_r) {
    int k = h / 2;
    int penalty = 0;
    for (int x = 0; x < k; ++x) {
        int r1 = r0 + x, r2 = r0 + k + x;
        if (r1 == target_r || r2 == target_r) continue;
        int v1 = cur_A[r1][c], v2 = cur_A[r2][c];
        int tr1 = v1 / N, tc1 = v1 % N;
        int tr2 = v2 / N, tc2 = v2 % N;
        int old_d = abs(r1 - tr1) + abs(r2 - tr2);
        int new_d = abs(r2 - tr1) + abs(r1 - tr2);
        penalty += (new_d - old_d);
    }
    return penalty;
}

vector<Op> find_op_path(int sr, int sc, int tr, int tc) {
    if (sr == tr && sc == tc) return {};

    static int dist[20][20];
    static int best_cost_to[20][20];
    static pair<int, int> parent_cell[20][20];
    static Op parent_op[20][20];

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            dist[r][c] = -1;
            best_cost_to[r][c] = 1e9;
        }
    }

    queue<pair<int, int>> q;
    dist[sr][sc] = 0;
    best_cost_to[sr][sc] = 0;
    q.push({sr, sc});

    while (!q.empty()) {
        auto [r, c] = q.front();
        q.pop();

        if (r == tr && c == tc) break;

        // Horizontal jumps
        int min_c = c, max_c = c;
        while (min_c > 0 && !locked_cell[r][min_c - 1] && !V_wall[r][min_c - 1]) min_c--;
        while (max_c + 1 < N && !locked_cell[r][max_c + 1] && !V_wall[r][max_c]) max_c++;

        int seg_len = max_c - min_c + 1;
        for (int w = 2; w <= seg_len; w += 2) {
            int k = w / 2;
            int start_c0 = max(min_c, c - w + 1);
            int end_c0 = min(c, max_c - w + 1);
            for (int c0 = start_c0; c0 <= end_c0; ++c0) {
                int next_c = (c < c0 + k) ? (c + k) : (c - k);
                int pen = eval_h_window_penalty(r, c0, w, c);
                if (dist[r][next_c] == -1) {
                    dist[r][next_c] = dist[r][c] + 1;
                    best_cost_to[r][next_c] = pen;
                    parent_cell[r][next_c] = {r, c};
                    parent_op[r][next_c] = Op{'H', r, c0, 1, w};
                    q.push({r, next_c});
                } else if (dist[r][next_c] == dist[r][c] + 1 && pen < best_cost_to[r][next_c]) {
                    best_cost_to[r][next_c] = pen;
                    parent_cell[r][next_c] = {r, c};
                    parent_op[r][next_c] = Op{'H', r, c0, 1, w};
                }
            }
        }

        // Vertical jumps
        int min_r = r, max_r = r;
        while (min_r > 0 && !locked_cell[min_r - 1][c] && !H_wall[min_r - 1][c]) min_r--;
        while (max_r + 1 < N && !locked_cell[max_r + 1][c] && !H_wall[max_r][c]) max_r++;

        int v_seg_len = max_r - min_r + 1;
        for (int h = 2; h <= v_seg_len; h += 2) {
            int k = h / 2;
            int start_r0 = max(min_r, r - h + 1);
            int end_r0 = min(r, max_r - h + 1);
            for (int r0 = start_r0; r0 <= end_r0; ++r0) {
                int next_r = (r < r0 + k) ? (r + k) : (r - k);
                int pen = eval_v_window_penalty(r0, c, h, r);
                if (dist[next_r][c] == -1) {
                    dist[next_r][c] = dist[r][c] + 1;
                    best_cost_to[next_r][c] = pen;
                    parent_cell[next_r][c] = {r, c};
                    parent_op[next_r][c] = Op{'V', r0, c, h, 1};
                    q.push({next_r, c});
                } else if (dist[next_r][c] == dist[r][c] + 1 && pen < best_cost_to[next_r][c]) {
                    best_cost_to[next_r][c] = pen;
                    parent_cell[next_r][c] = {r, c};
                    parent_op[next_r][c] = Op{'V', r0, c, h, 1};
                }
            }
        }
    }

    assert(dist[tr][tc] != -1);
    vector<Op> res_ops;
    int curr_r = tr, curr_c = tc;
    while (!(curr_r == sr && curr_c == sc)) {
        res_ops.push_back(parent_op[curr_r][curr_c]);
        auto [pr, pc] = parent_cell[curr_r][curr_c];
        curr_r = pr;
        curr_c = pc;
    }
    reverse(res_ops.begin(), res_ops.end());
    return res_ops;
}

void compute_graph_bfs(int root_r, int root_c, int rank_arr[20][20]) {
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            rank_arr[r][c] = -1;
        }
    }
    queue<pair<int, int>> q;
    rank_arr[root_r][root_c] = 0;
    q.push({root_r, root_c});

    while (!q.empty()) {
        auto [r, c] = q.front();
        q.pop();

        for (int d = 0; d < 4; ++d) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;
            if (r == nr) {
                int min_c = min(c, nc);
                if (V_wall[r][min_c]) continue;
            } else {
                int min_r = min(r, nr);
                if (H_wall[min_r][c]) continue;
            }
            if (rank_arr[nr][nc] == -1) {
                rank_arr[nr][nc] = rank_arr[r][c] + 1;
                q.push({nr, nc});
            }
        }
    }

    int max_d = 0;
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            max_d = max(max_d, rank_arr[r][c]);
        }
    }
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            rank_arr[r][c] = (max_d - rank_arr[r][c]) * 1000 + r * N + c;
        }
    }
}

void compute_geom_rank(int mode, int rank_arr[20][20]) {
    int inv_r, inv_c;
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            inv_r = N - 1 - r; inv_c = N - 1 - c;
            switch (mode) {
                case 0: rank_arr[r][c] = r * N + c; break;
                case 1: rank_arr[r][c] = r * N + inv_c; break;
                case 2: rank_arr[r][c] = inv_r * N + c; break;
                case 3: rank_arr[r][c] = inv_r * N + inv_c; break;
                case 4: rank_arr[r][c] = c * N + r; break;
                case 5: rank_arr[r][c] = inv_c * N + r; break;
                case 6: rank_arr[r][c] = c * N + inv_r; break;
                case 7: rank_arr[r][c] = inv_c * N + inv_r; break;
                case 8: {
                    int l = min({r, c, inv_r, inv_c});
                    rank_arr[r][c] = l * 1000 + r * N + c;
                    break;
                }
                case 9: {
                    int l = min({r, c, inv_r, inv_c});
                    rank_arr[r][c] = l * 1000 + inv_r * N + inv_c;
                    break;
                }
                case 10: rank_arr[r][c] = (r + c) * 100 + r; break;
                case 11: rank_arr[r][c] = (inv_r + inv_c) * 100 + inv_r; break;
                case 12: rank_arr[r][c] = (r + inv_c) * 100 + r; break;
                case 13: rank_arr[r][c] = (inv_r + c) * 100 + inv_r; break;
            }
        }
    }
}

vector<Op> solve_rank_table(const int rank_arr[20][20]) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            cur_A[i][j] = orig_A[i][j];
            pos_of[cur_A[i][j]] = {i, j};
            locked_cell[i][j] = false;
        }
    }
    cur_ops.clear();

    int remaining = N * N;
    while (remaining > 1) {
        vector<pair<int, int>> non_cut;
        find_articulation_points(non_cut);

        int best_rank = 1e9;
        pair<int, int> best_cell = {-1, -1};

        for (auto cell : non_cut) {
            int rk = rank_arr[cell.first][cell.second];
            if (rk < best_rank) {
                best_rank = rk;
                best_cell = cell;
            }
        }

        int target_card = best_cell.first * N + best_cell.second;
        auto curr_pos = pos_of[target_card];
        vector<Op> best_ops;
        if (curr_pos != best_cell) {
            best_ops = find_op_path(curr_pos.first, curr_pos.second, best_cell.first, best_cell.second);
        }

        for (const auto& op : best_ops) {
            apply_operation(op);
        }

        locked_cell[best_cell.first][best_cell.second] = true;
        --remaining;
    }

    return cur_ops;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    if (!(cin >> N)) return 0;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            cin >> orig_A[i][j];
        }
    }

    for (int i = 0; i < N; ++i) {
        string s;
        cin >> s;
        for (int j = 0; j < N - 1; ++j) {
            V_wall[i][j] = (s[j] == '1');
        }
    }

    for (int i = 0; i < N - 1; ++i) {
        string s;
        cin >> s;
        for (int j = 0; j < N; ++j) {
            H_wall[i][j] = (s[j] == '1');
        }
    }

    vector<Op> best_solution;
    size_t min_ops = 1e9;

    int cur_rank[20][20];

    // 1. All 14 geometric modes
    for (int m = 0; m < 14; ++m) {
        compute_geom_rank(m, cur_rank);
        auto ops = solve_rank_table(cur_rank);
        if (ops.size() < min_ops) {
            min_ops = ops.size();
            best_solution = ops;
        }
    }

    // 2. Comprehensive 7x7 grid of roots for BFS trees
    const int sample_coords[7] = {0, 3, 6, 9, 12, 16, 19};
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 7; ++j) {
            int rr = sample_coords[i], cc = sample_coords[j];
            compute_graph_bfs(rr, cc, cur_rank);
            auto ops = solve_rank_table(cur_rank);
            if (ops.size() < min_ops) {
                min_ops = ops.size();
                best_solution = ops;
            }
        }
    }

    for (const auto& op : best_solution) {
        cout << op.d << " " << op.r << " " << op.c << " " << op.h << " " << op.w << "\n";
    }

    return 0;
}
