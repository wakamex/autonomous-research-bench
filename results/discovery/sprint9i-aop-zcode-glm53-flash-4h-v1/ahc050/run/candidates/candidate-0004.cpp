// AHC050 "Gamble on Ice" solution.
//
// Policy: greedy min-mass placement. Each step, among free cells whose
// post-slide robot mass <= min + 3e-5, place on the cell maximizing
//   (rock-or-wall neighbor count) * 1e4 - (avg BFS dist to initial rocks of free neighbors),
// which grows rock clusters while preserving large open regions.
// Runs randomized restarts (jittered tie-breaks), keeps the best expected
// value, under a wall-clock guard.
#include <bits/stdc++.h>
using namespace std;
struct RNG {
    uint64_t s;
    RNG(uint64_t seed):s(seed){}
    uint64_t next(){
        s += 0x9E3779B97F4A7C15ull;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
};
static const double WINDOW = 3e-5;
static const double JITTER = 0.05;
static const int RMAX = 40;
static const double GUARD_SECONDS = 1.25;

int n, K, M;
vector<int> distr;

double gstep(vector<char>& rock, vector<double>& p, vector<double>& nxt,
             RNG* rng, double jitter, vector<pair<int,int>>* order){
    fill(nxt.begin(), nxt.end(), 0.0);
    for (int i = 0; i < n; i++) {
        int j = 0, base = i * n;
        while (j < n) {
            if (rock[base + j]) { j++; continue; }
            int js = j; double sum = 0;
            while (j < n && !rock[base + j]) { sum += p[base + j]; j++; }
            double q = 0.25 * sum;
            nxt[base + j - 1] += q; nxt[base + js] += q;
        }
    }
    for (int j = 0; j < n; j++) {
        int i = 0;
        while (i < n) {
            if (rock[i * n + j]) { i++; continue; }
            int is = i; double sum = 0;
            while (i < n && !rock[i * n + j]) { sum += p[i * n + j]; i++; }
            double q = 0.25 * sum;
            nxt[is * n + j] += q; nxt[(i - 1) * n + j] += q;
        }
    }
    double bestv = 1e300;
    for (int id = 0; id < n * n; id++) {
        if (rock[id]) continue;
        if (nxt[id] < bestv) bestv = nxt[id];
    }
    double hi = bestv + WINDOW;
    int best = -1; double bs = -1e300;
    for (int id = 0; id < n * n; id++) {
        if (rock[id]) continue;
        if (nxt[id] > hi) continue;
        int i = id / n, j = id % n;
        int nbw = 0;
        if (i == 0     || rock[id - n]) nbw++;
        if (i == n - 1 || rock[id + n]) nbw++;
        if (j == 0     || rock[id - 1]) nbw++;
        if (j == n - 1 || rock[id + 1]) nbw++;
        double nbstop2 = 0;
        int ni[4] = {i - 1, i + 1, i, i}, nj[4] = {j, j, j - 1, j + 1};
        for (int d = 0; d < 4; d++) {
            int i2 = ni[d], j2 = nj[d];
            if (i2 < 0 || i2 >= n || j2 < 0 || j2 >= n) continue;
            int id2 = i2 * n + j2;
            if (rock[id2]) continue;
            nbstop2 += distr[id2];
        }
        double sc = nbw * 1e4 - nbstop2 / 4;
        if (jitter > 0) sc += jitter * (double)(rng->next() % 1000000) / 1e6;
        if (sc > bs) { bs = sc; best = id; }
    }
    double l = nxt[best];
    nxt[best] = 0.0;
    rock[best] = 1;
    p.swap(nxt);
    if (order) order->push_back({best / n, best % n});
    return l;
}

int main(){
    auto T0 = chrono::steady_clock::now();
    if (scanf("%d %d", &n, &M) != 2) return 0;
    vector<string> g(n);
    for (int i = 0; i < n; i++) { char buf[256]; scanf("%s", buf); g[i] = buf; }
    K = n * n - M;
    vector<char> rock0(n * n, 0);
    vector<pair<int,int>> fc;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            if (g[i][j] == '#') rock0[i * n + j] = 1;
            else fc.push_back({i, j});
        }
    // BFS distance to nearest initial rock (walls excluded; unreachable -> 0)
    distr.assign(n * n, -1);
    {
        queue<int> bfs;
        for (int id = 0; id < n * n; id++) if (rock0[id]) { distr[id] = 0; bfs.push(id); }
        int dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
        while (!bfs.empty()) {
            int id = bfs.front(); bfs.pop();
            int i = id / n, j = id % n;
            for (auto& dd : dirs) {
                int i2 = i + dd[0], j2 = j + dd[1];
                if (i2 < 0 || i2 >= n || j2 < 0 || j2 >= n) continue;
                int id2 = i2 * n + j2;
                if (distr[id2] < 0) { distr[id2] = distr[id] + 1; bfs.push(id2); }
            }
        }
        for (int id = 0; id < n * n; id++) if (distr[id] < 0) distr[id] = 0;
    }
    vector<double> p0(n * n, 0.0);
    double inv = 1.0 / K;
    for (auto& c : fc) p0[c.first * n + c.second] = inv;

    RNG rng(0x123456789ABCDEFull);
    double bestRet = -1;
    vector<pair<int,int>> bestOrder, tmpOrder;
    vector<char> rock(n * n);
    vector<double> p(n * n), nxt(n * n);
    for (int r = 0; r < RMAX; r++) {
        rock = rock0;
        p = p0;
        double life = 1.0, ret = 0.0;
        tmpOrder.clear();
        tmpOrder.reserve(K);
        double jitter = (r == 0) ? 0.0 : JITTER;
        for (int t = 0; t < K; t++) {
            double l = gstep(rock, p, nxt, &rng, jitter, &tmpOrder);
            life -= l;
            ret += life;
        }
        if (ret > bestRet) { bestRet = ret; bestOrder = tmpOrder; }
        double el = chrono::duration<double>(chrono::steady_clock::now() - T0).count();
        if (el > GUARD_SECONDS) break;
    }
    string out;
    out.reserve(K * 8);
    for (auto& c : bestOrder) {
        out += to_string(c.first);
        out += ' ';
        out += to_string(c.second);
        out += '\n';
    }
    fwrite(out.data(), 1, out.size(), stdout);
    return 0;
}
