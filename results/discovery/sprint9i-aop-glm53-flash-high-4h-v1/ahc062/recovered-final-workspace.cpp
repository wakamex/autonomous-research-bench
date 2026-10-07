#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <functional>
#include <vector>

using namespace std;

int main() {
    auto start_time = chrono::steady_clock::now();
    ios::sync_with_stdio(false);
    int n;
    cin >> n;
    vector<long long> a(n * n);
    for (auto& x : a) cin >> x;
    int m = n * n;

    vector<long long> pref((n + 1) * (n + 1));
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j)
        pref[(i + 1) * (n + 1) + j + 1] = a[i * n + j] + pref[i * (n + 1) + j + 1] +
            pref[(i + 1) * (n + 1) + j] - pref[i * (n + 1) + j];
    auto range_sum = [&](int y, int x, int h, int w) {
        return pref[(y + h) * (n + 1) + x + w] - pref[y * (n + 1) + x + w] -
               pref[(y + h) * (n + 1) + x] + pref[y * (n + 1) + x];
    };
    auto cell = [&](int y, int x) { return y * n + x; };
    auto near = [&](int u, int v) {
        return abs(u / n - v / n) <= 1 && abs(u % n - v % n) <= 1;
    };

    auto build = [&](auto&& self, int y, int x, int h, int w, int fs, int fe) -> vector<int> {
        auto fallback = [&](int y, int x, int h, int w, int fs, int fe) {
            int area = h * w;
            if (fs < 0) fs = cell(y, x);
            if (fe < 0) fe = cell(y + h - 1, x + w - 1);
            vector<int> path, used(area, 0);
            auto local = [&](int u, int v) { return abs(u / n - v / n) <= 1 && abs(u % n - v % n) <= 1; };
            function<bool(int, int)> dfs = [&](int cur, int cnt) {
                path.push_back(cur);
                used[cur - cell(y, x)] = 1;
                if (cnt == area) return cur == fe;
                vector<int> nxt;
                int ur = cur / n, uc = cur % n;
                for (int dr = -1; dr <= 1; ++dr) for (int dc = -1; dc <= 1; ++dc) {
                    int r = ur + dr, c = uc + dc, v = cell(r, c);
                    if ((dr || dc) && r >= y && r < y + h && c >= x && c < x + w && !used[v - cell(y, x)])
                        nxt.push_back(v);
                }
                sort(nxt.begin(), nxt.end(), [&](int u, int v) {
                    int du = 0, dv = 0;
                    for (int dr = -1; dr <= 1; ++dr) for (int dc = -1; dc <= 1; ++dc) {
                        int r = u / n + dr, c = u % n + dc;
                        du += r >= y && r < y + h && c >= x && c < x + w && !used[cell(r, c) - cell(y, x)];
                        r = v / n + dr, c = v % n + dc;
                        dv += r >= y && r < y + h && c >= x && c < x + w && !used[cell(r, c) - cell(y, x)];
                    }
                    return du < dv;
                });
                for (int v : nxt) {
                    if (v == fe && cnt + 1 != area) continue;
                    if (dfs(v, cnt + 1)) return true;
                }
                path.pop_back();
                used[cur - cell(y, x)] = 0;
                return false;
            };
            return dfs(fs, 1) ? path : vector<int>{};
        };
        if (h * w <= 12) {
            int k = h * w, sy = cell(y, x);
            int start = fs < 0 ? 0 : (fs / n - y) * w + (fs % n - x);
            int finish = fe < 0 ? k - 1 : (fe / n - y) * w + (fe % n - x);
            vector<vector<char>> dp(1 << k, vector<char>(k, 0));
            vector<vector<short>> par(1 << k, vector<short>(k, -1));
            for (int s = 0; s < k; ++s) if (fs < 0 || s == start) dp[1 << s][s] = 1;
            for (int mask = 1; mask < (1 << k); ++mask) for (int v = 0; v < k; ++v) {
                if (!dp[mask][v]) continue;
                int vr = v / w, vc = v % w;
                for (int dr = -1; dr <= 1; ++dr) for (int dc = -1; dc <= 1; ++dc) {
                    int r = vr + dr, c = vc + dc;
                    if ((!dr && !dc) || r < 0 || r >= h || c < 0 || c >= w) continue;
                    int u = r * w + c;
                    if (!(mask >> u & 1) && !dp[mask | (1 << u)][u]) {
                        dp[mask | (1 << u)][u] = 1;
                        par[mask | (1 << u)][u] = v;
                    }
                }
            }
            int full = (1 << k) - 1, last = -1;
            for (int v = 0; v < k; ++v) if (dp[full][v] && (fe < 0 || v == finish)) last = v;
            if (last < 0) return {};
            vector<int> rev;
            {
                int mask = full, v = last;
                while (v >= 0) {
                    rev.push_back(cell(y + v / w, x + v % w));
                    int p = par[mask][v];
                    mask ^= 1 << v;
                    v = p;
                }
            }
            reverse(rev.begin(), rev.end());
            return rev;
        }

        int hh = (h + 1) / 2, ww = (w + 1) / 2;
        int hs[2] = {hh, h - hh}, ws[2] = {ww, w - ww};
        int ry[2] = {y, y + hh}, rx[2] = {x, x + ww};
        vector<int> ids{0, 1, 2, 3}, best_order;
        array<pair<int, int>, 3> best_trans;
        long long best_score = -1;
        do {
            if (fs >= 0) {
                int q = (fs / n >= ry[1] ? 1 : 0) * 2 + (fs % n >= rx[1] ? 1 : 0);
                if (ids[0] != q) continue;
            }
            if (fe >= 0) {
                int q = (fe / n >= ry[1] ? 1 : 0) * 2 + (fe % n >= rx[1] ? 1 : 0);
                if (ids[3] != q) continue;
            }
            for (int mask = 0; mask < 8; ++mask) {
                array<pair<int, int>, 3> tr;
                bool ok = true;
                for (int z = 0; z < 3; ++z) {
                    int qa = ids[z], qb = ids[z + 1];
                    int dr = (qb >> 1) - (qa >> 1), dc = (qb & 1) - (qa & 1);
                    int ay = ry[qa >> 1], ax = rx[qa & 1], by = ry[qb >> 1], bx = rx[qb & 1];
                    int ar = dr > 0 ? ay + hs[qa >> 1] - 1 : ay;
                    int ac = dc > 0 ? ax + ws[qa & 1] - 1 : ax;
                    int br = dr > 0 ? by : dr < 0 ? by + hs[qb >> 1] - 1 : by;
                    int bc = dc > 0 ? bx : dc < 0 ? bx + ws[qb & 1] - 1 : bx;
                    if (((mask >> z) & 1) && (dr == 0 || dc == 0)) {
                        if (dr == 0) {
                            ar = ay + hs[qa >> 1] - 1;
                            br = by + hs[qb >> 1] - 1;
                        } else {
                            ac = ax + ws[qa & 1] - 1;
                            bc = bx + ws[qb & 1] - 1;
                        }
                    }
                    tr[z] = {cell(ar, ac), cell(br, bc)};
                }
                for (int z = 0; z < 4 && ok; ++z) {
                    int st = z == 0 ? fs : tr[z - 1].second;
                    int en = z == 3 ? fe : tr[z].first;
                    if (st >= 0 && st == en) ok = false;
                }
                if (!ok) continue;
                long long sc = 0;
                for (int z = 0; z < 4; ++z) {
                    int q = ids[z];
                    sc += (z + 1) * range_sum(ry[q >> 1], rx[q & 1], hs[q >> 1], ws[q & 1]);
                }
                if (sc > best_score) best_score = sc, best_order = ids, best_trans = tr;
            }
        } while (next_permutation(ids.begin(), ids.end()));
        if (best_order.empty()) {
            cerr << "NO " << y << ' ' << x << ' ' << h << ' ' << w << ' ' << fs << ' ' << fe << '\n';
            return fallback(y, x, h, w, fs, fe);
        }

        vector<int> res;
        for (int z = 0; z < 4; ++z) {
            int q = best_order[z];
            int st = z == 0 ? fs : best_trans[z - 1].second;
            int en = z == 3 ? fe : best_trans[z].first;
            vector<int> part = self(self, ry[q >> 1], rx[q & 1], hs[q >> 1], ws[q & 1], st, en);
            if (part.empty() || (st >= 0 && part.front() != st) || (en >= 0 && part.back() != en)) {
                return {};
            }
            res.insert(res.end(), part.begin(), part.end());
        }
        return res;
    };

    vector<int> p = build(build, 0, 0, n, n, -1, -1);

    // Improving adjacent transpositions preserve the path when the new
    // neighbouring pairs are also king-adjacent. Repeat until a local
    // optimum or the time budget is reached.
    bool changed = true;
    while (changed) {
        changed = false;
        for (int k = 0; k + 1 < m; ++k) {
            if ((k & 511) == 0 && chrono::duration<double>(chrono::steady_clock::now() - start_time).count() > 2.85) {
                changed = false;
                break;
            }
            int u = p[k], v = p[k + 1];
            if (a[u] <= a[v]) continue;
            if ((k == 0 || near(p[k - 1], v)) && (k + 2 == m || near(u, p[k + 2]))) {
                swap(p[k], p[k + 1]);
                changed = true;
            }
        }
        for (int len = 3; len <= 30 && changed == false; ++len) {
            for (int l = 0, r = len - 1; r < m; ++l, ++r) {
                if ((l & 255) == 0 && chrono::duration<double>(chrono::steady_clock::now() - start_time).count() > 2.85) goto search_done;
                if (l && !near(p[l - 1], p[r])) continue;
                if (r + 1 < m && !near(p[l], p[r + 1])) continue;
                long long delta = 0;
                for (int z = 0; z < len; ++z) delta += 1LL * (l + z) * (a[p[r - z]] - a[p[l + z]]);
                if (delta > 0) {
                    reverse(p.begin() + l, p.begin() + r + 1);
                    changed = true;
                }
            }
        }
    }
    search_done:;

    // Or-opt moves of a single vertex. Candidate insertion positions are
    // generated from the king-neighbours of the moved cell, so every legal
    // reinsertion with a neighbouring predecessor is considered.
    vector<int> pos(m);
    vector<long long> psum(m + 1, 0);
    while (false && chrono::duration<double>(chrono::steady_clock::now() - start_time).count() <= 2.85) {
        bool moved = false;
        for (int i = 0; i < m; ++i) pos[p[i]] = i;
        for (int i = 0; i < m; ++i) psum[i + 1] = psum[i] + a[p[i]];
        for (int k = 0; k < m && !moved; ++k) {
            if ((k & 255) == 0 && chrono::duration<double>(chrono::steady_clock::now() - start_time).count() > 2.85) break;
            int x = p[k];
            if (k && k + 1 < m && !near(p[k - 1], p[k + 1])) continue;
            int best_t = -1;
            long long best_delta = 0;
            auto consider = [&](int t) {
                if (t < 0 || t >= m || t == k || t == k + 1) return;
                int succ = t < k ? p[t] : p[t + 1];
                if (succ == x || !near(x, succ)) return;
                long long shifted = t < k ? psum[k] - psum[t] : psum[t + 1] - psum[k + 1];
                long long delta = 1LL * (t - k) * a[x] + (t < k ? shifted : -shifted);
                if (delta > best_delta) best_delta = delta, best_t = t;
            };
            if (k && near(x, p[0])) consider(0);
            int xr = x / n, xc = x % n;
            for (int dr = -1; dr <= 1; ++dr) for (int dc = -1; dc <= 1; ++dc) {
                int r = xr + dr, c = xc + dc;
                if ((!dr && !dc) || r < 0 || r >= n || c < 0 || c >= n) continue;
                int oldpos = pos[r * n + c];
                if (oldpos == k) continue;
                int q = oldpos - (oldpos > k);
                consider(q + 1);
            }
            if (best_t >= 0) {
                p.erase(p.begin() + k);
                p.insert(p.begin() + best_t, x);
                moved = true;
            }
        }
        if (!moved) break;
    }
    string out;
    out.reserve(m * 8);
    for (int v : p) {
        out += to_string(v / n);
        out += ' ';
        out += to_string(v % n);
        out += '\n';
    }
    cout << out;
}
