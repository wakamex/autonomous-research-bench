// AHC068 - Rectangle Swap
//
// Operation rectangles must be wall-free, so a V/H op on a single column/row
// acts as a "cut": every card in the chosen even-length segment jumps by
// exactly d = half the segment length. Walls split each row/column into
// wall-free intervals; within one interval any repositioning of a card takes
// 1..5 cuts (found by BFS over segments). Moving a card across walls needs a
// detour: hop along a row interval, switch to a column interval at a shared
// cell, and so on.
//
// Algorithm: sweep a protected row-major prefix. For the frontier cell u,
// find the cheapest cell-path from the card assigned to u with Dijkstra,
// where edges are single hops along a shared wall-free interval weighted by
// the min cut count, and protected cells are impassable. Execute the hops
// with prot-aware cut BFS. Every iteration fixes one more prefix cell with
// cuts that never touch protected cells, so the board is solved exactly.
// The remaining time budget goes to randomized restarts; the shortest found
// op sequence is printed.

#include <bits/stdc++.h>
using namespace std;

static int N;
static array<array<int, 20>, 20> startB;
static array<array<unsigned char, 20>, 20> wallV;  // between (i,j),(i,j+1)
static array<array<unsigned char, 20>, 20> wallH;  // between (i,j),(i+1,j)

struct Op {
    char d;
    int r, c, h, w;
};

static uint64_t rngState = 0x9E3779B97F4A7C15ull;
static uint64_t rngNext() {
    rngState += 0x9E3779B97F4A7C15ull;
    uint64_t z = rngState;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static int rngInt(int n) { return (int)(rngNext() % (uint64_t)n); }

struct Board {
    array<array<int, 20>, 20> b;
    array<int, 400> posOf;
    void init() {
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++) posOf[b[i][j]] = i * N + j;
    }
    void swapCells(int c1, int c2) {
        int k1 = b[c1 / N][c1 % N], k2 = b[c2 / N][c2 % N];
        b[c1 / N][c1 % N] = k2;
        b[c2 / N][c2 % N] = k1;
        posOf[k1] = c2;
        posOf[k2] = c1;
    }
    void applyV(int r, int c, int h, int w) {
        int hh = h / 2;
        for (int j = c; j < c + w; j++)
            for (int x = r; x < r + hh; x++) swapCells(x * N + j, (x + hh) * N + j);
    }
    void applyH(int r, int c, int h, int w) {
        int ww = w / 2;
        for (int x = r; x < r + h; x++)
            for (int y = c; y < c + ww; y++) swapCells(x * N + y, x * N + (y + ww));
    }
};

static bool rectOK(int r, int c, int h, int w) {
    if (r < 0 || c < 0 || r + h > N || c + w > N) return false;
    for (int i = r; i < r + h; i++)
        for (int j = c; j < c + w - 1; j++)
            if (wallV[i][j]) return false;
    for (int i = r; i < r + h - 1; i++)
        for (int j = c; j < c + w; j++)
            if (wallH[i][j]) return false;
    return true;
}

static vector<array<int, 2>> shapes;  // (a, d): segment [a, a+2d)
static void initShapes() {
    shapes.clear();
    for (int d = 1; 2 * d <= N; d++)
        for (int a = 0; a + 2 * d <= N; a++) shapes.push_back({a, d});
}

// Min weighted cost to move position sf -> tg inside a free segment of
// length len, where a cut with half-length d costs WA + WB*d.
static int WA = 4, WB = 8;
static int KW = 4;   // candidate window: fix the cheapest of the next KW cells
static int JB = 2;   // restart jitter base
static int HON = 0;  // harvest pass enabled (net negative in tests)
static int BUDGET = 1450;  // time budget in ms
static int UMODE = 0;      // 0: first-incorrect, 1: random incorrect
static array<array<array<int, 20>, 20>, 21> cutcost;
static void initCutcost() {
    for (int len = 0; len <= N; len++)
        for (int a = 0; a < len; a++)
            for (int b = 0; b < len; b++) cutcost[len][a][b] = 1 << 20;
    for (int len = 1; len <= N; len++) {
        vector<int> cc((size_t)len * len, 1 << 20);
        vector<int> dist(len, -1);
        for (int s = 0; s < len; s++) {
            // Dijkstra with weights (WA + WB*d); small n, use simple loop
            vector<int> wd(len, 1 << 20);
            wd[s] = 0;
            vector<unsigned char> done(len, 0);
            for (;;) {
                int x = -1, best = 1 << 20;
                for (int t = 0; t < len; t++)
                    if (!done[t] && wd[t] < best) { best = wd[t]; x = t; }
                if (x < 0) break;
                done[x] = 1;
                for (const auto& sh : shapes) {
                    int a = sh[0], d = sh[1];
                    if (a + 2 * d > len) continue;
                    if (x < a || x >= a + 2 * d) continue;
                    int y = (x < a + d) ? (x + d) : (x - d);
                    int w = WA + WB * d;
                    if (wd[y] > wd[x] + w) wd[y] = wd[x] + w;
                }
            }
            for (int t = 0; t < len; t++) cc[(size_t)s * len + t] = wd[t];
        }
        for (int a = 0; a < len; a++)
            for (int b = 0; b < len; b++) cutcost[len][a][b] = cc[(size_t)a * len + b];
    }
}

