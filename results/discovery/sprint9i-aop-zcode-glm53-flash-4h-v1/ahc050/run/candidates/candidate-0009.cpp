// AHC050 "Gamble on Ice" solution.
//
// Policy: greedy min-mass placement; among free cells with post-slide robot
// mass <= min+3e-5, place on the cell maximizing
//   (rock-or-wall neighbor count)*1e4 - (avg dist to initial rocks of free neighbors),
// growing rock clusters while preserving large open regions.
// Search: randomized restarts (jittered tie-breaks), then iterated local
// search — resample suffixes of the best order from cached prefix states —
// under a wall-clock guard.
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
static const int    R0 = 30;
static const int    RMIN = 12;
static const double GUARD1 = 1.35;   // phase-1 soft guard (s)
static const double GUARD  = 1.50;   // global guard (s)
static const double GUARD_ILS = 0.55; // ILS hands over to swap-LS here (s)

int n, K, M;
vector<int> distr;
vector<char> rock0;
vector<double> p0;
RNG rng(0x123456789ABCDEFull);

// one placement step from state (rock,p); returns loss; fills order slot
static inline double place(vector<char>& rock, vector<double>& p, vector<double>& nxt,
                           double jitter, vector<pair<int,int>>* order){
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
        if (jitter > 0) sc += jitter * (double)(rng.next() % 1000000) / 1e6;
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
    auto elapsed = [&]{ return chrono::duration<double>(chrono::steady_clock::now() - T0).count(); };
    if (scanf("%d %d", &n, &M) != 2) return 0;
    vector<string> g(n);
    for (int i = 0; i < n; i++) { char buf[256]; scanf("%s", buf); g[i] = buf; }
    K = n * n - M;
    rock0.assign(n * n, 0);
    vector<pair<int,int>> fc;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            if (g[i][j] == '#') rock0[i * n + j] = 1;
            else fc.push_back({i, j});
        }
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
    p0.assign(n * n, 0.0);
    double inv = 1.0 / K;
    for (auto& c : fc) p0[c.first * n + c.second] = inv;

    // ---- phase 1: restarts ----
    double bestRet = -1;
    vector<pair<int,int>> bestOrder, tmp;
    vector<double> nxt(n * n);
    for (int r = 0; r < R0; r++) {
        vector<char> rock = rock0;
        vector<double> p = p0;
        double life = 1.0, ret = 0.0;
        tmp.clear(); tmp.reserve(K);
        double jitter = (r == 0) ? 0.0 : 0.05;
        for (int t = 0; t < K; t++) {
            double l = place(rock, p, nxt, jitter, &tmp);
            life -= l;
            ret += life;
        }
        if (ret > bestRet) { bestRet = ret; bestOrder = tmp; }
        if (r + 1 >= RMIN && elapsed() > GUARD1) break;
    }

    // ---- phase 2: ILS suffix resampling ----
    // cache prefix states of the best order every SP steps
    const int SP = 25;
    const int NC = K / SP + 1;
    vector<vector<double>> cP(NC);
    vector<vector<char>>   cR(NC);
    vector<double> lifeBest(K + 1, 0.0);
    auto rebuildCacheFrom = [&](int fromC){
        // find nearest cached checkpoint <= fromC
        int c = fromC;
        vector<char> rock; vector<double> p;
        if (fromC > 0 && !cP[fromC].empty()) { rock = cR[fromC]; p = cP[fromC]; }
        else { rock = rock0; p = p0; c = 0; }
        int t = c * SP;
        for (int cc = c; cc < NC; cc++) {
            if (cc > c) {
                // advance from t to cc*SP applying bestOrder
                int tgt = min(cc * SP, K);
                for (int s = t; s < tgt; s++) {
                    int id = bestOrder[s].first * n + bestOrder[s].second;
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
                    nxt[id] = 0.0; rock[id] = 1; p.swap(nxt);
                }
                t = min(cc * SP, K);
            }
            cP[cc] = p; cR[cc] = rock;
        }
    };
    {
        vector<char> rock2 = rock0; vector<double> p2 = p0;
        double life = 1.0; lifeBest[0] = 1.0;
        for (int s = 0; s < K; s++) {
            int id = bestOrder[s].first * n + bestOrder[s].second;
            fill(nxt.begin(), nxt.end(), 0.0);
            for (int i = 0; i < n; i++) {
                int j = 0, base = i * n;
                while (j < n) {
                    if (rock2[base + j]) { j++; continue; }
                    int js = j; double sum = 0;
                    while (j < n && !rock2[base + j]) { sum += p2[base + j]; j++; }
                    double q = 0.25 * sum;
                    nxt[base + j - 1] += q; nxt[base + js] += q;
                }
            }
            for (int j = 0; j < n; j++) {
                int i = 0;
                while (i < n) {
                    if (rock2[i * n + j]) { i++; continue; }
                    int is = i; double sum = 0;
                    while (i < n && !rock2[i * n + j]) { sum += p2[i * n + j]; i++; }
                    double q = 0.25 * sum;
                    nxt[is * n + j] += q; nxt[(i - 1) * n + j] += q;
                }
            }
            life -= nxt[id];
            nxt[id] = 0.0; rock2[id] = 1; p2.swap(nxt);
            lifeBest[s + 1] = life;
        }
    }
    rebuildCacheFrom(0);

    // prefix contribution sums of current best order
    vector<double> cum(K + 1, 0.0);
    auto recomputeCum = [&]{
        for (int s = 0; s < K; s++) cum[s + 1] = cum[s] + lifeBest[s + 1];
    };
    recomputeCum();

    while (true) {
        if (elapsed() > GUARD_ILS) break;
        // checkpoint biased toward tail
        int back = 30 + (int)(rng.next() % (uint64_t)min(K - 31, 900));
        int t0 = K - back;
        if (t0 < 1) t0 = 1;
        int c = t0 / SP;
        // load nearest cached state at or before t0, roll forward
        vector<char> rock = cR[c]; vector<double> p = cP[c];
        int t = c * SP;
        for (int s = t; s < t0; s++) {
            int id = bestOrder[s].first * n + bestOrder[s].second;
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
            nxt[id] = 0.0; rock[id] = 1; p.swap(nxt);
        }
        double life0 = lifeBest[t0];
        // resample suffix
        vector<pair<int,int>> cand;
        cand.reserve(K - t0);
        static vector<double> traj; traj.resize(K - t0);
        double life = life0, ret = 0.0;
        double jitter = (rng.next() % 4 == 0) ? 0.2 : 0.05;
        for (int s = t0; s < K; s++) {
            double l = place(rock, p, nxt, jitter, &cand);
            life -= l;
            ret += life;
            traj[s - t0] = life;
        }
        if (cum[t0] + ret > bestRet + 1e-9) {
            bestRet = cum[t0] + ret;
            for (int s = t0; s < K; s++) {
                bestOrder[s] = cand[s - t0];
                lifeBest[s + 1] = traj[s - t0];
            }
            rebuildCacheFrom(c);
            recomputeCum();
        }
    }
    // ---- phase 3: directed swap local search ----
    vector<double> stepLoss(K, 0.0);
    {
        vector<char> rock2 = rock0; vector<double> p2 = p0;
        double life = 1.0; (void)life;
        for (int s = 0; s < K; s++) {
            int id = bestOrder[s].first * n + bestOrder[s].second;
            fill(nxt.begin(), nxt.end(), 0.0);
            for (int i = 0; i < n; i++) {
                int j = 0, base = i * n;
                while (j < n) {
                    if (rock2[base + j]) { j++; continue; }
                    int js = j; double sum = 0;
                    while (j < n && !rock2[base + j]) { sum += p2[base + j]; j++; }
                    double q = 0.25 * sum;
                    nxt[base + j - 1] += q; nxt[base + js] += q;
                }
            }
            for (int j = 0; j < n; j++) {
                int i = 0;
                while (i < n) {
                    if (rock2[i * n + j]) { i++; continue; }
                    int is = i; double sum = 0;
                    while (i < n && !rock2[i * n + j]) { sum += p2[i * n + j]; i++; }
                    double q = 0.25 * sum;
                    nxt[is * n + j] += q; nxt[(i - 1) * n + j] += q;
                }
            }
            stepLoss[s] = nxt[id];
            life -= nxt[id];
            nxt[id] = 0.0; rock2[id] = 1; p2.swap(nxt);
        }
    }
    vector<int> ids(K);
    for (int s = 0; s < K; s++) ids[s] = bestOrder[s].first * n + bestOrder[s].second;
    // exact sum-of-life from cached checkpoint ck over the tail for given id order
    auto evaluateFrom = [&](int ck, vector<int>& orderIds)->double{
        vector<char> rock = cR[ck]; vector<double> p = cP[ck];
        int t = ck * SP;
        double life = lifeBest[t], ret = 0.0;
        for (int s = t; s < K; s++) {
            int id = orderIds[s];
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
            double l = nxt[id];
            life -= l; ret += life;
            nxt[id] = 0.0; rock[id] = 1; p.swap(nxt);
        }
        return ret;
    };
    static int SPANMAX = getenv("SPANMAX") ? atoi(getenv("SPANMAX")) : 1400;
    static int CHAIN = getenv("CHAIN") ? atoi(getenv("CHAIN")) : 3;
    while (elapsed() < GUARD && elapsed() >= GUARD_ILS) {
        int minT1 = K;
        for (int c = 0; c < CHAIN; c++) {
            int t1 = rng.next() % (uint64_t)K;
            int span = 1 + rng.next() % (uint64_t)min(K - 1, SPANMAX);
            int t2 = t1 + span;
            if (t2 >= K) continue;
            if (stepLoss[t1] <= stepLoss[t2]) continue;
            swap(ids[t1], ids[t2]);
            minT1 = min(minT1, t1);
        }
        if (minT1 == K) continue; // no swap applied
        int ck = minT1 / SP;
        double ret = evaluateFrom(ck, ids);
        double total = ret + cum[ck * SP];
        if (total > bestRet + 1e-9) {
            bestRet = total;
            for (int s = ck * SP; s < K; s++) {
                if (bestOrder[s].first * n + bestOrder[s].second != ids[s])
                    bestOrder[s] = {ids[s] / n, ids[s] % n};
            }
            {
                vector<char> rock = cR[ck]; vector<double> p = cP[ck];
                int t = ck * SP;
                double life = lifeBest[t];
                for (int s = t; s < K; s++) {
                    int id = ids[s];
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
                    double l = nxt[id];
                    life -= l; stepLoss[s] = l; lifeBest[s + 1] = life;
                    nxt[id] = 0.0; rock[id] = 1; p.swap(nxt);
                }
                recomputeCum();
            }
            rebuildCacheFrom(ck);
        } else {
            // revert: restore tail from the current best order
            for (int s = ck * SP; s < K; s++) ids[s] = bestOrder[s].first * n + bestOrder[s].second;
        }
    }
    string out;
    out.reserve(K * 8);
    for (auto& c2 : bestOrder) {
        out += to_string(c2.first);
        out += ' ';
        out += to_string(c2.second);
        out += '\n';
    }
    fwrite(out.data(), 1, out.size(), stdout);
    fprintf(stderr, "E=%.1f rel=%.6f restarts_done_time=%.3f\n", bestRet, bestRet / (K - 1), elapsed());
    return 0;
}
