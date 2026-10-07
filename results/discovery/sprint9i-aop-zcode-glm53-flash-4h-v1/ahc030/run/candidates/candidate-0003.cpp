// AHC030 - Polyomino Mining
//
// Approach: Bayesian inference over polyomino placements.
// Each field k sits at an unknown valid offset. Survey queries return a noisy
// linear measurement of v(S): response ~ N(k*eps + (1-2*eps)*v(S), k*eps*(1-eps)),
// clamped at 0 and rounded. We run an annealed Gibbs sampler over the joint
// placement configuration; cell marginals P(v>0) are estimated from visited
// configs. Drills contribute near-exact singleton pseudo-observations. Cells
// whose marginal stays uncertain are drilled before answering, and a
// drill-everything fallback guarantees a correct answer within the op budget.

#include <bits/stdc++.h>
using namespace std;
using u64 = uint64_t;

// ---------------- tunables ----------------
static int QA_QUERIES = 70;         // initial random surveys (inclusion p=0.5)
static int QB_RANDOM = 30;          // second round random surveys
static int QB_TARGETED = 10;        // second round targeted surveys
static int DRILL_CAP_A = 18;        // max drills in the first drill batch
static double DRILL_LO_A = 0.10, DRILL_HI_A = 0.90;
static int DRILL_CAP_C = 26;        // max drills in the final drill batch
static double DRILL_LO_C = 0.02, DRILL_HI_C = 0.98;
static int MAX_RETRIES = 4;
static double SIGMA_DRILL = 0.05;   // pseudo-observation noise for drills
static double DEADLINE_A = 0.60, DEADLINE_B = 1.25, DEADLINE_C = 1.55, HARD_LIMIT = 2.30;
static bool DEBUG = getenv("AHC_DEBUG") != nullptr;

// ---------------- timing ----------------
static chrono::steady_clock::time_point T0;
static inline double elapsed() {
    return chrono::duration<double>(chrono::steady_clock::now() - T0).count();
}

// ---------------- normal tail ----------------
// log P(Z > x) for standard normal Z.
static inline double logQtail(double x) {
    if (x < 3.5) {
        double z = x * 0.70710678118654752440;
        double t = 1.0 / (1.0 + 0.5 * z);
        const double cof[9] = {1.00002368, 0.37409196, 0.09678418, -0.18628806,
                               0.27886807, -1.13520398, 1.48851587, -0.82215223, 0.17087277};
        double acc = cof[8];
        for (int i = 7; i >= 0; i--) acc = cof[i] + t * acc;
        double p = t * exp(-z * z - 1.26551223 + t * acc);
        return log(p) - 0.6931471805599453;
    }
    double i2 = 1.0 / (x * x);
    double s = 1.0 - i2 + 3.0 * i2 * i2 - 15.0 * i2 * i2 * i2 + 105.0 * i2 * i2 * i2 * i2;
    return -0.5 * x * x - log(x) - 0.91893853320467274 + log(s);
}
static inline double logPhiBelow(double z) { // log P(Z < z)
    if (z >= 0.0) {
        double q = exp(logQtail(z));
        if (q >= 1.0) return 0.0;
        return log1p(-q);
    }
    return logQtail(-z);
}
static inline double logdiffexp(double a, double b) { // a >= b
    if (b - a < -700.0) return a;
    return a + log1p(-exp(b - a));
}
static inline double logPhiInterval(double lo, double hi) { // log P(lo < Z < hi)
    if (hi <= 0.0) return logdiffexp(logQtail(-hi), logQtail(-lo));
    if (lo >= 0.0) return logdiffexp(logQtail(lo), logQtail(hi));
    double q = exp(logQtail(-lo)) + exp(logQtail(hi));
    if (q >= 1.0) return -690.0;
    return log1p(-q);
}
// log-likelihood of observing response r when the true sum is V
static inline double loglikResponse(int k, double eps, double sigma, int r, double V) {
    double mu = k * eps + (1.0 - 2.0 * eps) * V;
    if (r <= 0) return logPhiBelow((0.5 - mu) / sigma);
    double lo = (r - 0.5 - mu) / sigma, hi = (r + 0.5 - mu) / sigma;
    return logPhiInterval(lo, hi);
}

// ---------------- bitmask over N*N cells ----------------
static const int NW = 7; // N <= 20  =>  N*N <= 400  =>  7 words
struct BS { u64 w[NW]; };
static inline void bsSet(BS &b, int c) { b.w[c >> 6] |= 1ull << (c & 63); }
static inline int bsAndPop(const BS &a, const BS &b) {
    int p = 0;
    for (int i = 0; i < NW; i++) p += __builtin_popcountll(a.w[i] & b.w[i]);
    return p;
}

// ---------------- global state ----------------
static int N, M, N2;
static double EPS;
static vector<int> vKnown;      // -1 unknown, else drilled value
static vector<char> drilled;    // flag

struct Obs { BS cells; int k; int r; double sigma; };
static vector<Obs> obs;

struct Field {
    int s;                      // shape area
    int P;                      // number of placements
    vector<BS> masks;
    vector<uint8_t> cm;         // P x OCAP contribution counts, row-major
};
static vector<Field> fs;
static vector<int> cfg;         // current placement index per field
static vector<double> baseAll;  // per-observation V(S) under current config
static int OCAP = 0;            // cm stride: fixed column capacity (2*N^2)

static vector<double> priorCov; // per-cell coverage probability under uniform placements

static long long opsUsed = 0;
static double costQueries = 0, costDrills = 0, costGuesses = 0;
static long long nSurveys = 0, nDrills = 0, nGuessFails = 0;

static inline long long opsLeft() { return 2LL * N2 - opsUsed; }
static inline bool budgetOK() { return opsUsed < (long long)N2 - 8; }

// ---------------- I/O ----------------
static void outLine(const string &s) {
    fwrite(s.data(), 1, s.size(), stdout);
    fputc('\n', stdout);
    fflush(stdout);
}
static bool readInt(int &v) {
    return (bool)(cin >> v);
}