// Move a card along one line using cuts whose segments avoid `blocked`
// positions, minimizing the same weighted cost as cutcost. Returns the cut
// count (or -1 if unreachable); out holds the (a, d) cuts in order.
static int leg(bool vert, int line, int from, int to,
               const array<unsigned char, 20>& blocked, vector<array<int, 2>>& out) {
    out.clear();
    if (from == to) return 0;
    (void)vert;
    (void)line;
    static array<int, 20> wd, parP, parA, parD;
    static array<unsigned char, 20> done;
    wd.fill(1 << 20);
    wd[from] = 0;
    done.fill(0);
    parP.fill(-1);
    for (;;) {
        int x = -1, best = 1 << 20;
        for (int t = 0; t < N; t++)
            if (!done[t] && wd[t] < best) { best = wd[t]; x = t; }
        if (x < 0) break;
        done[x] = 1;
        for (const auto& sh : shapes) {
            int a = sh[0], d = sh[1];
            if (x < a || x >= a + 2 * d) continue;
            bool bad = false;
            for (int t = a; t < a + 2 * d; t++)
                if (blocked[t]) { bad = true; break; }
            if (bad) continue;
            int y = (x < a + d) ? (x + d) : (x - d);
            int w = WA + WB * d;
            if (wd[y] > wd[x] + w) {
                wd[y] = wd[x] + w;
                parP[y] = x; parA[y] = a; parD[y] = d;
            }
        }
    }
    if (wd[to] >= 1 << 20) return -1;
    int cnt = 0;
    for (int y = to; y != from; y = parP[y]) { out.push_back({parA[y], parD[y]}); cnt++; }
    reverse(out.begin(), out.end());
    return cnt;
}

// Maximal wall-free interval of row y containing x: [rowLo, rowHi)
static array<array<int, 20>, 20> rowLo, rowHi, colLo, colHi;
static void initIntervals() {
    for (int y = 0; y < N; y++) {
        int lo = 0;
        for (int x = 0; x < N; x++) {
            rowLo[y][x] = lo;
            rowHi[y][x] = N;
        }
        for (int x = 0; x < N; x++) {
            if (x + 1 < N && wallV[y][x]) {
                for (int t = lo; t <= x; t++) rowHi[y][t] = x + 1;
                lo = x + 1;
                for (int t = x + 1; t < N; t++) rowLo[y][t] = lo;
            }
        }
    }
    for (int x = 0; x < N; x++) {
        int lo = 0;
        for (int y = 0; y < N; y++) {
            colLo[y][x] = lo;
            colHi[y][x] = N;
        }
        for (int y = 0; y < N; y++) {
            if (y + 1 < N && wallH[y][x]) {
                for (int t = lo; t <= y; t++) colHi[t][x] = y + 1;
                lo = y + 1;
                for (int t = y + 1; t < N; t++) colLo[t][x] = lo;
            }
        }
    }
}

