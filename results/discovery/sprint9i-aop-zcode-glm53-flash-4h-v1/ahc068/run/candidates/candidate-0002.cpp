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

// Min cuts to move position sf -> tg inside a free segment of length len.
static array<array<array<unsigned char, 20>, 20>, 21> cutcost;
static void initCutcost() {
    for (int len = 0; len <= N; len++)
        for (int a = 0; a < len; a++)
            for (int b = 0; b < len; b++) cutcost[len][a][b] = 99;
    for (int len = 1; len <= N; len++) {
        vector<int> cc((size_t)len * len, 99);
        vector<int> dist(len, -1);
        for (int s = 0; s < len; s++) {
            fill(dist.begin(), dist.end(), -1);
            dist[s] = 0;
            vector<int> q{s};
            for (size_t qh = 0; qh < q.size(); qh++) {
                int x = q[qh];
                for (const auto& sh : shapes) {
                    int a = sh[0], d = sh[1];
                    if (a + 2 * d > len) continue;
                    if (x < a || x >= a + 2 * d) continue;
                    int y = (x < a + d) ? (x + d) : (x - d);
                    if (dist[y] < 0) {
                        dist[y] = dist[x] + 1;
                        q.push_back(y);
                    }
                }
            }
            for (int t = 0; t < len; t++) cc[(size_t)s * len + t] = dist[t] < 0 ? 99 : dist[t];
        }
        for (int a = 0; a < len; a++)
            for (int b = 0; b < len; b++) cutcost[len][a][b] = (unsigned char)cc[(size_t)a * len + b];
    }
}

// Move a card along one line using cuts whose segments avoid `blocked`
// positions. vert: false = within row `line` (positions are columns),
// true = within column `line` (positions are rows). Returns cut count or -1.
static int leg(bool vert, int line, int from, int to,
               const array<unsigned char, 20>& blocked, vector<array<int, 2>>& out) {
    out.clear();
    if (from == to) return 0;
    (void)vert;
    (void)line;
    static array<int, 20> dist, parP, parA, parD, que;
    dist.fill(-1);
    dist[from] = 0;
    int qh = 0, qt = 0;
    que[qt++] = from;
    while (qh < qt) {
        int x = que[qh++];
        for (const auto& sh : shapes) {
            int a = sh[0], d = sh[1];
            if (x < a || x >= a + 2 * d) continue;
            bool bad = false;
            for (int t = a; t < a + 2 * d; t++)
                if (blocked[t]) { bad = true; break; }
            if (bad) continue;
            int y = (x < a + d) ? (x + d) : (x - d);
            if (dist[y] < 0) {
                dist[y] = dist[x] + 1;
                parP[y] = x; parA[y] = a; parD[y] = d;
                que[qt++] = y;
            }
        }
    }
    if (dist[to] < 0) return -1;
    for (int y = to; y != from; y = parP[y]) out.push_back({parA[y], parD[y]});
    reverse(out.begin(), out.end());
    return (int)out.size();
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
// weight = min cut count. Returns cell path (src..dst) or empty.
static vector<int> findRoute(int src, int dst, const array<unsigned char, 400>& prot,
                             bool randomized) {
    static array<int, 400> dist;
    static array<int, 400> par;
    dist.fill(INT_MAX);
    dist[src] = 0;
    par[src] = -1;
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<>> pq;
    static array<unsigned char, 400> done;
    done.fill(0);
    pq.push({0, src});
    int jitterBase = randomized ? 4 : 1;
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
    if (dist[dst] >= INT_MAX || !done[dst]) return {};
    vector<int> path;
    for (int c = dst; c >= 0; c = par[c]) path.push_back(c);
    reverse(path.begin(), path.end());
    return path;
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
    vector<int> path = findRoute(v, u, *protOut, rndMode);
    if (path.empty()) { failed = true; return; }
    executeHops(path);
    if (failed) return;
    (*protOut)[u] = 1;
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
        int u = -1;
        for (int x = 0; x < N * N; x++)
            if (!prot[x] && bd.b[x / N][x % N] != x) { u = x; break; }
        if (u < 0) break;
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

    vector<Op> best;
    size_t bestSz = SIZE_MAX;
    auto t0 = chrono::steady_clock::now();
    for (;;) {
        double el = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        bool randomized = el > 0.05;
        if (el > 1.45) break;
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
