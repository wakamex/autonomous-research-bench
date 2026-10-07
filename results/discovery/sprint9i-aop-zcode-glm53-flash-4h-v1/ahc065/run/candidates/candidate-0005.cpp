// AHC065 Conveyor Design
//
// Strategy: two Hamiltonian cycles (row-snake + column-snake), so every cell
// lies on both belts (max 2 belts/cell). Every operation moves every box one
// step, so the minimum ops to fetch a box to the exit equals its shortest-path
// distance in the union graph. We fetch successive minima along shortest
// paths; among the (belt, dir) moves that advance the current minimum, we
// pick the one whose side effects move the other remaining boxes furthest
// toward the exit (weighted toward soon-to-be-fetched boxes), with a little
// lookahead and randomized tie-breaking (best of R runs).

#include <bits/stdc++.h>
#include <cassert>
using namespace std;

static const int N = 20;
static const int NN = N * N;
static inline int ID(int i, int j) { return i * N + j; }
static const int EXIT = ID(0, N / 2);

#ifndef WEIGHT_P
#define WEIGHT_P 2.0
#endif
#ifndef LOOK2_M
#define LOOK2_M 1000
#endif
#ifndef LDEPTH
#define LDEPTH 8
#endif
#ifndef NREST
#define NREST 1
#endif

struct Belt {
    vector<int> seq;      // cells in cycle order
    vector<int> idx;      // idx[cell] = position in seq
};

static Belt belts[2];
static vector<int> dist; // static union-graph distance to exit

static void buildBelts() {
    // Belt A: row snake, return spine along column 0.
    {
        vector<int>& s = belts[0].seq;
        s.push_back(ID(0, 0));
        for (int j = 1; j < N; j++) s.push_back(ID(0, j));
        for (int r = 1; r < N; r++) {
            if (r & 1) { for (int j = N - 1; j >= 1; j--) s.push_back(ID(r, j)); }
            else       { for (int j = 1; j <= N - 1; j++) s.push_back(ID(r, j)); }
        }
        s.push_back(ID(N - 1, 0));
        for (int r = N - 2; r >= 1; r--) s.push_back(ID(r, 0));
    }
    // Belt B: column snake with return spine along row 0, mirrored vertically
    // so the spine sits on the bottom row (far from the exit).
    {
        vector<int> s;
        s.push_back(ID(0, 0));
        for (int i = 1; i < N; i++) s.push_back(ID(i, 0));
        for (int c = 1; c < N; c++) {
            if (c & 1) { for (int i = N - 1; i >= 1; i--) s.push_back(ID(i, c)); }
            else       { for (int i = 1; i <= N - 1; i++) s.push_back(ID(i, c)); }
        }
        s.push_back(ID(0, N - 1));
        for (int c = N - 2; c >= 1; c--) s.push_back(ID(0, c));
        vector<int> t;
        t.reserve(s.size());
        for (int v : s) t.push_back(ID(N - 1 - v / N, v % N));
        belts[1].seq = t;
    }
    for (int m = 0; m < 2; m++) {
        belts[m].idx.assign(NN, -1);
        const vector<int>& s = belts[m].seq;
        assert((int)s.size() == NN);
        for (int x = 0; x < NN; x++) {
            assert(belts[m].idx[s[x]] == -1); // distinct cells
            belts[m].idx[s[x]] = x;
        }
        for (int x = 0; x < NN; x++) { // adjacency incl. wrap-around
            int c = s[x], d = s[(x + 1) % NN];
            int di = abs(c / N - d / N), dj = abs(c % N - d % N);
            assert(di + dj == 1);
        }
    }
}

