#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

namespace ch = std::chrono;

struct Placement {
    int sid;
    vector<int> pos;
    vector<unsigned char> need;
};

static bool matches(const vector<unsigned char> &g, const Placement &p) {
    for (size_t i = 0; i < p.pos.size(); ++i)
        if (g[p.pos[i]] != p.need[i]) return false;
    return true;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m;
    cin >> n >> m;
    vector<string> s(m);
    for (auto &x : s) cin >> x;

    const int N = n;
    vector<Placement> pl;

    auto add_line = [&](int sid, int r, int c, int vertical) {
        Placement p;
        p.sid = sid;
        p.pos.reserve(s[sid].size());
        p.need.reserve(s[sid].size());
        int y = r, x = c;
        for (char ch : s[sid]) {
            p.pos.push_back(y * N + x);
            p.need.push_back(ch - 'A');
            if (vertical) y = (y + 1) % N;
            else x = (x + 1) % N;
        }
        pl.push_back(std::move(p));
    };

    // Deduplicate identical walks (relevant when a pattern repeats inside one line).
    vector<unordered_set<string>> seen(m);
    for (int id = 0; id < m; ++id) {
        int len = s[id].size();
        seen[id].reserve(2 * N * N);
        for (int d = 0; d < 2; ++d) {
            for (int r = 0; r < N; ++r) {
                for (int c = 0; c < N; ++c) {
                    string key;
                    key.reserve(len * 2);
                    int y = r, x = c;
                    for (int t = 0; t < len; ++t) {
                        key.push_back(char(y));
                        key.push_back(char(x));
                        if (d) y = (y + 1) % N;
                        else x = (x + 1) % N;
                    }
                    if (seen[id].insert(key).second) add_line(id, r, c, d);
                }
            }
        }
    }

    vector<int> cell_start(N * N + 1, 0);
    for (const Placement &p : pl)
        for (int q : p.pos) ++cell_start[q + 1];
    for (int q = 0; q < N * N; ++q) cell_start[q + 1] += cell_start[q];
    vector<int> entry_pi(cell_start[N * N]);
    vector<unsigned char> entry_req(cell_start[N * N]);
    {
        vector<int> at = cell_start;
        for (int i = 0; i < (int)pl.size(); ++i)
            for (size_t j = 0; j < pl[i].pos.size(); ++j) {
                int q = pl[i].pos[j];
                entry_pi[at[q]] = i;
                entry_req[at[q]] = pl[i].need[j];
                ++at[q];
            }
    }

    vector<vector<int>> sid_pl(m);
    for (int i = 0; i < (int)pl.size(); ++i) sid_pl[pl[i].sid].push_back(i);

    auto now = ch::steady_clock::now;
    auto start = now();
    const double budget = 2.58;

    mt19937 gen(0x1234567);
    vector<unsigned char> grid(N * N);
    for (auto &x : grid) x = gen() & 7;

    vector<unsigned char> match(pl.size());
    vector<int> bad(pl.size(), 0);
    int covered = 0;
    vector<int> sid_hits(m, 0);
    vector<unsigned char> sid_match(m, 0);
    vector<unsigned> seen_sid(m, 0);
    vector<unsigned> seen_pl(pl.size(), 0);
    vector<unsigned> seen_cell(N * N, 0);
    vector<int> changed_sid;
    vector<int> unc_ids;
    vector<int> cells;
    vector<unsigned char> oldvals;
    vector<int> aff, oldbad;
    unsigned visit = 0;
    auto recount = [&]() {
        covered = 0;
        fill(sid_hits.begin(), sid_hits.end(), 0);
        for (int i = 0; i < (int)pl.size(); ++i) {
            int cnt = 0;
            for (size_t j = 0; j < pl[i].pos.size(); ++j)
                cnt += grid[pl[i].pos[j]] != pl[i].need[j];
            bad[i] = cnt;
            match[i] = cnt == 0;
            sid_hits[pl[i].sid] += match[i];
        }
        for (int id = 0; id < m; ++id) {
            sid_match[id] = sid_hits[id] > 0;
            covered += sid_match[id];
        }
    };
    recount();

    vector<unsigned char> best = grid;
    int best_unc = m - covered;
    int best_dots = (int)count(grid.begin(), grid.end(), 8);

    const int W = 1000;
    long long energy = W * best_unc + best_dots;
    long long moves = 0;
    int since_restart = 0;
    uniform_real_distribution<double> uniform(0.0, 1.0);

    while (true) {
        double el = ch::duration<double>(now() - start).count();
        if (el > budget) break;
        double frac = min(1.0, el / budget);
        double temp = 160.0 * pow(0.002, frac) + 0.08;

        if (gen() % 100 < 25) {
            // Large neighborhood move: place an uncovered pattern in its best
            // sampled location.  This creates coherent line fragments that
            // single-cell moves cannot reach.
            unc_ids.clear();
            for (int id = 0; id < m; ++id)
                if (!sid_match[id]) unc_ids.push_back(id);
            if (!unc_ids.empty()) {
                int sid = unc_ids[gen() % unc_ids.size()];
                int best_p = -1, best_mm = 100;
                for (int trial = 0; trial < 100; ++trial) {
                    int pi = sid_pl[sid][gen() % sid_pl[sid].size()];
                    int mm = 0;
                    for (size_t j = 0; j < pl[pi].pos.size(); ++j)
                        mm += grid[pl[pi].pos[j]] != pl[pi].need[j];
                    if (mm < best_mm) { best_mm = mm; best_p = pi; }
                }
                if (best_p >= 0 && best_mm > 0) {
                    const Placement &p = pl[best_p];
                    cells.clear(); oldvals.clear();
                    // Build the unique set of mismatched cells.
                    cells.clear(); oldvals.clear();
                    ++visit;
                    for (size_t j = 0; j < p.pos.size(); ++j) {
                        int q = p.pos[j];
                        unsigned char want = p.need[j];
                        if (grid[q] == want) continue;
                        if (seen_cell[q] != visit) {
                            seen_cell[q] = visit;
                            cells.push_back(q);
                            oldvals.push_back(grid[q]);
                        }
                        grid[q] = want;
                    }
                    aff.clear(); oldbad.clear();
                    for (size_t ci = 0; ci < cells.size(); ++ci) {
                        int q = cells[ci];
                        unsigned char oldc = oldvals[ci], newc = grid[q];
                        for (int z = cell_start[q]; z < cell_start[q + 1]; ++z) {
                            int pi = entry_pi[z];
                            unsigned char req = entry_req[z];
                            if (seen_pl[pi] != visit) {
                                seen_pl[pi] = visit; aff.push_back(pi); oldbad.push_back(bad[pi]);
                            }
                            if (oldc != req && newc == req) --bad[pi];
                            else if (oldc == req && newc != req) ++bad[pi];
                        }
                    }
                    changed_sid.clear();
                    ++visit;
                    for (int pi : aff) {
                        int ssid = pl[pi].sid;
                        if (seen_sid[ssid] != visit) { seen_sid[ssid] = visit; changed_sid.push_back(ssid); }
                        bool was = match[pi]; bool isnow = bad[pi] == 0;
                        match[pi] = isnow;
                        if (was != isnow) sid_hits[ssid] += isnow ? 1 : -1;
                    }
                    long long full_ne = energy;
                    for (int ssid : changed_sid) {
                        bool was = sid_match[ssid], isnow = sid_hits[ssid] > 0;
                        if (was != isnow) full_ne += isnow ? -W : W;
                    }
                    long long delta = full_ne - energy;
                    bool accept = delta <= 0 || uniform(gen) < exp(-double(delta) / temp);
                    if (accept) {
                        energy = full_ne;
                        for (int ssid : changed_sid) sid_match[ssid] = sid_hits[ssid] > 0;
                        int unc = full_ne / W;
                        if (unc < best_unc) { best_unc = unc; best = grid; since_restart = 0; }
                        ++moves;
                    } else {
                        for (size_t ci = 0; ci < cells.size(); ++ci) grid[cells[ci]] = oldvals[ci];
                        for (size_t z = 0; z < aff.size(); ++z) {
                            int pi = aff[z]; bad[pi] = oldbad[z]; bool should = bad[pi] == 0;
                            int ssid = pl[pi].sid;
                            if (match[pi] != should) {
                                sid_hits[ssid] += should ? 1 : -1;
                                match[pi] = should;
                            }
                        }
                        for (int ssid : changed_sid) sid_match[ssid] = sid_hits[ssid] > 0;
                    }
                }
            }
            continue;
        }

        // Prefer a mismatched cell of a sampled placement: this gives a much
        // stronger move than changing a completely unrelated cell.
        int sample_id = sid_pl[gen() % m][gen() % sid_pl[gen() % m].size()];
        const Placement &sample = pl[sample_id];
        vector<int> mismatch;
        for (size_t j = 0; j < sample.pos.size(); ++j)
            if (grid[sample.pos[j]] != sample.need[j]) mismatch.push_back(j);
        if (mismatch.empty()) continue;
        int req_pos = mismatch[gen() % mismatch.size()];
        int cell = sample.pos[req_pos];
        unsigned char oldv = grid[cell];
        unsigned char newv = sample.need[req_pos];
        if (newv == oldv) continue;
        ++moves;

        grid[cell] = newv;
        ++visit;
        changed_sid.clear();
        long long ne = energy;
        for (int z = cell_start[cell]; z < cell_start[cell + 1]; ++z) {
            int pi = entry_pi[z];
            unsigned char req = entry_req[z];
            int sid = pl[pi].sid;
            if (seen_sid[sid] != visit) {
                seen_sid[sid] = visit;
                changed_sid.push_back(sid);
            }
            if (oldv != req && newv == req) --bad[pi];
            else if (oldv == req && newv != req) ++bad[pi];
            bool was = match[pi];
            bool isnow = bad[pi] == 0;
            match[pi] = isnow;
            if (was != isnow) {
                sid_hits[sid] += isnow ? 1 : -1;
            }
        }
        for (int sid : changed_sid) {
            bool was = sid_match[sid];
            bool isnow = sid_hits[sid] > 0;
            if (was != isnow) ne += isnow ? -W : W;
        }
        if (oldv == 8) ++ne;      // one more filled cell
        if (newv == 8) --ne;
        long long delta = ne - energy;

        bool accept = delta <= 0 || uniform(gen) < exp(-double(delta) / temp);
        if (accept) {
            energy = ne;
            for (int sid : changed_sid) sid_match[sid] = sid_hits[sid] > 0;
            int unc = ne / W;
            int dots = (int)count(grid.begin(), grid.end(), 8);
            if (unc < best_unc || (unc == best_unc && dots > best_dots)) {
                best_unc = unc;
                best_dots = dots;
                best = grid;
                since_restart = 0;
            }
        } else {
            // Roll back exactly the same edges touched above.
            grid[cell] = oldv;
            ++visit;
            for (int z = cell_start[cell]; z < cell_start[cell + 1]; ++z) {
                int pi = entry_pi[z];
                unsigned char req = entry_req[z];
                int sid = pl[pi].sid;
                if (seen_sid[sid] != visit) {
                    seen_sid[sid] = visit;
                    changed_sid.push_back(sid);
                }
                if (newv != req && oldv == req) --bad[pi];
                else if (newv == req && oldv != req) ++bad[pi];
                bool should = bad[pi] == 0;
                if (match[pi] != should) {
                    match[pi] = should;
                    sid_hits[sid] += should ? 1 : -1;
                }
            }
            for (int sid : changed_sid) sid_match[sid] = sid_hits[sid] > 0;
        }
        if (accept) ++since_restart;

        // Occasionally restart from the incumbent to spend late time in a good basin.
        if (since_restart > 120000) {
            grid = best;
            recount();
            energy = W * (m - covered) + (int)count(grid.begin(), grid.end(), 8);
            since_restart = 0;
        }
    }

    cerr << "unc " << best_unc << '/' << m << " dots " << best_dots << " moves " << moves << '\n';
    for (int i = 0; i < N * N; ++i) {
        char c = (best[i] == 8) ? '.' : char('A' + best[i]);
        cout << c;
        if ((i + 1) % N == 0) cout << '\n';
    }
}