// ---------------- observation handling ----------------
static void refreshBase(int o) {
    double b = 0;
    for (int f = 0; f < M; f++)
        b += fs[f].cm[(long long)cfg[f] * OCAP + o];
    baseAll[o] = b;
}
// per-observation log-likelihood table over every possible sum V in [0, k]
static vector<double> llTab;
static vector<long long> llOff;
static void addObs(const BS &cells, int k, int r, double sigma) {
    int o = (int)obs.size();
    obs.push_back({cells, k, r, sigma});
    for (auto &f : fs) {
        for (int p = 0; p < f.P; p++)
            f.cm[(long long)p * OCAP + o] = (uint8_t)bsAndPop(f.masks[p], cells);
    }
    llOff.push_back((long long)llTab.size());
    for (int V = 0; V <= k; V++)
        llTab.push_back(loglikResponse(k, EPS, sigma, r, (double)V));
    baseAll.push_back(0.0);
    refreshBase(o);
}

// ---------------- joint-mode search ----------------
// The posterior over joint placements is extremely multimodal: per-field
// conditionals have deep wrong modes, and local chains never mix. Instead we
// search for high-likelihood joint configurations with multi-start iterated
// conditional modes (ICM), keep the distinct modes found, and build cell
// marginals from a likelihood-weighted mixture of them.
static mt19937 rng(20240209ULL);
static inline double rng01() { return (double)rng() / 4294967296.0; }

struct Mode {
    double ll;
    vector<int> cfg;
    BS cov;                     // cells with v > 0 under this config
};
static vector<Mode> modes;
static vector<double> modeW;
static double modeWSum = 0;

static double jointLL() {
    double ll = 0;
    for (int o = 0; o < (int)obs.size(); o++)
        ll += loglikResponse(obs[o].k, EPS, obs[o].sigma, obs[o].r, baseAll[o]);
    return ll;
}
static BS configCoverage() {
    BS cov{};
    for (int f = 0; f < M; f++)
        for (int w = 0; w < NW; w++) cov.w[w] |= fs[f].masks[cfg[f]].w[w];
    return cov;
}

// conditional loglik of every placement of field f given the other fields
static void fieldScores(int f, vector<double> &sc, vector<int> &baseW,
                        vector<double> &tables, vector<size_t> &tblOff) {
    int O = (int)obs.size();
    Field &fld = fs[f];
    baseW.assign(O, 0);
    for (int o = 0; o < O; o++)
        baseW[o] = (int)llround(baseAll[o]) - fld.cm[(long long)cfg[f] * OCAP + o];
    size_t total = 0;
    tblOff.assign(O, 0);
    for (int o = 0; o < O; o++) {
        tblOff[o] = total;
        total += min(fld.s, obs[o].k) + 1;
    }
    tables.resize(total);
    for (int o = 0; o < O; o++) {
        int cmx = min(fld.s, obs[o].k);
        const double *lt = llTab.data() + llOff[o] + baseW[o];
        for (int c = 0; c <= cmx; c++) tables[tblOff[o] + c] = lt[c];
    }
    sc.assign(fld.P, 0.0);
    for (int p = 0; p < fld.P; p++) {
        const uint8_t *row = fld.cm.data() + (long long)p * OCAP;
        double s = 0;
        for (int o = 0; o < O; o++) s += tables[tblOff[o] + row[o]];
        sc[p] = s;
    }
}
static void applyPlacement(int f, int p) {
    int O = (int)obs.size();
    const uint8_t *row = fs[f].cm.data() + (long long)p * OCAP;
    const uint8_t *old = fs[f].cm.data() + (long long)cfg[f] * OCAP;
    for (int o = 0; o < O; o++) baseAll[o] += (int)row[o] - (int)old[o];
    cfg[f] = p;
}