static void computeDist() {
    dist.assign(NN, -1);
    queue<int> q;
    dist[EXIT] = 0;
    q.push(EXIT);
    auto rel = [&](int c, int d) {
        if (d >= 0 && dist[d] < 0) { dist[d] = dist[c] + 1; q.push(d); }
    };
    while (!q.empty()) {
        int c = q.front(); q.pop();
        for (int m = 0; m < 2; m++) {
            int x = belts[m].idx[c];
            rel(c, belts[m].seq[(x + 1) % NN]);
            rel(c, belts[m].seq[(x + NN - 1) % NN]);
        }
    }
    for (int c = 0; c < NN; c++) assert(dist[c] >= 0);
}

// One deterministic solve pass; returns the op sequence.
struct Solver {
    vector<int> boxAt, posOf;
    int curMin;
    double weightP = WEIGHT_P;
    mt19937 rng;
    vector<pair<int, int>> ops;
    int probeDepth = 0; // rotations while probing are not recorded

    void init(const vector<int>& a, unsigned seed) {
        boxAt = a;
        posOf.assign(NN, -1);
        for (int c = 0; c < NN; c++) posOf[boxAt[c]] = c;
        curMin = 0;
        ops.clear();
        if (boxAt[EXIT] == 0) { boxAt[EXIT] = -1; posOf[0] = -1; curMin = 1; }
        rng.seed(seed);
    }

    void rotate(int m, int d) {
        if (probeDepth == 0) ops.push_back({m, d});
        const vector<int>& s = belts[m].seq;
        static vector<int> occ;
        occ.resize(NN);
        for (int x = 0; x < NN; x++) occ[x] = boxAt[s[x]];
        for (int x = 0; x < NN; x++) {
            int b = occ[(x - d + NN) % NN];
            boxAt[s[x]] = b;
            if (b >= 0) posOf[b] = s[x];
        }
    }

    double weight(int b) const {
        int gap = b - curMin;
        return pow(1.0 + gap, -weightP);
    }

    // Per-fetch weight table (weight depends on box - curMin only).
    vector<double> wgt;

    // Weighted progress of all remaining boxes toward the exit.
    double gain(int m, int d) const {
        const vector<int>& s = belts[m].seq;
        const double* w = wgt.data();
        const int* box = boxAt.data();
        const int* ds = dist.data();
        double g = 0;
        for (int x = 0; x < NN; x++) {
            int b = box[s[x]];
            if (b < 0) continue;
            int nxt = s[(x + d + NN) % NN];
            g += w[b] * (ds[s[x]] - ds[nxt]);
        }
        return g;
    }

    void descentMoves(int c, vector<tuple<int, int, int>>& out) const {
        out.clear();
        for (int m = 0; m < 2; m++) {
            int x = belts[m].idx[c];
            int p = belts[m].seq[(x + 1) % NN];
            int q = belts[m].seq[(x + NN - 1) % NN];
            if (dist[p] == dist[c] - 1) out.push_back({m, 1, p});
            if (dist[q] == dist[c] - 1) out.push_back({m, -1, q});
        }
    }

    // Best achievable sum of gains over a depth-d lookahead of descent moves.
    // Probes rotate/restore without recording ops. Scratch is per-depth.
    vector<vector<tuple<int, int, int>>> lvCand;
    vector<vector<int>> lvBox, lvPos;

    double lookValue(int c, int depth) {
        if (depth == 0) return 0;
        descentMoves(c, lvCand[depth]);
        if (lvCand[depth].empty()) return 0;
        double best = -1e18;
        for (auto& [m, d, nc] : lvCand[depth]) {
            double g1 = gain(m, d); // hypothetical gain, pre-rotation state
            lvBox[depth] = boxAt;
            lvPos[depth] = posOf;
            probeDepth++;
            rotate(m, d);
            double v = g1
                     + (LOOK2_M / 1000.0) * (nc == EXIT ? 0.0 : lookValue(nc, depth - 1));
            probeDepth--;
            boxAt = lvBox[depth];
            posOf = lvPos[depth];
            if (v > best) best = v;
        }
        return best;
    }

