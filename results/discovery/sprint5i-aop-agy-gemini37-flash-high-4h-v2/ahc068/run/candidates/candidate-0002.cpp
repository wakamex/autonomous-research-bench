#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <cassert>

using namespace std;

struct Op {
    char d;
    int r, c, h, w;
};

int N;
int A[20][20];
bool V_wall[20][20]; // V_wall[r][c] between (r,c) and (r,c+1)
bool H_wall[20][20]; // H_wall[r][c] between (r,c) and (r+1,c)
bool locked_cell[20][20];
pair<int, int> pos_of[400];
vector<Op> ops;

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

void do_swap(int r1, int c1, int r2, int c2) {
    Op op;
    if (r1 == r2) {
        op.d = 'H';
        op.r = r1;
        op.c = min(c1, c2);
        op.h = 1;
        op.w = 2;
    } else {
        op.d = 'V';
        op.r = min(r1, r2);
        op.c = c1;
        op.h = 2;
        op.w = 1;
    }
    ops.push_back(op);

    int v1 = A[r1][c1];
    int v2 = A[r2][c2];
    A[r1][c1] = v2;
    A[r2][c2] = v1;
    pos_of[v2] = {r1, c1};
    pos_of[v1] = {r2, c2};
}

// Tarjan's algorithm to find articulation points among unlocked cells
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

// Find shortest path in unlocked cells from (sr, sc) to (tr, tc)
vector<pair<int, int>> find_path(int sr, int sc, int tr, int tc) {
    if (sr == tr && sc == tc) return {{sr, sc}};
    static int dist[20][20];
    static pair<int, int> parent[20][20];
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

        for (int d = 0; d < 4; ++d) {
            int nr = r + dr[d], nc = c + dc[d];
            if (can_step(r, c, nr, nc) && dist[nr][nc] == -1) {
                dist[nr][nc] = dist[r][c] + 1;
                parent[nr][nc] = {r, c};
                q.push({nr, nc});
            }
        }
    }

    assert(dist[tr][tc] != -1);
    vector<pair<int, int>> path;
    int curr_r = tr, curr_c = tc;
    while (!(curr_r == sr && curr_c == sc)) {
        path.push_back({curr_r, curr_c});
        auto [pr, pc] = parent[curr_r][curr_c];
        curr_r = pr;
        curr_c = pc;
    }
    path.push_back({sr, sc});
    reverse(path.begin(), path.end());
    return path;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    if (!(cin >> N)) return 0;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            cin >> A[i][j];
            pos_of[A[i][j]] = {i, j};
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

    int remaining = N * N;
    while (remaining > 1) {
        vector<pair<int, int>> non_cut;
        find_articulation_points(non_cut);

        // Find candidate with minimal distance
        int best_dist = 1e9;
        pair<int, int> best_cell = {-1, -1};
        vector<pair<int, int>> best_path;

        for (auto cell : non_cut) {
            int target_card = cell.first * N + cell.second;
            auto curr_pos = pos_of[target_card];
            if (curr_pos == cell) {
                best_dist = 0;
                best_cell = cell;
                best_path = {cell};
                break;
            }
            auto path = find_path(curr_pos.first, curr_pos.second, cell.first, cell.second);
            int d = (int)path.size() - 1;
            if (d < best_dist) {
                best_dist = d;
                best_cell = cell;
                best_path = path;
            }
        }

        // Apply path swaps
        for (size_t i = 0; i + 1 < best_path.size(); ++i) {
            do_swap(best_path[i].first, best_path[i].second, best_path[i+1].first, best_path[i+1].second);
        }

        locked_cell[best_cell.first][best_cell.second] = true;
        --remaining;
    }

    for (const auto& op : ops) {
        cout << op.d << " " << op.r << " " << op.c << " " << op.h << " " << op.w << "\n";
    }

    return 0;
}
