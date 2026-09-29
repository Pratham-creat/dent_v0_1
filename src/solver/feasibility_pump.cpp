#include "dent/solver/feasibility_pump.hpp"
#include "dent/solver/simplex.hpp"
#include <cmath>
namespace dent {
FeasibilityPumpResult FeasibilityPump::solve(const Problem&p,const std::vector<double>&start)const{
    FeasibilityPumpResult r;r.values=start;if(r.values.size()!=p.variables().size())return r;
    for(int it=0;it<max_iterations_;++it){r.iterations=it+1;for(size_t j=0;j<r.values.size();++j)if(p.variables()[j].type!=VariableType::Continuous)r.values[j]=std::round(r.values[j]);
        bool ok=true;for(size_t i=0;i<p.constraints().size();++i){double lhs=0;for(size_t j=0;j<r.values.size();++j)lhs+=p.matrix()[i][j]*r.values[j];const auto&c=p.constraints()[i];if(c.sense==ConstraintSense::LessEqual)ok&=lhs<=c.rhs+tolerance_;else if(c.sense==ConstraintSense::GreaterEqual)ok&=lhs>=c.rhs-tolerance_;else ok&=std::abs(lhs-c.rhs)<=tolerance_;}
        if(ok){r.feasible=true;return r;}
        for(size_t j=0;j<r.values.size();++j)if(p.variables()[j].type!=VariableType::Continuous)r.values[j]+=((it+j)&1)?0.25:-0.25;
    }return r;
}
}