// coordinate-ascent to a local mode; returns number of changing passes
static int icm(int maxPass) {
    int O = (int)obs.size();
    if (O == 0) return 0;
    vector<double> sc;
    vector<int> baseW;
    vector<double> tables;
    vector<size_t> tblOff;
    int passes = 0;
    for (int it = 0; it < maxPass; it++) {
        bool changed = false;
        for (int f = 0; f < M; f++) {
            if (fs[f].P <= 1) continue;
            fieldScores(f, sc, baseW, tables, tblOff);
            int best = 0;
            for (int p = 1; p < fs[f].P; p++)
                if (sc[p] > sc[best]) best = p;
            if (best != cfg[f]) {
                applyPlacement(f, best);
                changed = true;
            }
        }
        passes++;
        if (!changed) break;
    }
    return passes;
}
// pair-coordinate move: jointly re-optimize fields f and g (exact over P_f x P_g)
static void pairOptimize(int f, int g) {
    int O = (int)obs.size();
    if (O == 0) return;
    Field &Ff = fs[f];
    Field &Gg = fs[g];
    static vector<double> baseW;
    baseW.assign(O, 0.0);
    for (int o = 0; o < O; o++)
        baseW[o] = (int)llround(baseAll[o]) - Ff.cm[(long long)cfg[f] * OCAP + o] -
                   Gg.cm[(long long)cfg[g] * OCAP + o];
    int smax = Ff.s + Gg.s;
    static vector<double> t2;
    t2.assign((size_t)O * (smax + 1), 0.0);
    for (int o = 0; o < O; o++) {
        const double *lt = llTab.data() + llOff[o] + (int)baseW[o];
        for (int s = 0; s <= smax; s++) t2[(size_t)o * (smax + 1) + s] = lt[s];
    }
    int ba = cfg[f], bb = cfg[g];
    double best = -1e300;
    for (int a = 0; a < Ff.P; a++) {
        const uint8_t *ra = Ff.cm.data() + (long long)a * OCAP;
        for (int b = 0; b < Gg.P; b++) {
            const uint8_t *rb = Gg.cm.data() + (long long)b * OCAP;
            double s = 0;
            for (int o = 0; o < O; o++) s += t2[(size_t)o * (smax + 1) + ra[o] + rb[o]];
            if (s > best) { best = s; ba = a; bb = b; }
        }
    }
    if (ba != cfg[f]) applyPlacement(f, ba);
    if (bb != cfg[g]) applyPlacement(g, bb);
}
// single-field ICM + pair-move passes until fixpoint or deadline
static void polish(double deadline) {
    icm(20);
    while (elapsed() < deadline) {
        double before = jointLL();
        for (int f = 0; f < M; f++) {
            for (int g = f + 1; g < M; g++) {
                pairOptimize(f, g);
                if (elapsed() > deadline) return;
            }
        }
        if (jointLL() > before - 1e-9) break;
    }
}
static void randomizeConfig(int nFields) {
    if (nFields >= M) {
        for (int f = 0; f < M; f++) {
            int O = (int)obs.size();
            const uint8_t *old = fs[f].cm.data() + (long long)cfg[f] * OCAP;
            int pick = rng() % fs[f].P;
            const uint8_t *row = fs[f].cm.data() + (long long)pick * OCAP;
            for (int o = 0; o < O; o++) baseAll[o] += (int)row[o] - (int)old[o];
            cfg[f] = pick;
        }
    } else {
        vector<int> ids(M);
        for (int f = 0; f < M; f++) ids[f] = f;
        shuffle(ids.begin(), ids.end(), rng);
        for (int t = 0; t < nFields; t++) {
            int f = ids[t];
            int O = (int)obs.size();
            const uint8_t *old = fs[f].cm.data() + (long long)cfg[f] * OCAP;
            int pick = rng() % fs[f].P;
            const uint8_t *row = fs[f].cm.data() + (long long)pick * OCAP;
            for (int o = 0; o < O; o++) baseAll[o] += (int)row[o] - (int)old[o];
            cfg[f] = pick;
        }
    }
}
// prior expected contribution per field per obs (uniform placements)
static vector<vector<double>> priorEc;   // M x OCAP
static vector<double> priorBase;         // sum over all fields, per obs
static vector<vector<int>> priorEcI;     // integer versions (x1000 rounding)
static vector<int> priorBaseI;
static void buildPriorEc() {
    int OCAPv = OCAP;
    priorEc.assign(M, vector<double>(OCAPv, 0.0));
    for (int f = 0; f < M; f++) {
        Field &fld = fs[f];
        for (int p = 0; p < fld.P; p++) {
            double w = 1.0 / fld.P;
            const uint8_t *row = fld.cm.data() + (long long)p * OCAPv;
            for (int o = 0; o < OCAPv; o++) priorEc[f][o] += w * row[o];
        }
    }
    priorBase.assign(OCAPv, 0.0);
    for (int f = 0; f < M; f++)
        for (int o = 0; o < OCAPv; o++) priorBase[o] += priorEc[f][o];
    priorEcI.assign(M, vector<int>(OCAPv, 0));
    priorBaseI.assign(OCAPv, 0);
    for (int f = 0; f < M; f++)
        for (int o = 0; o < OCAPv; o++) priorEcI[f][o] = (int)llround(priorEc[f][o]);
    for (int o = 0; o < OCAPv; o++) priorBaseI[o] = (int)llround(priorBase[o]);
}
// data-guided stochastic init with prior fog on unplaced fields
static void greedyInit(double T) {
    int O = (int)obs.size();
    vector<int> ids(M);
    for (int f = 0; f < M; f++) ids[f] = f;
    shuffle(ids.begin(), ids.end(), rng);
    // start every field at its prior-expected contribution
    vector<double> base(O);
    for (int o = 0; o < O; o++) base[o] = priorBase[o];
    vector<char> done(M, 0);
    vector<double> sc;
    vector<int> baseW;
    vector<double> tables;
    vector<size_t> tblOff;
    for (int f : ids) {
        Field &fld = fs[f];
        // remove this field's fog, score against remaining fog + placed fields
        for (int o = 0; o < O; o++) base[o] -= priorEc[f][o];
        baseW.assign(O, 0);
        for (int o = 0; o < O; o++)
            baseW[o] = (int)llround(base[o]) - fld.cm[(long long)cfg[f] * OCAP + o];
        size_t total = 0;
        tblOff.assign(O, 0);
        for (int o = 0; o < O; o++) {
            tblOff[o] = total;
            total += min(fld.s, obs[o].k) + 1;
        }
        tables.resize(total);
        for (int o = 0; o < O; o++) {
            int cmx = min(fld.s, obs[o].k);
            const double *lt = llTab.data() + llOff[o] + baseW[o];
            for (int c = 0; c <= cmx; c++) tables[tblOff[o] + c] = lt[c];
        }
        sc.assign(fld.P, 0.0);
        double mx = -1e300;
        for (int p = 0; p < fld.P; p++) {
            const uint8_t *row = fld.cm.data() + (long long)p * OCAP;
            double s = 0;
            for (int o = 0; o < O; o++) s += tables[tblOff[o] + row[o]];
            sc[p] = s;
            if (s > mx) mx = s;
        }
        double sum = 0;
        for (int p = 0; p < fld.P; p++) {
            double e = exp((sc[p] - mx) / T);
            sc[p] = e;
            sum += e;
        }
        int pick = fld.P - 1;
        double u = rng01() * sum;
        for (int p = 0; p < fld.P; p++) {
            u -= sc[p];
            if (u <= 0) { pick = p; break; }
        }
        // commit the sampled placement into base (replace its fog)
        const uint8_t *row = fld.cm.data() + (long long)pick * OCAP;
        for (int o = 0; o < O; o++) base[o] += row[o];
        done[f] = 1;
        if (pick != cfg[f]) applyPlacement(f, pick);
    }
    (void)done;
}
static void recordMode() {
    double ll = jointLL();
    for (auto &md : modes)
        if (md.cfg == cfg) {         // known mode: refresh likelihood
            md.ll = ll;
            return;
        }
    Mode md;
    md.ll = ll;
    md.cfg = cfg;
    md.cov = configCoverage();
    modes.push_back(md);
}
static void setSearchWeights() {
    // refresh stored likelihoods against the current observation set
    if (!modes.empty()) {
        for (size_t i = 0; i < modes.size(); i++) {
            for (int f = 0; f < M; f++) applyPlacement(f, modes[i].cfg[f]);
            modes[i].ll = jointLL();
        }
        // walk cfg back to modes[0]; baseAll now matches modes[0]
        for (int f = 0; f < M; f++) applyPlacement(f, modes[0].cfg[f]);
    }
    double best = -1e300;
    for (auto &md : modes) best = max(best, md.ll);
    const double TW = 4.0;           // smoothing temperature over modes
    modeWSum = 0;
    modeW.assign(modes.size(), 0.0);
    for (size_t i = 0; i < modes.size(); i++) {
        double w = exp((modes[i].ll - best) / TW);
        modeW[i] = w;
        modeWSum += w;
    }
}
// one Gibbs sweep: resample every field from its full conditional
static void gibbsSweep(double T) {
    int O = (int)obs.size();
    if (O == 0) return;
    vector<double> sc;
    vector<int> baseW;
    vector<double> tables;
    vector<size_t> tblOff;
    for (int f = 0; f < M; f++) {
        Field &fld = fs[f];
        if (fld.P <= 1) continue;
        fieldScores(f, sc, baseW, tables, tblOff);
        double mx = -1e300;
        for (int p = 0; p < fld.P; p++) mx = max(mx, sc[p]);
        double sum = 0;
        for (int p = 0; p < fld.P; p++) {
            double e = exp((sc[p] - mx) / T);
            sc[p] = e;
            sum += e;
        }
        int pick = fld.P - 1;
        double u = rng01() * sum;
        for (int p = 0; p < fld.P; p++) {
            u -= sc[p];
            if (u <= 0) { pick = p; break; }
        }
        if (pick != cfg[f]) applyPlacement(f, pick);
    }
}

