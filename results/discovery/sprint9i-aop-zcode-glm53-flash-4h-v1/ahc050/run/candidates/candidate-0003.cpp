// AHC050 "Gamble on Ice": greedy min-mass placement with structural tie-break.
// Each step places a rock on a free cell with robot-mass <= min + WINDOW,
// maximizing (rock-or-wall neighbor count), then proximity of free neighbors
// to existing rocks. Falls back to pure argmin when min > WINDOW.
#include <bits/stdc++.h>
using namespace std;
static const double WINDOW = 3e-5;
int main(){
    int n, M;
    if(scanf("%d %d",&n,&M)!=2)return 0;
    vector<string> g(n);
    for(int i=0;i<n;i++){char buf[256];scanf("%s",buf);g[i]=buf;}
    const int K=n*n-M;
    vector<char> rock(n*n,0);
    vector<pair<int,int>> fc;
    for(int i=0;i<n;i++)for(int j=0;j<n;j++){if(g[i][j]=='#')rock[i*n+j]=1;else fc.push_back({i,j});}
    // BFS distance to nearest initial rock
    vector<int> distr(n*n,-1);
    {
        queue<int> bfs;
        for(int id=0;id<n*n;id++)if(rock[id]){distr[id]=0;bfs.push(id);}
        int dirs[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
        while(!bfs.empty()){
            int id=bfs.front();bfs.pop();
            int i=id/n,j=id%n;
            for(auto&dd:dirs){
                int i2=i+dd[0],j2=j+dd[1];
                if(i2<0||i2>=n||j2<0||j2>=n)continue;
                int id2=i2*n+j2;
                if(distr[id2]<0){distr[id2]=distr[id]+1;bfs.push(id2);}
            }
        }
        for(int id=0;id<n*n;id++)if(distr[id]<0)distr[id]=0;
    }
    vector<double> p(n*n,0.0),nxt(n*n);
    double inv=1.0/K;
    for(auto&c:fc)p[c.first*n+c.second]=inv;
    string out;
    out.reserve(K*8);
    for(int t=0;t<K;t++){
        fill(nxt.begin(),nxt.end(),0.0);
        for(int i=0;i<n;i++){int j=0,base=i*n;
            while(j<n){if(rock[base+j]){j++;continue;}int js=j;double sum=0;
                while(j<n&&!rock[base+j]){sum+=p[base+j];j++;}double q=0.25*sum;
                nxt[base+j-1]+=q;nxt[base+js]+=q;}}
        for(int j=0;j<n;j++){int i=0;
            while(i<n){if(rock[i*n+j]){i++;continue;}int is=i;double sum=0;
                while(i<n&&!rock[i*n+j]){sum+=p[i*n+j];i++;}double q=0.25*sum;
                nxt[is*n+j]+=q;nxt[(i-1)*n+j]+=q;}}
        double bestv=1e300;
        for(int id=0;id<n*n;id++){ if(rock[id])continue; if(nxt[id]<bestv)bestv=nxt[id]; }
        double hi=bestv+WINDOW;
        int best=-1; double bs=-1e300;
        for(int id=0;id<n*n;id++){
            if(rock[id])continue;
            if(nxt[id]>hi)continue;
            int i=id/n,j=id%n;
            int nbw=0;
            if(i==0||rock[id-n])nbw++;
            if(i==n-1||rock[id+n])nbw++;
            if(j==0||rock[id-1])nbw++;
            if(j==n-1||rock[id+1])nbw++;
            double nbstop2=0;
            int ni[4]={i-1,i+1,i,i}, nj[4]={j,j,j-1,j+1};
            for(int d=0;d<4;d++){
                int i2=ni[d],j2=nj[d];
                if(i2<0||i2>=n||j2<0||j2>=n)continue;
                int id2=i2*n+j2;
                if(rock[id2])continue;
                nbstop2+=distr[id2];
            }
            double sc=nbw*1e4-nbstop2/4;
            if(sc>bs){bs=sc;best=id;}
        }
        nxt[best]=0.0;rock[best]=1;p.swap(nxt);
        out+=to_string(best/n);out+=' ';out+=to_string(best%n);out+='\n';
    }
    fwrite(out.data(),1,out.size(),stdout);
    return 0;
}