// Dijkstra over cells; edges = hop within a shared wall-free, prot-free run,
// weight = min cut cost. Returns cost; path (src..dst) or empty if no route.
static int routeDijkstra(int src, int dst, const array<unsigned char, 400>& prot,
                         bool randomized, vector<int>* pathOut) {
    static array<int, 400> dist;
    static array<int, 400> par;
    dist.fill(INT_MAX);
    dist[src] = 0;
    par[src] = -1;
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<>> pq;
    static array<unsigned char, 400> done;
    done.fill(0);
    pq.push({0, src});
    int jitterBase = randomized ? JB : 1;
    while (!pq.empty()) {
        auto [d, c] = pq.top();
        pq.pop();
        if (done[c]) continue;
        done[c] = 1;
        if (c == dst) break;
        int y = c / N, x = c % N;
        // row hops: shrink the wall interval by prot cells first, so edge
        // costs match exactly what leg() can execute later
        {
            int lo = rowLo[y][x], hi = rowHi[y][x];
            int t = x;
            while (t > lo && !prot[y * N + (t - 1)]) t--;
            lo = t;
            t = x;
            while (t < hi - 1 && !prot[y * N + (t + 1)]) t++;
            hi = t + 1;
            int len = hi - lo;
            for (int q2 = lo; q2 < hi; q2++) {
                if (q2 == x) continue;
                int w = cutcost[len][x - lo][q2 - lo];
                int nd = dist[c] + w * jitterBase + (randomized ? rngInt(jitterBase) : 0);
                if (nd < dist[y * N + q2]) {
                    dist[y * N + q2] = nd;
                    par[y * N + q2] = c;
                    pq.push({nd, y * N + q2});
                }
            }
        }
        // column hops
        {
            int lo = colLo[y][x], hi = colHi[y][x];
            int t = y;
            while (t > lo && !prot[(t - 1) * N + x]) t--;
            lo = t;
            t = y;
            while (t < hi - 1 && !prot[(t + 1) * N + x]) t++;
            hi = t + 1;
            int len = hi - lo;
            for (int q2 = lo; q2 < hi; q2++) {
                if (q2 == y) continue;
                int w = cutcost[len][y - lo][q2 - lo];
                int nd = dist[c] + w * jitterBase + (randomized ? rngInt(jitterBase) : 0);
                if (nd < dist[q2 * N + x]) {
                    dist[q2 * N + x] = nd;
                    par[q2 * N + x] = c;
                    pq.push({nd, q2 * N + x});
                }
            }
        }
    }
    if (dist[dst] >= INT_MAX || !done[dst]) return -1;
    if (pathOut) {
        pathOut->clear();
        for (int c = dst; c >= 0; c = par[c]) pathOut->push_back(c);
        reverse(pathOut->begin(), pathOut->end());
    }
    return dist[dst];
}

// Adaptive construction: repeatedly take the first incorrect unprotected cell
// u. Before fixing+protecting u, verify that removing u from the unprotected
// graph still leaves all other incorrect cells in one connected component;
// if not, recursively fix+protect a starved cell first (deepest pocket cells
// get filled while their corridor is still open). Each call protects exactly
// one cell, so the construction terminates.
static Board bd;
static vector<Op>* opsOut;
static array<unsigned char, 400>* protOut;
static bool rndMode;
static bool failed;
static array<unsigned char, 20> blk;
static vector<array<int, 2>> cuts;

static void processCell(int u);

// If removing u splits incorrect cells across components, return a starved
// cell to process first; else -1.
static int starvedCell(int u) {
    static array<int, 400> comp;
    static array<int, 400> q;
    comp.fill(-1);
    int ncomp = 0;
    for (int s = 0; s < N * N; s++) {
        if ((*protOut)[s] || s == u || comp[s] >= 0) continue;
        int qh = 0, qt = 0;
        q[qt++] = s;
        comp[s] = ncomp;
        while (qh < qt) {
            int x = q[qh++];
            int y = x / N, xx = x % N;
            auto tryVisit = [&](int nx) {
                if (!(*protOut)[nx] && nx != u && comp[nx] < 0) { comp[nx] = ncomp; q[qt++] = nx; }
            };
            if (xx + 1 < N && !wallV[y][xx]) tryVisit(x + 1);
            if (xx > 0 && !wallV[y][xx - 1]) tryVisit(x - 1);
            if (y + 1 < N && !wallH[y][xx]) tryVisit(x + N);
            if (y > 0 && !wallH[y - 1][xx]) tryVisit(x - N);
        }
        ncomp++;
    }
    static array<int, 16> cnt, firstCell;
    if (ncomp > 16) return -1;  // never happens; be safe
    cnt.fill(0);
    firstCell.fill(-1);
    for (int x = 0; x < N * N; x++) {
        if ((*protOut)[x] || x == u || comp[x] < 0) continue;
        // all unprotected cells must stay mutually reachable: routes may cross
        // correct cells, so a sealed-off correct cell would become a landmine
        int c = comp[x];
        cnt[c]++;
        if (firstCell[c] < 0) firstCell[c] = x;
    }
    int mainComp = -1, mainCnt = 0, nWith = 0;
    for (int c = 0; c < ncomp; c++)
        if (cnt[c] > 0) {
            nWith++;
            if (cnt[c] > mainCnt) { mainCnt = cnt[c]; mainComp = c; }
        }
    if (nWith <= 1) return -1;
    for (int c = 0; c < ncomp; c++)
        if (cnt[c] > 0 && c != mainComp) return firstCell[c];
    return -1;
}

