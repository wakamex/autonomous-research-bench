#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <random>
#include <climits>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

using namespace std;

auto begin_time = chrono::steady_clock::now();
int main() {
    constexpr int SZ = 4;
    const array<int, SZ> di{-1, 1, 0, 0};
    const array<int, SZ> dj{0, 0, -1, 1};

    int n, m, k;
    cin >> n >> m >> k;
    vector<int> si(m), sj(m);
    for (int i = 0; i < m; ++i) cin >> si[i] >> sj[i];

    vector<string> vertical(n), horizontal(n - 1);
    for (int i = 0; i < n; ++i) cin >> vertical[i];
    for (int i = 0; i < n - 1; ++i) cin >> horizontal[i];

    auto edge_open = [&](int i, int j, int d) {
        if (d == 0) return i > 0 && horizontal[i - 1][j] == '0';
        if (d == 1) return i + 1 < n && horizontal[i][j] == '0';
        if (d == 2) return j > 0 && vertical[i][j - 1] == '0';
        return j + 1 < n && vertical[i][j] == '0';
    };

    int N = n * n;
    vector<vector<pair<int,int>>> g(N);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            for (int d = 0; d < SZ; ++d) {
                int ni = i + di[d], nj = j + dj[d];
                if (ni >= 0 && ni < n && nj >= 0 && nj < n && edge_open(i, j, d))
                    g[i * n + j].push_back({ni * n + nj, d});
            }
        }
    }

    // Quick exact Hamilton-path search. If it succeeds, no metric detours
    // are needed and the route has the minimum possible 899 moves.
    {
        const auto ham_begin = chrono::steady_clock::now();
        vector<char> visited(N, 0);
        vector<int> path;
        bool found_ham = false;
        auto connected_ok = [&]() {
            vector<char> seen(N, 0);
            queue<int> q;
            q.push(path.back());
            seen[path.back()] = 1;
            int cnt = 1;
            while (!q.empty()) {
                int p = q.front(); q.pop();
                for (auto [np, d] : g[p]) if (!visited[np] && !seen[np]) {
                    seen[np] = 1; ++cnt; q.push(np);
                }
            }
            int need = 0;
            for (int p = 0; p < N; ++p) if (!visited[p]) ++need;
            return cnt - 1 == need;
        };
        function<bool(int)> dfs_ham = [&](int at) {
            if ((int)path.size() == N) return true;
            if (chrono::duration<double>(chrono::steady_clock::now() - ham_begin).count() > 0.12)
                return false;
            if (!connected_ok()) return false;
            vector<pair<int,int>> cand;
            for (auto [np, d] : g[at]) if (!visited[np]) {
                int deg = 0;
                for (auto [nq, d2] : g[np]) if (!visited[nq] || nq == at) ++deg;
                cand.push_back({deg, np});
            }
            sort(cand.begin(), cand.end());
            for (auto [deg, np] : cand) {
                visited[np] = 1; path.push_back(np);
                if (dfs_ham(np)) return true;
                path.pop_back(); visited[np] = 0;
            }
            return false;
        };
        for (int r = 0; r < m && !found_ham; ++r) {
            int st = si[r] * n + sj[r];
            visited.assign(N, 0);
            path.assign(1, st);
            visited[st] = 1;
            if (dfs_ham(st)) {
                found_ham = true;
                vector<int> ham_moves;
                for (int z = 1; z < N; ++z) {
                    int p = path[z - 1], q0 = path[z];
                    int i = p / n, j = p % n, ni = q0 / n, nj = q0 % n;
                    ham_moves.push_back((ni < i) ? 0 : (ni > i) ? 1 : (nj < j) ? 2 : 3);
                }
                for (int b = 0; b < k; ++b) {
                    char c = "UDLRSSSSSS"[b];
                    for (int q0 = 0; q0 < m; ++q0) cout << c << " \n"[q0 + 1 == m];
                }
                for (int d : ham_moves) {
                    int button = (d == 0) ? 0 : (d == 1) ? 1 : (d == 2) ? 2 : 3;
                    cout << button << '\n';
                }
                return 0;
            }
        }
    }

    // Grid-shortest-path distances. Repeated cells are allowed in the final
    // route, so a short cycle in this metric is directly convertible to moves.
    vector<vector<int>> dist(N, vector<int>(N, INT_MAX));
    for (int s = 0; s < N; ++s) {
        queue<int> q;
        q.push(s);
        dist[s][s] = 0;
        while (!q.empty()) {
            int p = q.front(); q.pop();
            for (auto [np, d] : g[p]) {
                if (dist[s][np] > dist[s][p] + 1) {
                    dist[s][np] = dist[s][p] + 1;
                    q.push(np);
                }
            }
        }
    }

    auto optimize_cycle = [&](vector<int>& c) {
        bool imp = true;
        while (imp) {
            imp = false;
            for (int i = 0; i < N && !imp; ++i) {
                int a = c[i], b = c[(i + 1) % N];
                int bj = -1, bd = 0;
                for (int j = i + 2; j < N; ++j) {
                    if (i == 0 && j == N - 1) continue;
                    int cc = c[j], dd = c[(j + 1) % N];
                    int delta = dist[a][cc] + dist[b][dd] - dist[a][b] - dist[cc][dd];
                    if (delta < bd) { bd = delta; bj = j; }
                }
                if (bj >= 0) {
                    for (int l = i + 1, r = bj; l < r; ++l, --r) swap(c[l], c[r]);
                    imp = true;
                }
            }
        }
    };

    auto nearest_neighbor_cycle = [&](int st) {
        vector<char> used(N, 0);
        vector<int> c;
        c.reserve(N);
        int cur = st;
        used[cur] = 1;
        c.push_back(cur);
        for (int z = 1; z < N; ++z) {
            int best = -1, best_dist = INT_MAX;
            for (int p = 0; p < N; ++p) {
                if (!used[p] && dist[cur][p] < best_dist) {
                    best_dist = dist[cur][p];
                    best = p;
                }
            }
            used[best] = 1;
            cur = best;
            c.push_back(cur);
        }
        long long cost = 0;
        for (int z = 0; z < N; ++z) cost += dist[c[z]][c[(z + 1) % N]];
        return make_pair(c, cost);
    };

    long long best_cost = LLONG_MAX;
    vector<int> best_cycle;
    for (int r = 0; r < m; ++r) {
        auto [c, cost] = nearest_neighbor_cycle(si[r] * n + sj[r]);
        optimize_cycle(c);
        cost = 0;
        for (int z = 0; z < N; ++z) cost += dist[c[z]][c[(z + 1) % N]];
        if (cost < best_cost) { best_cost = cost; best_cycle = move(c); }
    }

    mt19937 rng(123456789);
    for (int kick = 0; kick < 200; ++kick) {
        if (chrono::duration<double>(chrono::steady_clock::now() - begin_time).count() > 1.00) break;
        vector<int> c = best_cycle;
        int is[3];
        for (int &x : is) x = 1 + rng() % (N / 2);
        sort(is, is + 3);
        int i = is[0], j = is[1], kk = is[2];
        vector<int> nc;
        nc.reserve(N);
        for (int z = 0; z < i; ++z) nc.push_back(c[z]);
        for (int z = j; z < kk; ++z) nc.push_back(c[z]);
        for (int z = i; z < j; ++z) nc.push_back(c[z]);
        for (int z = kk; z < N; ++z) nc.push_back(c[z]);
        optimize_cycle(nc);
        long long cost = 0;
        for (int z = 0; z < N; ++z) cost += dist[nc[z]][nc[(z + 1) % N]];
        if (cost < best_cost) { best_cost = cost; best_cycle = move(nc); }
    }

    // Choose the active robot and the entry point on the cycle. The cycle's
    // final return edge is omitted after all cells have been visited.
    long long best_len = LLONG_MAX;
    int best_robot = 0, best_start_index = 0;
    for (int z = 0; z < N; ++z) {
        int s = best_cycle[z];
        int prev = best_cycle[(z + N - 1) % N];
        long long scan_len = best_cost - dist[prev][s];
        for (int r = 0; r < m; ++r) {
            long long len = dist[si[r] * n + sj[r]][s] + scan_len;
            if (len < best_len) {
                best_len = len;
                best_robot = r;
                best_start_index = z;
            }
        }
    }

    vector<int> order;
    order.reserve(N);
    for (int z = 0; z < N; ++z)
        order.push_back(best_cycle[(best_start_index + z) % N]);

    auto path_between = [&](int from, int to) {
        if (from == to) return vector<int>{};
        vector<int> par(N, -2);
        queue<int> q;
        q.push(from);
        par[from] = -1;
        while (!q.empty()) {
            int p = q.front(); q.pop();
            if (p == to) break;
            for (auto [np, d] : g[p]) if (par[np] == -2) {
                par[np] = p;
                q.push(np);
            }
        }
        vector<int> cells;
        for (int p = to; p != from; p = par[p]) cells.push_back(p);
        reverse(cells.begin(), cells.end());
        return cells;
    };

    vector<int> moves;
    int cur = si[best_robot] * n + sj[best_robot];
    for (int target : order) {
        for (int p : path_between(cur, target)) {
            int i = cur / n, j = cur % n, ni = p / n, nj = p % n;
            int d = (ni < i) ? 0 : (ni > i) ? 1 : (nj < j) ? 2 : 3;
            moves.push_back(d);
            cur = p;
        }
    }

    for (int b = 0; b < k; ++b) {
        char c = "UDLRSSSSSS"[b];
        for (int r = 0; r < m; ++r) cout << c << " \n"[r + 1 == m];
    }
    for (int d : moves) {
        int button = (d == 0) ? 0 : (d == 1) ? 1 : (d == 2) ? 2 : 3;
        cout << button << '\n';
    }
}
