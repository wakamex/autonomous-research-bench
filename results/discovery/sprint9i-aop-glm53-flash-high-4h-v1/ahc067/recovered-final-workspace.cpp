#include <bits/stdc++.h>
using namespace std;

struct EdgeOut { int d, i, j, g; };
struct SwitchOut { int i, j, s; };

int N, M, K;
vector<string> grid;
vector<vector<int>> id;
vector<array<int,2>> cell;
vector<vector<int>> adj;

struct DSU {
    vector<int> p, r;
    DSU(int n): p(n), r(n) { iota(p.begin(), p.end(), 0); }
    int find(int x) { while (p[x] != x) x = p[x] = p[p[x]]; return x; }
    bool join(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        if (r[a] < r[b]) swap(a, b);
        p[b] = a;
        if (r[a] == r[b]) r[a]++;
        return true;
    }
};

int tin_[400], low_[400], timer_ = 0;
set<pair<int,int>> bridges;

void dfs_bridges(int v, int pe) {
    tin_[v] = low_[v] = timer_++;
    for (int u : adj[v]) {
        int e = min(v,u) * 400 + max(v,u);
        if (e == pe) continue;
        if (tin_[u] != -1) {
            low_[v] = min(low_[v], tin_[u]);
        } else {
            dfs_bridges(u, e);
            low_[v] = min(low_[v], low_[u]);
            if (low_[u] > tin_[v]) bridges.insert({min(v,u), max(v,u)});
        }
    }
}

int enc(int i, int j, int mask) { return (mask * N + i) * N + j; }

vector<int> seen_version, state_dist, state_prev;
int bfs_version = 0;

long long evaluate(const vector<EdgeOut>& doors, const vector<SwitchOut>& switches) {
    vector<vector<int>> dh(max(0,N-1), vector<int>(N,-1));
    vector<vector<int>> dv(N, vector<int>(max(0,N-1),-1));
    for (const auto& d : doors) {
        if (d.d == 0) dh[d.i][d.j] = d.g;
        else dv[d.i][d.j] = d.g;
    }
    vector<vector<int>> sw(N, vector<int>(N,-1));
    for (const auto& x : switches) sw[x.i][x.j] = x.s;
    int total=(1<<K)*N*N;
    if ((int)seen_version.size()!=total) {
        seen_version.assign(total,0); state_dist.assign(total,0); state_prev.assign(total,-1); bfs_version=0;
    }
    ++bfs_version;
    queue<int> q; int start=enc(0,0,0); seen_version[start]=bfs_version; state_dist[start]=0;
    q.push(start);
    const int di[4]={-1,1,0,0}, dj[4]={0,0,-1,1};
    while (!q.empty()) {
        int h=q.front(); q.pop();
        int mask=h/(N*N), rem=h%(N*N), i=rem/N, j=rem%N;
        if (i==N-1 && j==N-1) return state_dist[h];
        for (int z=0; z<4; z++) {
            int x=i+di[z], y=j+dj[z];
            if (x<0||x>=N||y<0||y>=N||grid[x][y]=='#') continue;
            int g=-1;
            if (z==0) g=dh[x][j]; else if (z==1) g=dh[i][j];
            else if (z==2) g=dv[i][y]; else g=dv[i][j];
            bool open=(g<0)||((((mask>>(g>>1))&1)==(g&1)));
            if (!open) continue;
            int nh=enc(x,y,mask);
            if (seen_version[nh]!=bfs_version) { seen_version[nh]=bfs_version; state_dist[nh]=state_dist[h]+1; q.push(nh); }
        }
        if (sw[i][j]>=0) {
            int nm=mask^(1<<sw[i][j]), nh=enc(i,j,nm);
            if (seen_version[nh]!=bfs_version) { seen_version[nh]=bfs_version; state_dist[nh]=state_dist[h]+1; q.push(nh); }
        }
    }
    return 1;
}

