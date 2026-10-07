// AHC050 "Gamble on Ice" — greedy: each step place a rock on the free cell
// with minimal robot probability mass (post-slide).
#include <bits/stdc++.h>
using namespace std;

int main(int argc, char** argv) {
    int n, m;
    if (scanf("%d %d", &n, &m) != 2) return 0;
    vector<string> g(n);
    for (int i = 0; i < n; i++) {
        char buf[256];
        scanf("%s", buf);
        g[i] = buf;
    }
    int K = n * n - m;
    vector<char> rock(n * n, 0);
    vector<double> p(n * n, 0.0), nxt(n * n);
    vector<pair<int,int>> free_cells;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (g[i][j] == '.') {
                rock[i * n + j] = 0;
                p[i * n + j] = 1.0 / K;
                free_cells.push_back({i, j});
            } else {
                rock[i * n + j] = 1;
            }
    string out;
    out.reserve(K * 8);
    double life = 1.0;
    vector<char> done(n * n, 0);
    for (int t = 0; t < K; t++) {
        // q = slide(p): segment accumulation, both directions in one walk
        fill(nxt.begin(), nxt.end(), 0.0);
        for (int i = 0; i < n; i++) {
            int j = 0;
            int base = i * n;
            while (j < n) {
                if (rock[base + j]) { j++; continue; }
                int js = j;
                double sum = 0.0;
                while (j < n && !rock[base + j]) { sum += p[base + j]; j++; }
                int je = j - 1;
                double q = 0.25 * sum;
                nxt[base + je] += q;
                nxt[base + js] += q;
            }
        }
        for (int j = 0; j < n; j++) {
            int i = 0;
            while (i < n) {
                if (rock[i * n + j]) { i++; continue; }
                int is = i;
                double sum = 0.0;
                while (i < n && !rock[i * n + j]) { sum += p[i * n + j]; i++; }
                int ie = i - 1;
                double q = 0.25 * sum;
                nxt[is * n + j] += q;
                nxt[ie * n + j] += q;
            }
        }
        // argmin over free cells
        int best = -1;
        double bestv = 1e300;
        for (auto& c : free_cells) {
            int id = c.first * n + c.second;
            if (done[id]) continue;
            if (nxt[id] < bestv) { bestv = nxt[id]; best = id; }
        }
        // place
        int bi = best / n, bj = best % n;
        life -= nxt[best];
        nxt[best] = 0.0;
        rock[best] = 1;
        done[best] = 1;
        p.swap(nxt);
        out += to_string(bi);
        out += ' ';
        out += to_string(bj);
        out += '\n';
    }
    fwrite(out.data(), 1, out.size(), stdout);
    return 0;
}