static void executeHops(const vector<int>& path) {
    for (size_t pi = 0; pi + 1 < path.size() && !failed; pi++) {
        int c1 = path[pi], c2 = path[pi + 1];
        int y1 = c1 / N, x1 = c1 % N, y2 = c2 / N, x2 = c2 % N;
        if (y1 == y2) {
            int lo = rowLo[y1][x1], hi = rowHi[y1][x1];
            for (int t = 0; t < N; t++)
                blk[t] = (t < lo || t >= hi || (*protOut)[y1 * N + t]) ? 1 : 0;
            int nc = leg(false, y1, x1, x2, blk, cuts);
            if (nc < 0) { failed = true; return; }
            for (auto& cd : cuts) {
                if (!rectOK(y1, cd[0], 1, 2 * cd[1])) { failed = true; return; }
                bd.applyH(y1, cd[0], 1, 2 * cd[1]);
                opsOut->push_back({'H', y1, cd[0], 1, 2 * cd[1]});
            }
        } else {
            int lo = colLo[y1][x1], hi = colHi[y1][x1];
            for (int t = 0; t < N; t++)
                blk[t] = (t < lo || t >= hi || (*protOut)[t * N + x1]) ? 1 : 0;
            int nc = leg(true, x1, y1, y2, blk, cuts);
            if (nc < 0) { failed = true; return; }
            for (auto& cd : cuts) {
                if (!rectOK(cd[0], x1, 2 * cd[1], 1)) { failed = true; return; }
                bd.applyV(cd[0], x1, 2 * cd[1], 1);
                opsOut->push_back({'V', cd[0], x1, 2 * cd[1], 1});
            }
        }
    }
}

static void processCell(int u) {
    if (failed) return;
    for (int guard = 0; guard < 600; guard++) {
        int k = starvedCell(u);
        if (k < 0) break;
        processCell(k);
        if (failed) return;
    }
    int v = bd.posOf[u];
    vector<int> path;
    routeDijkstra(v, u, *protOut, rndMode, &path);
    if (path.empty()) { failed = true; return; }
    executeHops(path);
    if (failed) return;
    (*protOut)[u] = 1;
}

// One cut with half-length d placed at (a) exchanges cells (t) <-> (t+d).
// A "harvest" cut fixes >= 2 cards at once when cards already sit in their
// target cell's partner position. Apply the best such cut while its net gain
// (cards fixed minus cards broken) is at least 2.
static void harvestPass() {
    for (;;) {
        int bestGain = 1;
        int ba = -1, bd2 = -1, bLine = -1;
        bool bVert = false;
        for (int p = 0; p < N * N; p++) {
            if ((*protOut)[p]) continue;
            if (bd.b[p / N][p % N] == p) continue;
            int v = bd.posOf[p];
            if (v == p) continue;
            int py = p / N, px = p % N, vy = v / N, vx = v % N;
            int d, lo, hi;
            bool vert;
            if (py == vy) { d = abs(px - vx); lo = rowLo[py][px]; hi = rowHi[py][px]; vert = false; }
            else if (px == vx) { d = abs(py - vy); lo = colLo[py][px]; hi = colHi[py][px]; vert = true; }
            else continue;
            if (d == 0) continue;
            int mn = vert ? min(py, vy) : min(px, vx);
            int mx = vert ? max(py, vy) : max(px, vx);
            int aLo = max({lo, mn - d + 1, mx - 2 * d + 1});
            int aHi = min({hi - 2 * d, mn, mx - d});
            for (int a = aLo; a <= aHi; a++) {
                bool bad = false;
                for (int t = a; t < a + 2 * d && !bad; t++)
                    if ((*protOut)[vert ? (t * N + px) : (py * N + t)]) bad = true;
                if (bad) continue;
                int gain = 0;
                for (int t = a; t < a + 2 * d; t++) {
                    int pr = (t < a + d) ? t + d : t - d;
                    int ct = vert ? (t * N + px) : (py * N + t);
                    int cp = vert ? (pr * N + px) : (py * N + pr);
                    bool becomes = bd.b[cp / N][cp % N] == ct;
                    bool broken = bd.b[ct / N][ct % N] == ct;
                    gain += (becomes ? 1 : 0) - (broken ? 1 : 0);
                }
                if (gain > bestGain) { bestGain = gain; ba = a; bd2 = d; bLine = vert ? px : py; bVert = vert; }
            }
        }
        if (ba < 0) break;
        if (bVert) {
            bd.applyV(ba, bLine, 2 * bd2, 1);
            opsOut->push_back({'V', ba, bLine, 2 * bd2, 1});
        } else {
            bd.applyH(bLine, ba, 1, 2 * bd2);
            opsOut->push_back({'H', bLine, ba, 1, 2 * bd2});
        }
    }
}