struct BState { int i, j, mask; };

vector<BState> evaluate_path(const vector<EdgeOut>& doors, const vector<SwitchOut>& switches) {
    vector<vector<int>> dh(max(0,N-1), vector<int>(N,-1));
    vector<vector<int>> dv(N, vector<int>(max(0,N-1),-1));
    for (const auto& d : doors) {
        if (d.d == 0) dh[d.i][d.j] = d.g; else dv[d.i][d.j] = d.g;
    }
    vector<vector<int>> sw(N, vector<int>(N,-1));
    for (const auto& x : switches) sw[x.i][x.j] = x.s;
    int total=(1<<K)*N*N;
    if ((int)seen_version.size()!=total) {
        seen_version.assign(total,0); state_dist.assign(total,0); state_prev.assign(total,-1); bfs_version=0;
    }
    ++bfs_version;
    int start=enc(0,0,0), goal=-1;
    seen_version[start]=bfs_version; state_dist[start]=0; state_prev[start]=-1;
    queue<int> q; q.push(start);
    const int di[4]={-1,1,0,0}, dj[4]={0,0,-1,1};
    while (!q.empty()) {
        int h=q.front(); q.pop();
        int mask=h/(N*N), rem=h%(N*N), i=rem/N, j=rem%N;
        if (i==N-1 && j==N-1) { goal=h; break; }
        for (int z=0; z<4; z++) {
            int x=i+di[z], y=j+dj[z];
            if (x<0||x>=N||y<0||y>=N||grid[x][y]=='#') continue;
            int g=-1;
            if (z==0) g=dh[x][j]; else if (z==1) g=dh[i][j];
            else if (z==2) g=dv[i][y]; else g=dv[i][j];
            bool open=(g<0)||((((mask>>(g>>1))&1)==(g&1)));
            if (!open) continue;
            int nh=enc(x,y,mask);
            if (seen_version[nh]!=bfs_version) {
                seen_version[nh]=bfs_version; state_dist[nh]=state_dist[h]+1; state_prev[nh]=h; q.push(nh);
            }
        }
        if (sw[i][j]>=0) {
            int nm=mask^(1<<sw[i][j]), nh=enc(i,j,nm);
            if (seen_version[nh]!=bfs_version) {
                seen_version[nh]=bfs_version; state_dist[nh]=state_dist[h]+1; state_prev[nh]=h; q.push(nh);
            }
        }
    }
    vector<BState> path;
    if (goal<0) return path;
    for (int h=goal; h>=0; h=state_prev[h]) {
        int mask=h/(N*N), rem=h%(N*N);
        path.push_back({rem/N,rem%N,mask});
        if (h==start) break;
    }
    reverse(path.begin(),path.end());
    return path;
}

