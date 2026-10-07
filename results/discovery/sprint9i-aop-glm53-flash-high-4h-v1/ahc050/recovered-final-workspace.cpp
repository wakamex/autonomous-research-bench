#include <bits/stdc++.h>
using namespace std;
int n,m,K,DX[4]={-1,1,0,0},DY[4]={0,0,-1,1};
vector<string> S0,S;vector<array<int,4>> st;
void init(){S=S0;for(int p=0;p<n*n;p++)for(int d=0;d<4;d++){if(S[p/n][p%n]=='#'){st[p][d]=-1;continue;}int x=p/n,y=p%n;while(1){int tx=x+DX[d],ty=y+DY[d];if(tx<0||tx>=n||ty<0||ty>=n||S0[tx][ty]=='#')break;x=tx;y=ty;}st[p][d]=x*n+y;}}
void apply(int c,vector<double>&q){int x=c/n,y=c%n;S[x][y]='#';for(int d=0;d<4;d++){int px=x,py=y;while(px>=0&&px<n&&py>=0&&py<n){int p=px*n+py;if(st[p][d]!=c)break;int nx=px,ny=py;while(1){int tx=nx+DX[d],ty=ny+DY[d];if(tx<0||tx>=n||ty<0||ty>=n||S[tx][ty]=='#')break;nx=tx;ny=ty;}int ns=nx*n+ny;double v=q[p]*0.25;q[p]-=v;q[ns]+=v;st[p][d]=ns;px-=DX[d];py-=DY[d];}}}
pair<vector<int>,double> run(int LL,double A){
 init();vector<double> q(n*n);for(int p=0;p<n*n;p++)if(S[p/n][p%n]=='.')q[p]=1.0/K;
 vector<int> ord;ord.reserve(K);double life=1,ret=0;
 for(int t=0;t<K;t++){
  int idx[64],z=0;
  for(int p=0;p<n*n;p++)if(q[p]>1e-15){
   if(z<LL){int i=z++;while(i&&q[idx[i-1]]<q[p]){idx[i]=idx[i-1];i--;}idx[i]=p;}
   else if(q[p]>q[idx[LL-1]]){int i=LL-1;idx[i]=p;while(i&&q[idx[i-1]]<q[p]){idx[i]=idx[i-1];i--;}idx[i]=p;}
  }
  double bestv=-1e100;int best=idx[0];
  for(int i=0;i<z;i++){
   int c=idx[i];auto S2=S;auto st2=st;vector<double> q2=q;q2[c]=0;apply(c,q2);S=S2;st=st2;double mx=0;for(double v:q2)mx=max(mx,v);double ev=q[c]+A*mx;if(ev>bestv){bestv=ev;best=c;}
  }
  ord.push_back(best);ret+=life-q[best];life-=q[best];q[best]=0;apply(best,q);
 }
 return {ord,ret};
}
double official(const vector<int>&ord){
 vector<char> S(n*n);for(int p=0;p<n*n;p++)S[p]=(S0[p/n][p%n]=='#');
 vector<double> crt(n*n),nx(n*n);double u=1.0/K;for(int p=0;p<n*n;p++)if(!S[p])crt[p]=u;
 double life=1,ret=0;
 for(int c:ord){fill(nx.begin(),nx.end(),0.0);
  for(int p=0;p<n*n;p++){if(crt[p]==0)continue;int x=p/n,y=p%n;for(int d=0;d<4;d++){int i2=x,j2=y;while(1){int tx=i2+DX[d],ty=j2+DY[d];if(tx<0||tx>=n||ty<0||ty>=n||S[tx*n+ty])break;i2=tx;j2=ty;}nx[i2*n+j2]+=crt[p]*0.25;}}
  swap(crt,nx);life-=crt[c];crt[c]=0;S[c]=1;ret+=life;}
 return ret;
}
int main(){ios::sync_with_stdio(false);cin.tie(0);cin>>n>>m;S0.resize(n);for(auto&x:S0)cin>>x;K=n*n-m;st.assign(n*n,{});
 vector<pair<int,double>> ps{{14,-0.40},{14,-0.46},{14,-0.50},{13,-0.40},{13,-0.50},{13,-0.75},{13,-0.32},{14,-0.36},{13,-0.28},{13,-0.24},{11,-0.18},{11,-0.20},{12,-0.24},{15,-0.40}};
 vector<int> best;double bs=-1;for(auto p:ps){auto r=run(p.first,p.second);if(true){r.second=official(r.first);if(r.second>bs){bs=r.second;best=r.first;}}}for(int x:best)cout<<x/n<<' '<<x%n<<'\n';
}
