#include "dent/solver/milp_cuts.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
namespace dent {
GeneratedCut MILPCutManager::rounded_integer_row(const Problem&p,int i)const{
    GeneratedCut c{CutKind::Gomory,"__dent_gomory_"+std::to_string(i),ConstraintSense::LessEqual,std::floor(p.constraints()[i].rhs),p.matrix()[i]};
    return c;
}
GeneratedCut MILPCutManager::mir_integer_row(const Problem&p,int i)const{
    GeneratedCut c{CutKind::MIR,"__dent_mir_"+std::to_string(i),ConstraintSense::LessEqual,std::floor(p.constraints()[i].rhs),p.matrix()[i]};
    return c;
}
GeneratedCut MILPCutManager::cover_row(const Problem&p,int i,const std::vector<double>&x)const{
    std::vector<double>a(p.variables().size(),0);int count=0;double w=0;
    for(size_t j=0;j<a.size();++j)if(p.variables()[j].type==VariableType::Binary&&p.matrix()[i][j]>tolerance_&&x[j]>tolerance_){a[j]=1;w+=p.matrix()[i][j];++count;}
    return {CutKind::Cover,"__dent_cover_"+std::to_string(i),ConstraintSense::LessEqual,std::max(0,count-1),a};
}
std::vector<GeneratedCut> MILPCutManager::generate(const Problem&p,const std::vector<double>&x)const{
    std::vector<GeneratedCut> out;
    for(size_t i=0;i<p.constraints().size();++i){
        const auto&r=p.constraints()[i]; if(r.sense!=ConstraintSense::LessEqual)continue;
        bool allint=true;for(size_t j=0;j<p.variables().size();++j)if(std::abs(p.matrix()[i][j])>tolerance_&&p.variables()[j].type==VariableType::Continuous){allint=false;break;}
        if(allint && std::abs(r.rhs-std::floor(r.rhs))>tolerance_)out.push_back(rounded_integer_row(p,(int)i));
        if(allint && std::abs(r.rhs-std::floor(r.rhs))>tolerance_)out.push_back(mir_integer_row(p,(int)i));
        auto cc=cover_row(p,(int)i,x);int nz=0;for(double v:cc.coefficients)if(std::abs(v)>tolerance_)++nz;if(nz>=2&&std::accumulate(cc.coefficients.begin(),cc.coefficients.end(),0.0)>cc.rhs+tolerance_)out.push_back(std::move(cc));
    }
    return out;
}
bool MILPCutManager::add_all(Problem&p,const std::vector<GeneratedCut>&cuts,int*added)const{int n=0;for(const auto&c:cuts){int r=p.add_constraint(c.name,c.sense,c.rhs);for(size_t j=0;j<c.coefficients.size();++j)if(std::abs(c.coefficients[j])>tolerance_)p.set_constraint_coefficient(r,(int)j,c.coefficients[j]);++n;}if(added)*added=n;return n>0;}
}
