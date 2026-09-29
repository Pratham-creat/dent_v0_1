#include "dent/numerics/forrest_tomlin.hpp"
#include <algorithm>
#include <cmath>
namespace dent {
bool ForrestTomlin::factorize(const std::vector<std::vector<double>>& a,double tol){
    tolerance_=tol; etas_.clear(); lu_=a; const int n=(int)a.size(); perm_.resize(n);
    for(int i=0;i<n;++i){perm_[i]=i;if((int)a[i].size()!=n)return false;}
    for(int k=0;k<n;++k){
        int p=k; for(int i=k+1;i<n;++i) if(std::abs(lu_[i][k])>std::abs(lu_[p][k]))p=i;
        if(std::abs(lu_[p][k])<=tol)return false; if(p!=k){std::swap(lu_[p],lu_[k]);std::swap(perm_[p],perm_[k]);}
        for(int i=k+1;i<n;++i){double f=lu_[i][k]/lu_[k][k];lu_[i][k]=f;for(int j=k+1;j<n;++j)lu_[i][j]-=f*lu_[k][j];}
    } return true;
}
bool ForrestTomlin::update_eta(int pivot_row,const std::vector<double>& column){
    if(pivot_row<0 || pivot_row>=(int)lu_.size() || (int)column.size()!=(int)lu_.size() || std::abs(column[pivot_row])<=tolerance_) return false;
    Eta e{pivot_row,column}; for(double& v:e.column)v/=column[pivot_row]; etas_.push_back(std::move(e)); return true;
}
bool ForrestTomlin::solve(const std::vector<double>& rhs,std::vector<double>& x) const{
    const int n=(int)lu_.size(); if((int)rhs.size()!=n)return false; std::vector<double> y(n);
    for(int i=0;i<n;++i){y[i]=rhs[perm_[i]];for(int j=0;j<i;++j)y[i]-=lu_[i][j]*y[j];}
    for(const auto& e:etas_){double pivot=y[e.row];for(int i=0;i<n;++i)if(i!=e.row)y[i]-=e.column[i]*pivot;}
    x.assign(n,0);for(int ii=0;ii<n;++ii){int i=n-1-ii;double v=y[i];for(int j=i+1;j<n;++j)v-=lu_[i][j]*x[j];if(std::abs(lu_[i][i])<=tolerance_)return false;x[i]=v/lu_[i][i];}return true;
}
bool ForrestTomlin::solve_transpose(const std::vector<double>& rhs,std::vector<double>& x) const{
    const int n=(int)lu_.size();if((int)rhs.size()!=n)return false;std::vector<std::vector<double>> at(n,std::vector<double>(n));
    for(int i=0;i<n;++i)for(int j=0;j<n;++j)at[i][j]=lu_[j][i];
    ForrestTomlin t;if(!t.factorize(at,tolerance_))return false;return t.solve(rhs,x);
}
std::size_t ForrestTomlin::size()const{return lu_.size();} std::size_t ForrestTomlin::updates()const{return etas_.size();}
}