// Gibbs sweeps over a subset of fields only (others fixed)
static void gibbsSubset(const vector<int> &ids, double T, int sweeps) {
    int O = (int)obs.size();
    if (O == 0) return;
    vector<double> sc;
    vector<int> baseW;
    vector<double> tables;
    vector<size_t> tblOff;
    for (int s = 0; s < sweeps; s++) {
        for (int f : ids) {
            Field &fld = fs[f];
            if (fld.P <= 1) continue;
            fieldScores(f, sc, baseW, tables, tblOff);
            double mx = -1e300;
            for (int p = 0; p < fld.P; p++) mx = max(mx, sc[p]);
            double sum = 0;
            for (int p = 0; p < fld.P; p++) {
                double e = exp((sc[p] - mx) / T);
                sc[p] = e;
                sum += e;
            }
            int pick = fld.P - 1;
            double u = rng01() * sum;
            for (int p = 0; p < fld.P; p++) {
                u -= sc[p];
                if (u <= 0) { pick = p; break; }
            }
            if (pick != cfg[f]) applyPlacement(f, pick);
        }
    }
}

// ---------------- beam search ----------------
// Systematic exploration: extend partial configurations field by field,
// keeping the best W partial configs. Unplaced fields contribute a rounded
// prior-expected "fog" so partial scores stay meaningful; the surviving
// configs are re-scored exactly (icm + jointLL) and recorded as modes.
struct BeamNode {
    double score;
    int parent;
    int choice;
};
static vector<int> Vnow;   // current per-obs sum under cfg
static double curLL;
static void beamSearch(int W, double deadline) {
    int O = (int)obs.size();
    if (O == 0) return;
    vector<int> obsK(O);
    for (int o = 0; o < O; o++) obsK[o] = obs[o].k;
    vector<int> order(M);
    for (int f = 0; f < M; f++) order[f] = f;
    sort(order.begin(), order.end(), [](int a, int b) { return fs[a].P < fs[b].P; });
    vector<int> levels;                 // fields with free placement
    for (int f : order)
        if (fs[f].P > 1) levels.push_back(f);
    // fold fixed fields (P==1) into the root base
    vector<int> root(O, 0);
    for (int f : order)
        if (fs[f].P == 1) {
            const uint8_t *row = fs[f].cm.data();
            const int *pe = priorEcI[f].data();
            for (int o = 0; o < O; o++) root[o] += (int)row[o] - pe[o];
        }
    vector<vector<int>> bases(1, root);
    vector<BeamNode> nodes(1, BeamNode{0.0, -1, -1});
    const double *tab = llTab.data();
    vector<double> cands;
    vector<int> candParent, candChoice;
    for (size_t li = 0; li < levels.size(); li++) {
        int f = levels[li];
        Field &fld = fs[f];
        const int *pe = priorEcI[f].data();
        cands.clear();
        candParent.clear();
        candChoice.clear();
        cands.reserve((size_t)nodes.size() * fld.P);
        candParent.reserve((size_t)nodes.size() * fld.P);
        candChoice.reserve((size_t)nodes.size() * fld.P);
        for (size_t j = 0; j < nodes.size(); j++) {
            const int *bj = bases[j].data();
            for (int p = 0; p < fld.P; p++) {
                const uint8_t *row = fld.cm.data() + (long long)p * OCAP;
                double s = 0;
                for (int o = 0; o < O; o++) {
                    int cur = priorBaseI[o] + bj[o];
                    if (cur < 0) cur = 0;
                    else if (cur > obsK[o]) cur = obsK[o];
                    int nxt = cur + (int)row[o] - pe[o];
                    if (nxt < 0) nxt = 0;
                    else if (nxt > obsK[o]) nxt = obsK[o];
                    s += tab[llOff[o] + nxt] - tab[llOff[o] + cur];
                }
                cands.push_back(s);
                candParent.push_back(j);
                candChoice.push_back(p);
            }
        }
        int C = (int)cands.size();
        vector<int> idx(C);
        for (int i = 0; i < C; i++) idx[i] = i;
        // diversity-preserving selection: top-W with at most 2 per parent
        if (C > 3 * W) {
            nth_element(idx.begin(), idx.begin() + 3 * W, idx.end(),
                        [&](int a, int b) { return cands[a] > cands[b]; });
            idx.resize(3 * W);
        }
        sort(idx.begin(), idx.end(), [&](int a, int b) { return cands[a] > cands[b]; });
        {
            vector<int> cnt(nodes.size(), 0);
            vector<int> sel;
            sel.reserve(W);
            for (int i = 0; i < (int)idx.size() && (int)sel.size() < W; i++) {
                int ci = idx[i];
                if (cnt[candParent[ci]] >= 2) continue;
                cnt[candParent[ci]]++;
                sel.push_back(ci);
            }
            idx.swap(sel);
        }
        C = (int)idx.size();
        vector<vector<int>> nb;
        vector<BeamNode> nn;
        nb.reserve(C);
        nn.reserve(C);
        for (int i = 0; i < C; i++) {
            int ci = idx[i];
            int pj = candParent[ci];
            int p = candChoice[ci];
            const uint8_t *row = fld.cm.data() + (long long)p * OCAP;
            const int *pe2 = priorEcI[f].data();
            vector<int> b2(O);
            const int *bj = bases[pj].data();
            for (int o = 0; o < O; o++) b2[o] = bj[o] + (int)row[o] - pe2[o];
            nn.push_back(BeamNode{cands[ci] + nodes[pj].score, pj, p});
            nb.push_back(b2);
        }
        nodes.swap(nn);
        bases.swap(nb);
        if (elapsed() > deadline) break;
    }
    // reconstruct configs (walk parents), re-score exactly, record modes
    vector<int> rank(nodes.size());
    for (size_t i = 0; i < nodes.size(); i++) rank[i] = i;
    sort(rank.begin(), rank.end(),
         [&](int a, int b) { return nodes[a].score > nodes[b].score; });
    size_t take = min<size_t>(nodes.size(), 30);
    for (size_t t = 0; t < take; t++) {
        int j = rank[t];
        vector<int> picks(levels.size(), -1);
        for (size_t li = levels.size(); li-- > 0;) {
            picks[li] = nodes[j].choice;
            j = nodes[j].parent;
            if (j < 0) break;
        }
        for (size_t li = 0; li < levels.size(); li++)
            if (picks[li] >= 0) applyPlacement(levels[li], picks[li]);
        icm(6);
        curLL = jointLL();
        recordMode();
    }
}

