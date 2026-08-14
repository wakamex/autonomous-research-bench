#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false); cin.tie(nullptr);
    int n, L; if (!(cin >> n >> L)) return 0;
    vector<int> t(n); for (int &x : t) cin >> x;
    vector<long long> need(n);
    for (int i=0;i<n;i++) need[i]=2LL*t[i];
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int x,int y){return t[x]>t[y];});
    vector<int> a(n),b(n);
    for (int x:ord) {
        auto pick = [&]() {
            int z=0;
            for (int j=1;j<n;j++) if (need[j]>need[z]) z=j;
            return z;
        };
        a[x]=pick(); need[a[x]]-=t[x];
        b[x]=pick(); need[b[x]]-=t[x];
    }
    auto error = [&](int steps) {
        vector<int> cnt(n);
        int x=0;
        for (int k=0;k<steps;k++) {
            ++cnt[x];
            x = (cnt[x]&1) ? a[x] : b[x];
        }
        long long e=0;
        for(int i=0;i<n;i++) e += abs(1LL*cnt[i]*L-1LL*t[i]*steps);
        return e;
    };
    long long best=error(L);
    vector<int> bestA=a,bestB=b;
    mt19937 rng(712367821);
    struct Cand { long long e; vector<int> a,b; };
    vector<Cand> cand;
    for(int trial=0;trial<512;trial++) {
        int noise = 10 << ((trial/8)%8);
        iota(ord.begin(),ord.end(),0);
        vector<int> key(n);
        for(int i=0;i<n;i++) key[i]=t[i]+int(rng()%(2*noise+1))-noise;
        sort(ord.begin(),ord.end(),[&](int x,int y){return key[x]>key[y];});
        for(int i=0;i<n;i++) need[i]=2LL*t[i];
        for(int x:ord) {
            auto pick = [&]() {
                int z=0;
                for(int j=1;j<n;j++) if(need[j]>need[z]) z=j;
                return z;
            };
            a[x]=pick(); need[a[x]]-=t[x];
            b[x]=pick(); need[b[x]]-=t[x];
            if(rng()&1) swap(a[x],b[x]);
        }
        cand.push_back({error(50000),a,b});
    }
    sort(cand.begin(),cand.end(),[](const Cand&x,const Cand&y){return x.e<y.e;});
    for(int k=0;k<16;k++) {
        a=cand[k].a; b=cand[k].b;
        long long e=error(L);
        if(e<best) best=e,bestA=a,bestB=b;
    }
    a=bestA; b=bestB;
    for(int i=0;i<n;i++) cout<<a[i]<<' '<<b[i]<<'\n';
}
