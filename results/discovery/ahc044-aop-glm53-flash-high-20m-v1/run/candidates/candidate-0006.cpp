#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

using namespace std;

static const int MX = 100;

struct Graph {
    array<int, MX> a, b;
};

struct Result {
    long long error;
    array<long long, MX> cnt;
};

static Result evaluate(const Graph& g, const array<int, MX>& target, int L) {
    Result res{};
    res.cnt.fill(0);
    array<char, 2 * MX> seen;
    seen.fill(0);
    array<int, 2 * MX> where;
    where.fill(-1);
    array<int, 2 * MX> seqx;
    seqx.fill(-1);
    int x = 0, p = 0; // p is parity of the old visit-count for x.
    int len = 0;
    while (len < L) {
        const int id = 2 * x + p;
        if (seen[id]) break;
        seen[id] = 1;
        where[id] = len;
        seqx[len] = x;
        ++res.cnt[x];
        ++len;
        const int out = p ^ 1;
        const int y = out ? g.b[x] : g.a[x];
        p = res.cnt[y] & 1;
        x = y;
    }
    if (len < L) {
        const int id = 2 * x + p;
        const int pos = where[id];
        const int cyc = len - pos;
        const long long rem = L - len;
        const long long full = rem / cyc;
        const int rest = (int)(rem % cyc);
        if (full) {
            array<long long, MX> add{};
            for (int i = pos; i < len; ++i) ++add[seqx[i]];
            for (int i = 0; i < MX; ++i) res.cnt[i] += full * add[i];
        }
        int xx2 = x, pp2 = p;
        for (int i = 0; i < rest; ++i) {
            ++res.cnt[xx2];
            const int out = pp2 ^ 1;
            const int y = out ? g.b[xx2] : g.a[xx2];
            pp2 = res.cnt[y] & 1;
            xx2 = y;
        }
    }
    long long e = 0;
    for (int i = 0; i < MX; ++i) e += abs(res.cnt[i] - target[i]);
    res.error = e;
    return res;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n, l;
    cin >> n >> l;
    array<int, MX> T{};
    for (int i = 0; i < n; ++i) cin >> T[i];

    const auto started = chrono::steady_clock::now();
    auto elapsed_ms = [&]() {
        return chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - started).count();
    };

    mt19937 rng(0x12345678u ^ (unsigned)T[0] ^ ((unsigned)T[1] << 7) ^ ((unsigned)T[n - 1] << 17));
    auto ri = [&](int lo, int hi) { return uniform_int_distribution<int>(lo, hi)(rng); };

    Graph cur{};
    for (int i = 0; i < n; ++i) {
        cur.a[i] = (i + 1) % n;
        cur.b[i] = (i + 1) % n;
    }
    Result rc = evaluate(cur, T, l);
    Graph best = cur;
    long long eb = rc.error;

    int stuck = 0;
    while (elapsed_ms() < 1650) {
        const int x = ri(0, n - 1);
        const int par = ri(0, 1);
        int oldy = par ? cur.b[x] : cur.a[x];
        int newy = oldy;
        long long ne = rc.error;
        bool found = false;
        for (int y = 0; y < n; ++y) {
            if (y == oldy) continue;
            if (par) cur.b[x] = y; else cur.a[x] = y;
            Result r = evaluate(cur, T, l);
            if (!found || r.error < ne) {
                ne = r.error;
                newy = y;
                found = true;
            }
        }
        if (par) cur.b[x] = newy; else cur.a[x] = newy;
        if (found && ne < rc.error) {
            rc.error = ne;
            // Fill rc.counts with the accepted graph.
            rc = evaluate(cur, T, l);
            stuck = 0;
            if (rc.error < eb) {
                eb = rc.error;
                best = cur;
            }
        } else {
            // No mutation improved; restore the tested edge exactly.
            if (par) cur.b[x] = oldy; else cur.a[x] = oldy;
            rc = evaluate(cur, T, l);
            ++stuck;
            if (stuck >= 4) {
                cur = best;
                int kicks = ri(2, 7);
                for (int k = 0; k < kicks; ++k) {
                    int xx = ri(0, n - 1), pp = ri(0, 1);
                    int yy = ri(0, n - 1);
                    if (pp) cur.b[xx] = yy; else cur.a[xx] = yy;
                }
                rc = evaluate(cur, T, l);
                if (rc.error < eb) {
                    eb = rc.error;
                    best = cur;
                }
                stuck = 0;
            }
        }
    }

    for (int i = 0; i < n; ++i) cout << best.a[i] << ' ' << best.b[i] << '\n';
}