// ---------------- simulated annealing ----------------// ---------------- simulated annealing ----------------
// incremental move evaluation via llTab: Delta ll only touches obs where the
// two placements' contributions differ; ~10x cheaper than full rescoring.
static void saSync() {
    int O = (int)obs.size();
    Vnow.assign(O, 0);
    for (int f = 0; f < M; f++) {
        const uint8_t *row = fs[f].cm.data() + (long long)cfg[f] * OCAP;
        for (int o = 0; o < O; o++) Vnow[o] += row[o];
    }
    curLL = 0;
    for (int o = 0; o < O; o++)
        curLL += llTab[llOff[o] + Vnow[o]];
}
// one SA segment; returns improved current ll
static void saSegment(double T, long long moves, double deadline) {
    int O = (int)obs.size();
    if (O == 0) return;
    const double *tab = llTab.data();
    for (long long it = 0; it < moves; it++) {
        if ((it & 1023) == 0 && elapsed() > deadline) return;
        int f = rng() % M;
        int P = fs[f].P;
        if (P <= 1) continue;
        int p = rng() % P;
        const uint8_t *row = fs[f].cm.data() + (long long)p * OCAP;
        const uint8_t *rowc = fs[f].cm.data() + (long long)cfg[f] * OCAP;
        double d = 0;
        bool diff = false;
        for (int o = 0; o < O; o++) {
            int dd = (int)row[o] - (int)rowc[o];
            if (dd) {
                d += tab[llOff[o] + Vnow[o] + dd] - tab[llOff[o] + Vnow[o]];
                diff = true;
            }
        }
        if (!diff) continue;
        if (d >= 0 || exp(d / T) > rng01()) {
            for (int o = 0; o < O; o++) Vnow[o] += (int)row[o] - (int)rowc[o];
            cfg[f] = p;
            curLL += d;
        }
    }
}
static void search(int restarts, double deadline, double polishDeadline) {
    if (obs.empty()) return;
    int sinceImprove = 0;
    double bestLL = -1e300;
    for (auto &md : modes) bestLL = max(bestLL, md.ll);
    saSync();
    vector<int> bestCfgG = cfg;
    if (!modes.empty()) {
        int bi0 = 0;
        for (int i = 1; i < (int)modes.size(); i++)
            if (modes[i].ll > modes[bi0].ll) bi0 = i;
        bestCfgG = modes[bi0].cfg;
    }
    if (restarts < 40) restarts = 40; // keep a minimum ILS effort
    for (int r = 0; r < restarts; r++) {
        if (elapsed() > deadline) break;
        if (r == 0) {
            beamSearch(M >= 8 ? 300 : 150, min(deadline, elapsed() + 0.30));
            int bbi = 0;
            for (size_t i = 1; i < modes.size(); i++)
                if (modes[i].ll > modes[bbi].ll) bbi = i;
            if (!modes.empty()) {
                for (int f = 0; f < M; f++) applyPlacement(f, modes[bbi].cfg[f]);
                saSync();
                bestCfgG = modes[bbi].cfg;
            }
            // long guided chain from current cfg with temperature cycling
            for (int cyc = 0; cyc < 120; cyc++) {
                double T = (cyc % 6 == 5) ? 2.5 : (1.6 - 0.15 * (cyc % 6));
                gibbsSweep(T);
                if (cyc % 4 == 3) {
                    icm(6);
                    curLL = jointLL();
                    recordMode();
                    if (curLL > bestLL + 1e-9) { bestLL = curLL; bestCfgG = cfg; sinceImprove = 0; }
                    if (DEBUG) fprintf(stderr, "  CHAIN r=%d cyc=%d T=%.2f ll=%.1f nobs=%zu\n", r, cyc, T, curLL, obs.size());
                }
                if (elapsed() > deadline) break;
            }
        } else {
            // ruin-and-recreate ILS from the global best
            for (int f = 0; f < M; f++) applyPlacement(f, bestCfgG[f]);
            vector<int> ids(M);
            for (int f = 0; f < M; f++) ids[f] = f;
            shuffle(ids.begin(), ids.end(), rng);
            int R = 2 + rng() % min(4, M);
            if ((int)R >= M) R = M;
            ids.resize(R);
            for (int f : ids) {
                int O = (int)obs.size();
                const uint8_t *old = fs[f].cm.data() + (long long)cfg[f] * OCAP;
                int pick = rng() % fs[f].P;
                const uint8_t *row = fs[f].cm.data() + (long long)pick * OCAP;
                for (int o = 0; o < O; o++) baseAll[o] += (int)row[o] - (int)old[o];
                cfg[f] = pick;
            }
            gibbsSubset(ids, 1.4, 4);
            gibbsSubset(ids, 0.8, 2);
            icm(5);
            curLL = jointLL();
            recordMode();
            if (curLL > bestLL + 1e-9) { bestLL = curLL; bestCfgG = cfg; sinceImprove = 0; } else sinceImprove++;
        }
        if ((int)modes.size() >= 64) break;
    }
    // leave cfg at the best mode found, then full pair-move polish
    int bi = 0;
    for (int i = 1; i < (int)modes.size(); i++)
        if (modes[i].ll > modes[bi].ll) bi = i;
    if (!modes.empty()) {
        for (int f = 0; f < M; f++) applyPlacement(f, modes[bi].cfg[f]);
    }
    if (elapsed() < polishDeadline) polish(min(polishDeadline, deadline + 0.05));
    setSearchWeights();
    if (DEBUG) {
        int b2 = 0;
        for (int i = 1; i < (int)modes.size(); i++)
            if (modes[i].ll > modes[b2].ll) b2 = i;
        if (!modes.empty())
            fprintf(stderr, "  SEARCHEND bestll=%.1f nobs=%zu modes=%zu cfgmatch_cfg=%d\n",
                    modes[b2].ll, obs.size(), modes.size(), (int)(modes[b2].cfg == cfg));
    }
}

