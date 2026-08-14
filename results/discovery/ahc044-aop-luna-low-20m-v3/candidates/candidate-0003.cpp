#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false); cin.tie(nullptr);
    int n, L; if (!(cin >> n >> L)) return 0;
    vector<int> t(n); for (int &x : t) cin >> x;
    vector<long long> need(n);
    for (int i=0;i<n;i++) need[i]=2LL*t[i];
    vector<int> ord(n); iota(ord.begin(),ord.end(),0);
    sort(ord.begin(),ord.end(),[&](int x,int y){return t[x]<t[y];});
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
    for(int i=0;i<n;i++) cout<<a[i]<<' '<<b[i]<<'\n';
}
