#include "dent/solver/simplex_pivot.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace dent {
int choose_harris_leaving(const std::vector<double>& rhs,const std::vector<double>& column,double tol){
    double theta=std::numeric_limits<double>::infinity();
    for(size_t i=0;i<rhs.size()&&i<column.size();++i)if(column[i]>tol)theta=std::min(theta,std::max(0.0,rhs[i]/column[i]));
    if(!std::isfinite(theta))return -1;
    int best=-1; double best_rhs=-std::numeric_limits<double>::infinity();
    const double harris=theta+std::max(tol,1e-7);
    for(size_t i=0;i<rhs.size()&&i<column.size();++i)if(column[i]>tol && rhs[i]/column[i]<=harris && rhs[i]>best_rhs){best_rhs=rhs[i];best=(int)i;}
    return best;
}
int choose_steepest_edge_entering(const std::vector<double>& rc,const std::vector<std::vector<double>>& rows,double tol){
    int best=-1; double score=0;
    for(size_t j=0;j<rc.size();++j){if(rc[j]>=-tol)continue;double norm=1.0;for(const auto&r:rows)if(j<r.size())norm+=r[j]*r[j];double s=(-rc[j])/std::sqrt(norm);if(s>score){score=s;best=(int)j;}}
    return best;
}
void apply_objective_perturbation(std::vector<double>& rc,double eps){for(size_t i=0;i<rc.size();++i)if(std::abs(rc[i])<eps)rc[i]=-(eps*(1.0+static_cast<double>(i)*1e-6));}
}