// ---------------- marginals ----------------
static void computeMarginals(vector<double> &m) {
    m.assign(N2, 0.0);
    if (modeWSum > 0) {
        for (size_t i = 0; i < modes.size(); i++) {
            double wi = modeW[i] / modeWSum;
            if (wi < 1e-12) continue;
            for (int w = 0; w < NW; w++) {
                u64 bits = modes[i].cov.w[w];
                while (bits) {
                    int b = __builtin_ctzll(bits);
                    bits &= bits - 1;
                    m[(w << 6) + b] += wi;
                }
            }
        }
    }
    for (int c = 0; c < N2; c++)
        if (drilled[c]) m[c] = vKnown[c] > 0 ? 1.0 : 0.0;
}


// ---------------- operations ----------------
static void sendSurvey(const vector<int> &cells) {
    int k = (int)cells.size();
    string s;
    s.reserve(k * 5 + 16);
    s += "q ";
    s += to_string(k);
    char buf[16];
    for (int c : cells) {
        snprintf(buf, sizeof(buf), " %d %d", c / N, c % N);
        s += buf;
    }
    outLine(s);
    int r;
    if (!readInt(r)) exit(0);
    opsUsed++;
    nSurveys++;
    costQueries += 1.0 / sqrt((double)k);
    BS b{};
    for (int c : cells) bsSet(b, c);
    double sigma = sqrt((double)k * EPS * (1.0 - EPS));
    addObs(b, k, r, sigma);
}
static void randomSurvey(double pin) {
    vector<int> cells;
    for (int c = 0; c < N2; c++)
        if (rng01() < pin) cells.push_back(c);
    if ((int)cells.size() < 2) {
        cells.clear();
        cells.push_back(rng() % N2);
        cells.push_back(rng() % N2);
        sort(cells.begin(), cells.end());
        cells.erase(unique(cells.begin(), cells.end()), cells.end());
        if ((int)cells.size() < 2) cells.push_back((cells[0] + 1) % N2);
    }
    sendSurvey(cells);
}
static void drill(int cell) {
    string s = "q 1 " + to_string(cell / N) + " " + to_string(cell % N);
    outLine(s);
    int v;
    if (!readInt(v)) exit(0);
    opsUsed++;
    nDrills++;
    costDrills += 1.0;
    vKnown[cell] = v;
    drilled[cell] = 1;
    BS b{};
    bsSet(b, cell);
    addObs(b, 1, v, SIGMA_DRILL);
}

