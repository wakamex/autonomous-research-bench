// AHC065 Conveyor Design
// Strategy: two Hamiltonian cycles (row-snake + column-snake), every cell on
// both belts. Fetch each successive minimum box to the exit along a shortest
// path in the union graph; each op moves the target box one edge.

#include <bits/stdc++.h>
#include <cassert>
using namespace std;

static const int N = 20;
static const int NN = N * N;
static inline int ID(int i, int j) { return i * N + j; }
static const int EXIT = ID(0, N / 2);

struct Belt {
    vector<int> seq;      // cells in cycle order
    vector<int> idx;      // idx[cell] = position in seq
};

static Belt belts[2];

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

int main() {
    buildBelts();
    int n;
    if (scanf("%d", &n) != 1) return 0;
    vector<int> boxAt(NN);
    for (auto& x : boxAt) scanf("%d", &x);

    // Static BFS distance to exit over the union graph.
    vector<int> dist(NN, -1);
    {
        queue<int> q;
        dist[EXIT] = 0;
        q.push(EXIT);
        auto tryEdge = [&](int c, int d) {
            if (d >= 0 && dist[d] < 0) { dist[d] = dist[c] + 1; q.push(d); }
        };
        while (!q.empty()) {
            int c = q.front(); q.pop();
            for (int m = 0; m < 2; m++) {
                int x = belts[m].idx[c];
                tryEdge(c, belts[m].seq[(x + 1) % NN]);
                tryEdge(c, belts[m].seq[(x + NN - 1) % NN]);
            }
        }
        for (int c = 0; c < NN; c++) assert(dist[c] >= 0);
    }

    vector<int> posOf(NN);
    for (int c = 0; c < NN; c++) posOf[boxAt[c]] = c;

    int curMin = 0;
    if (boxAt[EXIT] == 0) { boxAt[EXIT] = -1; posOf[0] = -1; curMin = 1; }

    vector<pair<int, int>> ops; // (belt, dir)

    auto rotateBelt = [&](int m, int d) {
        ops.push_back({m, d});
        const vector<int>& s = belts[m].seq;
        static vector<int> occ;
        occ.resize(NN);
        for (int x = 0; x < NN; x++) occ[x] = boxAt[s[x]];
        for (int x = 0; x < NN; x++) {
            int b = occ[(x - d + NN) % NN];
            boxAt[s[x]] = b;
            if (b >= 0) posOf[b] = s[x];
        }
    };

    while (curMin < NN) {
        int k = curMin;
        if (posOf[k] == EXIT) {
            // k already at exit: step it off along belt A; the next fetch
            // step brings it back (row-0 edges belong to both belts).
            rotateBelt(0, 1);
            if (boxAt[EXIT] == curMin) { boxAt[EXIT] = -1; curMin++; continue; }
        }
        int c = posOf[k];
        while (c != EXIT) {
            // pick a belt move that descends the BFS distance
            int bm = -1, bd = 0, bc = -1;
            for (int m = 0; m < 2 && bm < 0; m++) {
                int x = belts[m].idx[c];
                int p = belts[m].seq[(x + 1) % NN];
                int q = belts[m].seq[(x + NN - 1) % NN];
                if (dist[p] == dist[c] - 1) { bm = m; bd = 1; bc = p; }
                else if (dist[q] == dist[c] - 1) { bm = m; bd = -1; bc = q; }
            }
            assert(bm >= 0);
            rotateBelt(bm, bd);
            c = bc;
            if (boxAt[EXIT] == curMin) {
                boxAt[EXIT] = -1;
                curMin++;
                break;
            }
        }
    }
    assert(ops.size() <= 100000);

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
        out += to_string(ops.size());
        out += '\n';
        char buf[16];
        for (auto& [m, d] : ops) {
            int len = snprintf(buf, sizeof(buf), "%d %d\n", m, d);
            out.append(buf, len);
        }
        fwrite(out.data(), 1, out.size(), stdout);
    }
    fprintf(stderr, "T=%zu\n", ops.size());
    return 0;
}
