#include <bits/stdc++.h>
using namespace std;
using ll = long long;

/*
 * AHC053 "Random Sum Game"  (N=500 cards, M=50 targets uniform in [L,U], sorted).
 *
 * Pile j's sum is scored against the j-th smallest target B_(j), whose
 * distribution is Beta(j+1, 50-j) scaled to [L, U].  Cards are committed before
 * the targets are seen and pile indices are fixed, so each pile must
 * independently approximate a draw from that distribution.
 *
 * Kit per pile: one "big" card p_j plus n_j ladder cards g_j*{1,2,4,...,2^(n_j-1)}.
 * Reachable sums are exactly p_j + t*g_j for t in [0, 2^n_j - 1]: a uniform
 * tiling of the window [p_j, p_j + (2^n_j-1)*g_j] (complete sequence).
 *
 * Constants minimize  E[dist outside window] + width/(4*(2^n-1))  against the
 * exact Beta order-statistic distribution (analytic incomplete-beta tails),
 * plus global reallocation of the 450 ladder cards (near-balanced by convexity;
 * narrower windows at the edges where the order statistic concentrates).
 */

static const int P_N[50] = {8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,8,8,7};
static const ll P_P[50] = {998000078469168,998003630693993,998015418831861,998029025590513,998052134736395,998080348338858,998112827411400,998148943573477,998188221410627,998230294440489,998274875616105,998321736398611,998370692039547,998421591635540,998474311829017,998528749549664,998584819663066,998642451333745,998701585675867,998762174494506,998824178177281,998847001555580,998910390646076,998975181690883,999041363552453,999108931214829,999177886414284,999248236460999,999368824796870,999442954222589,999518478980981,999595433785555,999673861657880,999753816048308,999835360924087,999918574306771,1000003551198511,1000090405768866,1000179278184087,1000270343536345,1000363821302041,1000459999727230,1000559276910992,1000662255476666,1000769998063042,1000884774664669,1001012367200620,1001205105892886,1001403387339780,1001699177405694};
static const ll P_G[50] = {2029202807,2661361098,3159603458,1902891102,2089169243,2252784663,2398165328,2528349754,2645521662,2751309304,2846958960,2933447997,3011556540,3081914997,3145038104,3201350081,3251202559,3294886386,3332643183,3364670273,3391130174,1791791310,1799973394,1805407509,1808118581,1808118324,1805407519,1799973655,3412150911,3391130375,3364670419,3332642781,3294886348,3251201822,3201348891,3145035933,3081910210,3011547690,2933429864,2846917641,2751217074,2645307985,2527831317,2396846103,2249239906,2079120729,1873174846,3049221215,2314319803,2355921914};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ll N, M, L, U;
    if (!(cin >> N >> M >> L >> U)) return 0;

    {
        string out;
        out.reserve(N * 15);
        for (int j = 0; j < M; j++) {
            out += to_string(P_P[j]);
            for (int k = 0; k < P_N[j]; k++) {
                out += ' ';
                out += to_string(P_G[j] << k);
            }
            out += ' ';
        }
        out.back() = '\n';
        cout << out << flush;
    }

    vector<ll> B(M);
    for (auto &x : B) cin >> x;
    sort(B.begin(), B.end());

    string out;
    out.reserve(N * 3);
    for (int j = 0; j < M; j++) {
        const int n = P_N[j];
        const ll span = (1LL << n) - 1;
        const ll g = P_G[j];
        ll t = 0;
        if (B[j] > P_P[j]) {
            t = (B[j] - P_P[j] + g / 2) / g;
            t = min(t, span);
        }
        out += to_string(j + 1); // big card first, matching A order
        out += ' ';
        for (int k = 0; k < n; k++) {
            out += to_string((t >> k) & 1 ? j + 1 : 0);
            out += ' ';
        }
    }
    out.back() = '\n';
    cout << out << flush;
    return 0;
}