// ---------------- driver ----------------
int main() {
    T0 = chrono::steady_clock::now();
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (!(cin >> N >> M >> EPS)) return 0;
    N2 = N * N;
    vKnown.assign(N2, -1);
    drilled.assign(N2, 0);

    fs.resize(M);
    for (int f = 0; f < M; f++) {
        int d;
        cin >> d;
        vector<pair<int, int>> pts(d);
        for (int t = 0; t < d; t++) cin >> pts[t].first >> pts[t].second;
        int minh = N, minw = N, maxh = -1, maxw = -1;
        for (int t = 0; t < d; t++) {
            minh = min(minh, pts[t].first);
            minw = min(minw, pts[t].second);
            maxh = max(maxh, pts[t].first);
            maxw = max(maxw, pts[t].second);
        }
        BS shape{};
        for (int t = 0; t < d; t++)
            bsSet(shape, (pts[t].first - minh) * N + (pts[t].second - minw));
        int h = maxh - minh + 1, w = maxw - minw + 1;
        Field &fld = fs[f];
        fld.s = d;
        for (int di = 0; di + h <= N; di++)
            for (int dj = 0; dj + w <= N; dj++) {
                BS mk{};
                int off = di * N + dj;
                for (int wd = 0; wd < NW; wd++) {
                    u64 bits = shape.w[wd];
                    while (bits) {
                        int b = __builtin_ctzll(bits);
                        bits &= bits - 1;
                        bsSet(mk, wd * 64 + b + off);
                    }
                }
                fld.masks.push_back(mk);
            }
        fld.P = (int)fld.masks.size();
    }
    OCAP = 2 * N2;
    for (auto &f : fs) f.cm.assign((long long)f.P * OCAP, 0);
    // (priorEc built lazily by greedyInit via buildPriorEc)
    // initialize a random starting configuration
    cfg.assign(M, 0);
    for (int f = 0; f < M; f++) cfg[f] = rng() % fs[f].P;
    buildPriorEc();
    // prior per-cell coverage probability (uniform placements)
    priorCov.assign(N2, 1.0);
    {
        vector<double> qf(N2);
        for (auto &f : fs) {
            fill(qf.begin(), qf.end(), 0.0);
            for (int p = 0; p < f.P; p++) {
                for (int w = 0; w < NW; w++) {
                    u64 bits = f.masks[p].w[w];
                    while (bits) {
                        int b = __builtin_ctzll(bits);
                        bits &= bits - 1;
                        qf[(w << 6) + b] += 1.0 / f.P;
                    }
                }
            }
            for (int c = 0; c < N2; c++) priorCov[c] *= (1.0 - min(1.0, qf[c]));
        }
        for (int c = 0; c < N2; c++) priorCov[c] = 1.0 - priorCov[c];
    }

    // scale query counts mildly with problem size
    int qa = QA_QUERIES;
    if (N <= 11) qa = min(qa, 45);
    if (N >= 18) qa = min(qa + 15, 95);
    if (M >= 6) qa = min(qa + 40, 140);
    if (M >= 8) qa = min(qa + 30, 170);
    if (M >= 12) qa = min(qa + 20, 190);

    for (int i = 0; i < qa && budgetOK(); i++) randomSurvey(0.5);
    search(48, DEADLINE_A, DEADLINE_A - 0.12);

    // ---- first drill batch: most uncertain cells ----
    {
        vector<double> m;
        computeMarginals(m);
        vector<int> cand;
        for (int c = 0; c < N2; c++)
            if (!drilled[c] && m[c] > DRILL_LO_A && m[c] < DRILL_HI_A) cand.push_back(c);
        sort(cand.begin(), cand.end(), [&](int a, int b) {
            return fabs(m[a] - 0.5) < fabs(m[b] - 0.5);
        });
        int cnt = 0;
        for (int c : cand) {
            if (cnt >= DRILL_CAP_A || !budgetOK()) break;
            drill(c);
            cnt++;
        }
    }
    search(48, DEADLINE_B, DEADLINE_B - 0.15);

    // ---- adaptive query rounds: random + targeted on uncertain cells ----
    int rndQ = QB_RANDOM, rndT = QB_TARGETED;
    if (M >= 6) { rndQ = 55; rndT = 18; }
    if (M >= 8) { rndQ = 65; rndT = 20; }
    if (M >= 12) { rndQ = 75; rndT = 24; }
    int nRounds = (M >= 6) ? 4 : 3;
    double deadlines[4] = {DEADLINE_C, 1.50, 1.70, 1.85};
    for (int round = 0; round < nRounds; round++) {
        vector<double> m;
        computeMarginals(m);
        vector<int> unc;
        for (int c = 0; c < N2; c++)
            if (!drilled[c] && m[c] > 0.03 && m[c] < 0.97) unc.push_back(c);
        sort(unc.begin(), unc.end(), [&](int a, int b) {
            return fabs(m[a] - 0.5) < fabs(m[b] - 0.5);
        });
        for (int i = 0; i < rndQ && budgetOK(); i++) randomSurvey(0.5);
        for (int i = 0; i < rndT && budgetOK(); i++) {
            vector<int> cells;
            int take = min((int)unc.size(), 16);
            for (int t = 0; t < take; t++) cells.push_back(unc[t]);
            for (int c = 0; c < N2; c++)
                if (!drilled[c] && rng01() < 0.25) cells.push_back(c);
            sort(cells.begin(), cells.end());
            cells.erase(unique(cells.begin(), cells.end()), cells.end());
            if ((int)cells.size() >= 2) sendSurvey(cells);
        }
        search(48, deadlines[round], deadlines[round] - 0.12);
    }

    // ---- ambiguity-resolution loop: drill where top modes disagree ----
    for (int iter = 0; iter < 3; iter++) {
        if (elapsed() > HARD_LIMIT - 0.75 || !budgetOK()) break;
        vector<double> m;
        computeMarginals(m);
        vector<int> cand;
        // (a) strongly uncertain posterior cells
        for (int c = 0; c < N2; c++)
            if (!drilled[c] && m[c] > 0.15 && m[c] < 0.85) cand.push_back(c);
        sort(cand.begin(), cand.end(), [&](int a, int b) {
            return fabs(m[a] - 0.5) < fabs(m[b] - 0.5);
        });
        // (b) cells where the two best modes disagree, when they are near-tied
        if (modes.size() >= 2) {
            int i1 = 0, i2 = -1;
            for (size_t i = 1; i < modes.size(); i++)
                if (modes[i].ll > modes[i1].ll) { i2 = i1; i1 = i; }
                else if (i2 < 0 || modes[i].ll > modes[i2].ll) i2 = i;
            if (i2 >= 0 && modes[i1].ll - modes[i2].ll < 100.0) {
                BS d{};
                for (int w = 0; w < NW; w++) d.w[w] = modes[i1].cov.w[w] ^ modes[i2].cov.w[w];
                vector<int> dc;
                for (int w = 0; w < NW; w++) {
                    u64 bits = d.w[w];
                    while (bits) {
                        int b = __builtin_ctzll(bits);
                        bits &= bits - 1;
                        int c = (w << 6) + b;
                        if (!drilled[c]) dc.push_back(c);
                    }
                }
                sort(dc.begin(), dc.end(), [&](int a, int b) {
                    return fabs(priorCov[a] - 0.5) < fabs(priorCov[b] - 0.5);
                });
                for (int c : dc) cand.push_back(c);
            }
        }
        if (cand.empty()) break;
        int cnt = 0;
        for (int c : cand) {
            if (cnt >= 8 || !budgetOK()) break;
            drill(c);
            cnt++;
        }
        search(24, min(HARD_LIMIT - 0.60, elapsed() + 0.28), min(HARD_LIMIT - 0.50, elapsed() + 0.38));
    }
    search(16, HARD_LIMIT - 0.55, HARD_LIMIT - 0.45);

    // ---- answer ----
    vector<double> m;
    computeMarginals(m);
    vector<int> order(N2);
    for (int c = 0; c < N2; c++) order[c] = c;
    sort(order.begin(), order.end(), [&](int a, int b) {
        return fabs(m[a] - 0.5) < fabs(m[b] - 0.5);
    });
    vector<char> inG(N2);
    for (int c = 0; c < N2; c++) inG[c] = (m[c] >= 0.5);
    vector<char> toggled(N2, 0);
    int nextFlip = 0;

    auto outputGuess = [&]() {
        vector<int> g;
        for (int c = 0; c < N2; c++)
            if (inG[c]) g.push_back(c);
        string s = "a " + to_string(g.size());
        char buf[16];
        for (int c : g) {
            snprintf(buf, sizeof(buf), " %d %d", c / N, c % N);
            s += buf;
        }
        outLine(s);
        opsUsed++;
        int resp;
        if (!readInt(resp)) exit(0);
        if (resp == 0) costGuesses += 1.0;
        return resp;
    };

    int resp = outputGuess();
    vector<char> triedMode(modes.size(), 0);
    if (!modes.empty()) {
        // mark the already-answered top mode as tried
        int b0 = 0;
        for (size_t i = 1; i < modes.size(); i++)
            if (modeW[i] > modeW[b0]) b0 = i;
        triedMode[b0] = 1;
    }
    while (resp != 1) {
        nGuessFails++;
        // cheap first: try the next-best mode's coverage as a whole answer
        if (opsLeft() >= 6) {
            int nxt = -1;
            for (size_t i = 0; i < modes.size(); i++)
                if (!triedMode[i] && (nxt < 0 || modeW[i] > modeW[nxt])) nxt = i;
            if (nxt >= 0 && modeW[nxt] > 1e-12) {
                triedMode[nxt] = 1;
                for (int w = 0; w < NW; w++) {
                    u64 bits = modes[nxt].cov.w[w];
                    for (int c = w * 64; c < (w + 1) * 64 && c < N2; c++)
                        inG[c] = (modes[nxt].cov.w[w] >> (c % 64)) & 1;
                    (void)bits;
                }
                for (int c = 0; c < N2; c++) if (drilled[c]) inG[c] = (vKnown[c] > 0);
                resp = outputGuess();
                if (resp == 1) break;
            }
        }
        bool canDrill = opsLeft() >= 40 && nGuessFails <= 10 && elapsed() < HARD_LIMIT - 0.20;
        if (canDrill) {
            // discriminative drills: XOR of the two best-likelihood modes,
            // padded with prior-uncertain cells
            vector<int> cand;
            if (modes.size() >= 2) {
                int i1 = 0, i2 = -1;
                for (size_t i = 1; i < modes.size(); i++)
                    if (modes[i].ll > modes[i1].ll) { i2 = i1; i1 = i; }
                    else if (i2 < 0 || modes[i].ll > modes[i2].ll) i2 = i;
                BS d{};
                for (int w = 0; w < NW; w++) d.w[w] = modes[i1].cov.w[w] ^ modes[i2].cov.w[w];
                for (int w = 0; w < NW; w++) {
                    u64 bits = d.w[w];
                    while (bits) {
                        int b = __builtin_ctzll(bits);
                        bits &= bits - 1;
                        int c = (w << 6) + b;
                        if (!drilled[c]) cand.push_back(c);
                    }
                }
                sort(cand.begin(), cand.end(), [&](int a, int b) {
                    return fabs(priorCov[a] - 0.5) < fabs(priorCov[b] - 0.5);
                });
            }
            {
                vector<int> allc;
                for (int c = 0; c < N2; c++)
                    if (!drilled[c]) allc.push_back(c);
                sort(allc.begin(), allc.end(), [&](int a, int b) {
                    return fabs(priorCov[a] - 0.5) < fabs(priorCov[b] - 0.5);
                });
                for (int c : allc) {
                    cand.push_back(c);
                    if ((int)cand.size() >= 16) break;
                }
            }
            int cnt = 0;
            for (int c : cand) {
                if (cnt >= 8 || opsLeft() < 30) break;
                drill(c);
                cnt++;
            }
            // re-search with the new evidence and answer again
            search(24, min(HARD_LIMIT - 0.25, elapsed() + 0.30), min(HARD_LIMIT - 0.20, elapsed() + 0.40));
            computeMarginals(m);
            for (int c = 0; c < N2; c++) inG[c] = (m[c] >= 0.5);
            resp = outputGuess();
            continue;
        }
        // fallback: drill everything if it fits the op budget, else keep flipping
        long long undrilled = 0;
        for (int c = 0; c < N2; c++) if (!drilled[c]) undrilled++;
        if (opsLeft() >= undrilled + 1) {
            for (int c = 0; c < N2 && opsLeft() >= 1; c++)
                if (!drilled[c]) drill(c);
            for (int c = 0; c < N2; c++) inG[c] = (vKnown[c] > 0);
            resp = outputGuess();
            if (resp == 1) break;
        }
        // best effort: toggle next-most-uncertain cells
        while (resp != 1 && opsLeft() >= 2 && nextFlip < N2) {
            while (nextFlip < N2 && (drilled[order[nextFlip]] || toggled[order[nextFlip]])) nextFlip++;
            if (nextFlip >= N2) break;
            int c = order[nextFlip++];
            inG[c] = !inG[c];
            toggled[c] = 1;
            resp = outputGuess();
        }
        if (resp != 1) {
            // last resort flip loop failed; give up (penalty)
        }
        break;
    }

    if (DEBUG) {
        double cost = costQueries + costDrills + costGuesses;
        fprintf(stderr, "n=%d m=%d eps=%.2f | q=%lld(%.2f) d=%lld g=%lld | cost=%.3f score=%lld | t=%.2f\n",
                N, M, EPS, nSurveys, costQueries, nDrills, nGuessFails,
                cost, (long long)llround(1e6 * max(cost, 1.0 / N)), elapsed());
    }
    return 0;
}