    vector<pair<int, int>> solve(const vector<int>& a, unsigned seed) {
        init(a, seed);
        lvCand.assign(LDEPTH + 1, {});
        lvBox.assign(LDEPTH + 1, {});
        lvPos.assign(LDEPTH + 1, {});
        vector<tuple<int, int, int>> cands;
        auto t0 = chrono::steady_clock::now();
        auto elapsed = [&]() {
            return chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        };
        while (curMin < NN) {
            int k = curMin;
            wgt.resize(NN);
            for (int b = curMin; b < NN; b++) wgt[b] = pow(1.0 + (b - curMin), -weightP);
            // TLE insurance: fall back to a shallower search if we run long.
            int depth = LDEPTH;
            double el = elapsed();
            if (el > 1.5) depth = 2;
            else if (el > 1.2) depth = 4;
            if (posOf[k] == EXIT) {
                // Step the box off the exit with the best side effect; the
                // next step brings it back (row-0 edges belong to both belts).
                int bm = 0, bd = 1;
                double bg = -1e18;
                for (int m = 0; m < 2; m++)
                    for (int d = 1; d >= -1; d -= 2) {
                        double g = gain(m, d);
                        if (g > bg) { bg = g; bm = m; bd = d; }
                    }
                rotate(bm, bd);
                if (boxAt[EXIT] == curMin) { boxAt[EXIT] = -1; curMin++; continue; }
            }
            int c = posOf[k];
            while (c != EXIT) {
                descentMoves(c, cands);
                assert(!cands.empty());
                // Score candidates: immediate gain plus deep-lookahead value.
                int bm = -1, bd = 0, bc = -1;
                double bg = -1e18;
                vector<int> sb, sp;
                for (auto& [m, d, nc] : cands) {
                    double g1 = gain(m, d);
                    double rest = 0;
                    if (depth > 1 && nc != EXIT) {
                        sb = boxAt;
                        sp = posOf;
                        probeDepth++;
                        rotate(m, d);
                        rest = lookValue(nc, depth - 1);
                        probeDepth--;
                        boxAt = sb;
                        posOf = sp;
                    }
                    double g = g1 + (LOOK2_M / 1000.0) * rest;
#ifndef TIEB
#define TIEB 1
#endif
#if TIEB
                    // randomized tie-breaking
                    if (g > bg + 1e-9 || (g > bg - 1e-9 && (rng() & 1))) {
#else
                    if (g > bg + 1e-9) {
#endif
                        bg = g; bm = m; bd = d; bc = nc;
                    }
                }
                assert(bm >= 0);
                rotate(bm, bd);
                c = bc;
                if (boxAt[EXIT] == curMin) {
                    boxAt[EXIT] = -1;
                    curMin++;
                    break;
                }
            }
        }
        return ops;
    }
};

int main() {
    buildBelts();
    computeDist();
    int n;
    if (scanf("%d", &n) != 1) return 0;
    vector<int> a(NN);
    for (auto& x : a) scanf("%d", &x);

    Solver solver;
    vector<pair<int, int>> bestOps;
    for (int r = 0; r < NREST; r++) {
        auto ops = solver.solve(a, 0x9e3779b9u ^ (r * 0x85ebca6bu));
        if (bestOps.empty() || ops.size() < bestOps.size()) bestOps = ops;
    }
    assert(bestOps.size() <= 100000);

    // Output
    {
        string out;
        out.reserve(1 << 20);
        out += "2\n";
        for (int m = 0; m < 2; m++) {
            out += to_string(NN);
            for (int v : belts[m].seq) {
                out += ' '; out += to_string(v / N);
                out += ' '; out += to_string(v % N);
            }
            out += '\n';
        }
        out += to_string(bestOps.size());
        out += '\n';
        char buf[16];
        for (auto& [m, d] : bestOps) {
            int len = snprintf(buf, sizeof(buf), "%d %d\n", m, d);
            out.append(buf, len);
        }
        fwrite(out.data(), 1, out.size(), stdout);
    }
#ifdef DIAG
    fprintf(stderr, "T=%zu\n", bestOps.size());
#endif
    return 0;
}
