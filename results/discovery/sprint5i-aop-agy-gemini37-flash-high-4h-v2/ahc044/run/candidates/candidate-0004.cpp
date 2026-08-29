#include <iostream>
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>

using namespace std;

static inline int simulate_fast(int n, int l, const int* T, const int* a, const int* b, int* counts) {
    for (int i = 0; i < n; ++i) counts[i] = 0;
    int curr = 0;
    for (int step = 0; step < l; ++step) {
        counts[curr]++;
        int t = counts[curr];
        curr = (t & 1) ? a[curr] : b[curr];
    }
    int err = 0;
    for (int i = 0; i < n; ++i) {
        err += abs(counts[i] - T[i]);
    }
    return err;
}

static bool is_strongly_connected(int n, const int* a, const int* b, const int* T) {
    vector<vector<int>> adj(n), radj(n);
    for (int i = 0; i < n; ++i) {
        adj[i].push_back(a[i]);
        adj[i].push_back(b[i]);
        radj[a[i]].push_back(i);
        radj[b[i]].push_back(i);
    }
    vector<bool> vis(n, false);
    vector<int> q = {0};
    vis[0] = true;
    for (size_t i = 0; i < q.size(); ++i) {
        int u = q[i];
        for (int v : adj[u]) {
            if (!vis[v]) {
                vis[v] = true;
                q.push_back(v);
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        if (T[i] > 0 && !vis[i]) return false;
    }
    vector<bool> rvis(n, false);
    vector<int> rq = {0};
    rvis[0] = true;
    for (size_t i = 0; i < rq.size(); ++i) {
        int u = rq[i];
        for (int v : radj[u]) {
            if (!rvis[v]) {
                rvis[v] = true;
                rq.push_back(v);
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        if (T[i] > 0 && !rvis[i]) return false;
    }
    return true;
}

static void repair_connectivity(int n, int* a, int* b, const int* T) {
    for (int i = 0; i < n; ++i) {
        if (a[i] == i && b[i] == i) {
            b[i] = (i + 1) % n;
        }
    }
    while (!is_strongly_connected(n, a, b, T)) {
        vector<vector<int>> adj(n), radj(n);
        for (int i = 0; i < n; ++i) {
            adj[i].push_back(a[i]); adj[i].push_back(b[i]);
            radj[a[i]].push_back(i); radj[b[i]].push_back(i);
        }
        vector<bool> vis(n, false);
        vector<int> q = {0};
        vis[0] = true;
        for (size_t i = 0; i < q.size(); ++i) {
            int u = q[i];
            for (int v : adj[u]) {
                if (!vis[v]) { vis[v] = true; q.push_back(v); }
            }
        }
        int unreachable = -1;
        for (int i = 0; i < n; ++i) {
            if (T[i] > 0 && !vis[i]) { unreachable = i; break; }
        }
        if (unreachable != -1) {
            int src = q[0];
            b[src] = unreachable;
            continue;
        }
        vector<bool> rvis(n, false);
        vector<int> rq = {0};
        rvis[0] = true;
        for (size_t i = 0; i < rq.size(); ++i) {
            int u = rq[i];
            for (int v : radj[u]) {
                if (!rvis[v]) { rvis[v] = true; rq.push_back(v); }
            }
        }
        int cannot_reach = -1;
        for (int i = 0; i < n; ++i) {
            if (T[i] > 0 && !rvis[i]) { cannot_reach = i; break; }
        }
        if (cannot_reach != -1) {
            b[cannot_reach] = 0;
            continue;
        }
        break;
    }
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    auto start_time = chrono::steady_clock::now();

    int n, l;
    if (!(cin >> n >> l)) return 0;

    vector<int> T(n);
    for (int i = 0; i < n; ++i) {
        cin >> T[i];
    }

    mt19937 rng(1337);

    vector<int> edges(2 * n);
    vector<int> in_sum(n, 0);

    for (int i = 0; i < n; ++i) {
        edges[2 * i] = (i + 1) % n;
        edges[2 * i + 1] = (i + 1) % n;
        in_sum[edges[2 * i]] += T[i];
        in_sum[edges[2 * i + 1]] += T[i];
    }

    auto get_cost = [&]() {
        long long cost = 0;
        for (int v = 0; v < n; ++v) cost += abs(in_sum[v] - 2 * T[v]);
        return cost;
    };

    long long curr_cost = get_cost();
    long long best_cost = curr_cost;
    vector<int> best_edges = edges;

    double time_limit_sec = 1.70;
    double sa_time_limit = 1.45;
    double temp_start = 8000.0, temp_end = 0.0001;

    int iter = 0;
    while (true) {
        if ((iter & 0x7FFF) == 0) {
            auto now = chrono::steady_clock::now();
            double elapsed = chrono::duration<double>(now - start_time).count();
            if (elapsed >= sa_time_limit) break;
        }
        iter++;
        auto now = chrono::steady_clock::now();
        double progress = min(1.0, chrono::duration<double>(now - start_time).count() / sa_time_limit);
        double temp = temp_start * pow(temp_end / temp_start, progress);

        int move_type = rng() % 100;
        if (move_type < 35) {
            int k = rng() % (2 * n);
            int old_v = edges[k];
            int new_v = rng() % n;
            if (old_v == new_v) continue;
            int u = k / 2;
            int other_slot = (k % 2 == 0) ? (k + 1) : (k - 1);
            if (new_v == u && edges[other_slot] == u) continue;

            int w = T[u];
            long long old_part = abs(in_sum[old_v] - 2 * T[old_v]) + abs(in_sum[new_v] - 2 * T[new_v]);
            long long new_part = abs(in_sum[old_v] - w - 2 * T[old_v]) + abs(in_sum[new_v] + w - 2 * T[new_v]);
            long long delta = new_part - old_part;

            if (delta < 0 || (delta > 0 && uniform_real_distribution<double>(0, 1)(rng) < exp(-delta / temp))) {
                in_sum[old_v] -= w;
                in_sum[new_v] += w;
                edges[k] = new_v;
                curr_cost += delta;
                if (curr_cost < best_cost) {
                    best_cost = curr_cost;
                    best_edges = edges;
                }
            }
        } else if (move_type < 70) {
            int k1 = rng() % (2 * n), k2 = rng() % (2 * n);
            int v1 = edges[k1], v2 = edges[k2];
            if (v1 == v2) continue;
            int u1 = k1 / 2, u2 = k2 / 2;
            int other1 = (k1 % 2 == 0) ? (k1 + 1) : (k1 - 1);
            int other2 = (k2 % 2 == 0) ? (k2 + 1) : (k2 - 1);
            if (v2 == u1 && edges[other1] == u1) continue;
            if (v1 == u2 && edges[other2] == u2) continue;

            int w1 = T[u1], w2 = T[u2];
            if (w1 == w2) continue;

            long long old_part = abs(in_sum[v1] - 2 * T[v1]) + abs(in_sum[v2] - 2 * T[v2]);
            long long new_part = abs(in_sum[v1] + w2 - w1 - 2 * T[v1]) + abs(in_sum[v2] + w1 - w2 - 2 * T[v2]);
            long long delta = new_part - old_part;

            if (delta < 0 || (delta > 0 && uniform_real_distribution<double>(0, 1)(rng) < exp(-delta / temp))) {
                in_sum[v1] += w2 - w1;
                in_sum[v2] += w1 - w2;
                edges[k1] = v2;
                edges[k2] = v1;
                curr_cost += delta;
                if (curr_cost < best_cost) {
                    best_cost = curr_cost;
                    best_edges = edges;
                }
            }
        } else if (move_type < 85) {
            int k1 = rng() % (2 * n), k2 = rng() % (2 * n), k3 = rng() % (2 * n);
            int v1 = edges[k1], v2 = edges[k2], v3 = edges[k3];
            if (v1 == v2 || v2 == v3 || v3 == v1) continue;
            int w1 = T[k1 / 2], w2 = T[k2 / 2], w3 = T[k3 / 2];

            long long old_part = abs(in_sum[v1] - 2 * T[v1]) + abs(in_sum[v2] - 2 * T[v2]) + abs(in_sum[v3] - 2 * T[v3]);
            long long new_part = abs(in_sum[v1] - w1 + w3 - 2 * T[v1]) + 
                                 abs(in_sum[v2] - w2 + w1 - 2 * T[v2]) + 
                                 abs(in_sum[v3] - w3 + w2 - 2 * T[v3]);
            long long delta = new_part - old_part;

            if (delta < 0 || (delta > 0 && uniform_real_distribution<double>(0, 1)(rng) < exp(-delta / temp))) {
                in_sum[v1] += w3 - w1;
                in_sum[v2] += w1 - w2;
                in_sum[v3] += w2 - w3;
                edges[k1] = v2;
                edges[k2] = v3;
                edges[k3] = v1;
                curr_cost += delta;
                if (curr_cost < best_cost) {
                    best_cost = curr_cost;
                    best_edges = edges;
                }
            }
        } else {
            int k1 = rng() % (2 * n), k2 = rng() % (2 * n);
            if (k1 == k2) continue;
            int v1 = edges[k1];
            if (edges[k2] != v1) continue;
            int k3 = rng() % (2 * n);
            int v2 = edges[k3];
            if (v1 == v2) continue;

            int w1 = T[k1 / 2], w2 = T[k2 / 2], w3 = T[k3 / 2];
            int net_v1 = w3 - (w1 + w2);
            int net_v2 = (w1 + w2) - w3;

            long long old_part = abs(in_sum[v1] - 2 * T[v1]) + abs(in_sum[v2] - 2 * T[v2]);
            long long new_part = abs(in_sum[v1] + net_v1 - 2 * T[v1]) + abs(in_sum[v2] + net_v2 - 2 * T[v2]);
            long long delta = new_part - old_part;

            if (delta < 0 || (delta > 0 && uniform_real_distribution<double>(0, 1)(rng) < exp(-delta / temp))) {
                in_sum[v1] += net_v1;
                in_sum[v2] += net_v2;
                edges[k1] = v2;
                edges[k2] = v2;
                edges[k3] = v1;
                curr_cost += delta;
                if (curr_cost < best_cost) {
                    best_cost = curr_cost;
                    best_edges = edges;
                }
            }
        }
    }

    vector<int> a(n), b(n);
    for (int i = 0; i < n; ++i) {
        a[i] = best_edges[2 * i];
        b[i] = best_edges[2 * i + 1];
    }
    repair_connectivity(n, a.data(), b.data(), T.data());

    // Stage 2: Direct simulation fine tuning
    vector<int> counts(n);
    int best_sim_err = simulate_fast(n, l, T.data(), a.data(), b.data(), counts.data());

    while (true) {
        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration<double>(now - start_time).count();
        if (elapsed >= time_limit_sec) break;

        int u = rng() % n;
        int slot = rng() % 2;
        int old_target = (slot == 0) ? a[u] : b[u];
        int new_target = rng() % n;

        if (rng() % 2 == 0) {
            vector<int> deficits;
            for (int i = 0; i < n; ++i) {
                if (counts[i] < T[i]) deficits.push_back(i);
            }
            if (!deficits.empty()) {
                new_target = deficits[rng() % deficits.size()];
            }
        }

        if (new_target == old_target) continue;
        if (new_target == u && (slot == 0 ? b[u] : a[u]) == u) continue;

        if (slot == 0) a[u] = new_target;
        else b[u] = new_target;

        if (!is_strongly_connected(n, a.data(), b.data(), T.data())) {
            if (slot == 0) a[u] = old_target;
            else b[u] = old_target;
            continue;
        }

        vector<int> cand_counts(n);
        int sim_err = simulate_fast(n, l, T.data(), a.data(), b.data(), cand_counts.data());
        if (sim_err < best_sim_err) {
            best_sim_err = sim_err;
            counts = cand_counts;
        } else {
            if (slot == 0) a[u] = old_target;
            else b[u] = old_target;
        }
    }

    for (int i = 0; i < n; ++i) {
        cout << a[i] << ' ' << b[i] << '\n';
    }

    return 0;
}
