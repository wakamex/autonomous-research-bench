#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <cassert>
#include <chrono>

using namespace std;

struct Op {
    char d;
    int r, c, h, w;
};

int N;
int orig_A[20][20];
bool V_wall[20][20]; // V_wall[r][c] between (r,c) and (r,c+1)
bool H_wall[20][20]; // H_wall[r][c] between (r,c) and (r+1,c)

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

vector<Op> find_op_path(int sr, int sc, int tr, int tc) {
    if (sr == tr && sc == tc) return {};

    static int dist[20][20];
    static pair<int, int> parent_cell[20][20];
    static Op parent_op[20][20];

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            dist[r][c] = -1;
        }
    }

    queue<pair<int, int>> q;
    dist[sr][sc] = 0;
    q.push({sr, sc});

    while (!q.empty()) {
        auto [r, c] = q.front();
        q.pop();

        if (r == tr && c == tc) break;

        int min_c = c, max_c = c;
        while (min_c > 0 && !locked_cell[r][min_c - 1] && !V_wall[r][min_c - 1]) {
            min_c--;
        }
        while (max_c + 1 < N && !locked_cell[r][max_c + 1] && !V_wall[r][max_c]) {
            max_c++;
        }

        int seg_len = max_c - min_c + 1;
        for (int w = 2; w <= seg_len; w += 2) {
            int k = w / 2;
            int start_c0 = max(min_c, c - w + 1);
            int end_c0 = min(c, max_c - w + 1);
            for (int c0 = start_c0; c0 <= end_c0; ++c0) {
                int next_c = (c < c0 + k) ? (c + k) : (c - k);
                if (dist[r][next_c] == -1) {
                    dist[r][next_c] = dist[r][c] + 1;
                    parent_cell[r][next_c] = {r, c};
                    parent_op[r][next_c] = Op{'H', r, c0, 1, w};
                    q.push({r, next_c});
                }
            }
        }

        int min_r = r, max_r = r;
        while (min_r > 0 && !locked_cell[min_r - 1][c] && !H_wall[min_r - 1][c]) {
            min_r--;
        }
        while (max_r + 1 < N && !locked_cell[max_r + 1][c] && !H_wall[max_r][c]) {
            max_r++;
        }

        int v_seg_len = max_r - min_r + 1;
        for (int h = 2; h <= v_seg_len; h += 2) {
            int k = h / 2;
            int start_r0 = max(min_r, r - h + 1);
            int end_r0 = min(r, max_r - h + 1);
            for (int r0 = start_r0; r0 <= end_r0; ++r0) {
                int next_r = (r < r0 + k) ? (r + k) : (r - k);
                if (dist[next_r][c] == -1) {
                    dist[next_r][c] = dist[r][c] + 1;
                    parent_cell[next_r][c] = {r, c};
                    parent_op[next_r][c] = Op{'V', r0, c, h, 1};
                    q.push({next_r, c});
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

int rank_cell(int r, int c, int mode) {
    int rr = r, cc = c;
    int inv_r = N - 1 - r, inv_c = N - 1 - c;
    switch (mode) {
        case 0: return r * N + c; // Row TL
        case 1: return r * N + inv_c; // Row TR
        case 2: return inv_r * N + c; // Row BL
        case 3: return inv_r * N + inv_c; // Row BR
        case 4: return c * N + r; // Col TL
        case 5: return inv_c * N + r; // Col TR
        case 6: return c * N + inv_r; // Col BL
        case 7: return inv_c * N + inv_r; // Col BR
        case 8: { // Onion TL
            int l = min({r, c, inv_r, inv_c});
            return l * 1000 + r * N + c;
        }
        case 9: { // Onion BR
            int l = min({r, c, inv_r, inv_c});
            return l * 1000 + inv_r * N + inv_c;
        }
        case 10: return (r + c) * 100 + r; // Diag TL
        case 11: return (inv_r + inv_c) * 100 + inv_r; // Diag BR
        case 12: return (r + inv_c) * 100 + r; // Diag TR
        case 13: return (inv_r + c) * 100 + inv_r; // Diag BL
        default: return r * N + c;
    }
}

vector<Op> solve_mode(int mode) {
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
            int rk = rank_cell(cell.first, cell.second, mode);
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

    for (int m = 0; m < 14; ++m) {
        auto ops = solve_mode(m);
        if (ops.size() < min_ops) {
            min_ops = ops.size();
            best_solution = ops;
        }
    }

    for (const auto& op : best_solution) {
        cout << op.d << " " << op.r << " " << op.c << " " << op.h << " " << op.w << "\n";
    }

    return 0;
}