#ifndef ZERO_LIMIT
#define ZERO_LIMIT 3
#endif

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    if (!(cin >> N >> M >> K)) return 0;
    grid.resize(N);
    for (auto& s : grid) cin >> s;
    id.assign(N, vector<int>(N,-1));
    for (int i=0; i<N; i++) for (int j=0; j<N; j++) if (grid[i][j]=='.') {
        id[i][j] = cell.size(); cell.push_back({i,j});
    }
    int nn = cell.size();
    adj.resize(nn);
    vector<array<int,3>> edges;
    const int di[4]={-1,1,0,0}, dj[4]={0,0,-1,1};
    for (int i=0; i<N; i++) for (int j=0; j<N; j++) if (id[i][j]>=0) {
        for (int z=0; z<4; z++) {
            int x=i+di[z], y=j+dj[z];
            if (x>=0 && x<N && y>=0 && y<N && id[x][y]>=0) {
                int a=id[i][j], b=id[x][y];
                adj[a].push_back(b);
                if (a<b) edges.push_back({a,b,z});
            }
        }
    }
    fill(tin_, tin_+nn, -1);
    dfs_bridges(0,-1);
    DSU dsu(nn);
    for (auto& e : edges) if (!bridges.count({e[0],e[1]})) dsu.join(e[0], e[1]);
    map<pair<int,int>, int> bcomp;
    struct BEdge { int ca, cb, ua, va; };
    vector<BEdge> bedges;
    vector<vector<pair<int,int>>> cadj(nn);
    for (auto& e : edges) {
        if (!bridges.count({e[0],e[1]})) continue;
        int ua=e[0], va=e[1];
        int ca=dsu.find(ua), cb=dsu.find(va);
        if (ca == cb) continue;
        if (ca > cb) { swap(ca,cb); swap(ua,va); }
        pair<int,int> key={ca,cb};
        auto it = bcomp.find(key);
        int ei;
        if (it == bcomp.end()) { ei=bedges.size(); bcomp[key]=ei; bedges.push_back({ca,cb,ua,va}); }
        else ei=it->second;
        cadj[ca].push_back({cb,ei});
        cadj[cb].push_back({ca,ei});
    }
    int s=dsu.find(id[0][0]), t=dsu.find(id[N-1][N-1]);
    vector<int> par(nn,-1), pare(nn,-1);
    queue<int> q; q.push(s); par[s]=s;
    while (!q.empty()) {
        int v=q.front(); q.pop();
        if (v==t) break;
        for (auto [u,e] : cadj[v]) if (par[u]==-1) { par[u]=v; pare[u]=e; q.push(u); }
    }
    vector<int> path_edges;
    for (int v=t; v!=s; v=par[v]) path_edges.push_back(pare[v]);
    reverse(path_edges.begin(), path_edges.end());
    vector<int> child_on_path;
    for (int v=t; v!=s; v=par[v]) child_on_path.push_back(v);
    reverse(child_on_path.begin(), child_on_path.end());

    vector<EdgeOut> doors;
    vector<SwitchOut> switches;
    set<int> used_switch_cells;
    int mask = 0, kind = 0;
    for (size_t qi=0; qi<path_edges.size() && (int)doors.size()<M; qi++) {
        int ei=path_edges[qi], child=child_on_path[qi], parent=par[child];
        const auto& be=bedges[ei];
        int i,j,x,y;
        if (be.ca == parent && be.cb == child) {
            i=cell[be.ua][0]; j=cell[be.ua][1];
            x=cell[be.va][0]; y=cell[be.va][1];
        } else {
            i=cell[be.va][0]; j=cell[be.va][1];
            x=cell[be.ua][0]; y=cell[be.ua][1];
        }
        int z=(i==x)?1:0;
        int cur=id[i][j];
        bool can_switch=used_switch_cells.insert(cur).second;
        int g;
        if (can_switch) {
            int desired=!((mask>>kind)&1);
            switches.push_back({i,j,kind});
            mask^=1<<kind;
            g=desired;
        } else {
            g=((mask>>kind)&1);
        }
        doors.push_back({z, min(i,x), min(j,y), g});
    }
    // Try several deterministic/random greedy orders and keep the best exact result.
    vector<EdgeOut> final_doors=doors;
    vector<SwitchOut> final_switches=switches;
    long long final_score=evaluate(doors,switches);
    auto run_variant=[&](int strategy) {
        auto td=doors; auto ts=switches;
        set<pair<int,int>> used;
        for (const auto& x : ts) used.insert({x.i,x.j});
        mt19937 rng(12345u + strategy * 7919u);
        while ((int)td.size() < M) {
            auto path=evaluate_path(td,ts);
            if (path.size()<2) break;
            long long cur=evaluate(td,ts);
            struct Cand { EdgeOut door; SwitchOut sw; bool has_sw; long long gain; };
            vector<Cand> equal, better;
            for (size_t q=1; q<path.size(); q++) {
                const auto& a=path[q-1]; const auto& b=path[q];
                if (a.i==b.i && a.j==b.j) continue;
                int d=(a.i==b.i), i=min(a.i,b.i), j=min(a.j,b.j);
                bool occupied=false;
                for (const auto& x : td) if (x.d==d && x.i==i && x.j==j) occupied=true;
                if (occupied) continue;
                for (int g=0; g<2; g++) {
                    auto ed=td; ed.push_back({d,i,j,g});
                    long long v=evaluate(ed,ts);
                    if (v==cur) equal.push_back({{d,i,j,g},{0,0,0},false,0});
                    if (!used.count({a.i,a.j})) {
                        auto es=ts; es.push_back({a.i,a.j,0});
                        v=evaluate(ed,es);
                        if (v>cur) better.push_back({{d,i,j,g},{a.i,a.j,0},true,v-cur});
                    }
                }
            }
            int pick=-1;
            if (!better.empty()) {
                long long mg=-1; vector<int> ids;
                for (int z=0; z<(int)better.size(); z++) {
                    if (better[z].gain>mg) { mg=better[z].gain; ids={z}; }
                    else if (better[z].gain==mg) ids.push_back(z);
                }
                pick=ids[rng()%ids.size()];
                const Cand& c=better[pick];
                td.push_back(c.door); ts.push_back(c.sw); used.insert({c.sw.i,c.sw.j});
                continue;
            }
            if (!equal.empty()) {
                if (strategy==0) pick=0;
                else if (strategy==1) pick=(int)equal.size()-1;
                else pick=(int)(rng()%equal.size());
                td.push_back(equal[pick].door);
                continue;
            }
            break;
        }
        long long sc=evaluate(td,ts);
        if (sc>final_score) { final_score=sc; final_doors=td; final_switches=ts; }
    };
    int base_count = (int)doors.size();
    auto started = chrono::steady_clock::now();
    for (int strategy=0; strategy<64; strategy++) {
        if (strategy >= 12 &&
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - started).count() > 1150) break;
        run_variant(strategy);
    }
    // Use remaining time for one-for-one swaps against every non-base edge.
    auto swap_started = chrono::steady_clock::now();
    for (int sweep=0; sweep<3; sweep++) {
        if (chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - swap_started).count() > 480) break;
        long long cur=evaluate(final_doors,final_switches), best=cur;
        int best_remove=-1, best_d=0, best_i=-1, best_j=-1, best_g=0;
        bool stop=false;
        for (int rem=base_count; rem<(int)final_doors.size() && !stop; rem++) {
            auto td=final_doors; td.erase(td.begin()+rem);
            for (const auto& e : edges) {
                int u=e[0], v=e[1];
                int ai=cell[u][0], aj=cell[u][1], bi=cell[v][0], bj=cell[v][1];
                int d=(ai==bi), i=min(ai,bi), j=min(aj,bj);
                bool occupied=false;
                for (const auto& x : td) if (x.d==d && x.i==i && x.j==j) occupied=true;
                if (occupied) continue;
                for (int g=0; g<2; g++) {
                    auto te=td; te.push_back({d,i,j,g});
                    long long v=evaluate(te,final_switches);
                    if (v>best) {
                        best=v; best_remove=rem; best_d=d; best_i=i; best_j=j; best_g=g;
                    }
                }
                if (chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - swap_started).count() > 480) { stop=true; break; }
            }
        }
        if (best_remove < 0 || best <= cur) break;
        final_doors.erase(final_doors.begin()+best_remove);
        final_doors.push_back({best_d,best_i,best_j,best_g});
    }
    doors=final_doors; switches=final_switches;
    doors=final_doors; switches=final_switches;

    cout << doors.size() << '\n';
    for (auto& d : doors) cout << d.d << ' ' << d.i << ' ' << d.j << ' ' << d.g << '\n';
    cout << switches.size() << '\n';
    for (auto& s : switches) cout << s.i << ' ' << s.j << ' ' << s.s << '\n';
}