static vector<Op> construct(bool randomized) {
    bd.b = startB;
    bd.init();
    vector<Op> ops;
    ops.reserve(8192);
    static array<unsigned char, 400> prot;
    prot.fill(0);
    opsOut = &ops;
    protOut = &prot;
    rndMode = randomized;
    failed = false;

    for (int calls = 0; calls < 4 * N * N + 16; calls++) {
        if (HON) harvestPass();
        // gather the next few incorrect unprotected cells
        int cand[8];
        int nc = 0;
        for (int x = 0; x < N * N && nc < KW; x++)
            if (!prot[x] && bd.b[x / N][x % N] != x) cand[nc++] = x;
        if (nc == 0) break;
        int u = cand[0];
        if (nc > 1) {
            int bestC = INT_MAX;
            for (int i = 0; i < nc; i++) {
                int c = routeDijkstra(bd.posOf[cand[i]], cand[i], prot, randomized, nullptr);
                if (c >= 0 && c < bestC) { bestC = c; u = cand[i]; }
            }
        }
        processCell(u);
        if (failed) break;
    }
    return ops;
}

int main() {
    if (scanf("%d", &N) != 1) return 0;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) scanf("%d", &startB[i][j]);
    {
        char buf[64];
        for (int i = 0; i < N; i++) {
            scanf("%s", buf);
            for (int j = 0; j < N - 1; j++) wallV[i][j] = buf[j] == '1';
        }
        for (int i = 0; i < N - 1; i++) {
            scanf("%s", buf);
            for (int j = 0; j < N; j++) wallH[i][j] = buf[j] == '1';
        }
    }
    initShapes();
    initCutcost();
    initIntervals();
    if (getenv("AHC_A")) WA = atoi(getenv("AHC_A"));
    if (getenv("AHC_B")) WB = atoi(getenv("AHC_B"));
    if (getenv("AHC_K")) KW = max(1, atoi(getenv("AHC_K")));
    if (getenv("AHC_H")) HON = atoi(getenv("AHC_H"));
    if (getenv("AHC_J")) JB = max(1, atoi(getenv("AHC_J")));
    if (getenv("AHC_T")) BUDGET = atoi(getenv("AHC_T"));
    if (getenv("AHC_U")) UMODE = atoi(getenv("AHC_U"));

    vector<Op> best;
    size_t bestSz = SIZE_MAX;
    auto t0 = chrono::steady_clock::now();
    for (;;) {
        double el = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        bool randomized = el > 0.05;
        if (el > BUDGET / 1000.0) break;
        auto ops = construct(randomized);
        if (ops.size() < bestSz) {
            bestSz = ops.size();
            best = move(ops);
        }
    }

    string out;
    out.reserve(best.size() * 16);
    for (auto& op : best) {
        out += op.d;
        out += ' ';
        out += to_string(op.r);
        out += ' ';
        out += to_string(op.c);
        out += ' ';
        out += to_string(op.h);
        out += ' ';
        out += to_string(op.w);
        out += '\n';
    }
    fwrite(out.data(), 1, out.size(), stdout);
    return 0;
}
