#include "dent/solver/reliability_branching.hpp"
#include <cmath>
namespace dent {
bool ReliabilityBranching::needs_strong(const PseudoCost&p)const{return p.down_observations<reliability_||p.up_observations<reliability_;}
void ReliabilityBranching::update(PseudoCost&p,bool down,double gain)const{gain=std::max(1e-9,gain);if(down){p.down=(p.down*p.down_observations+gain)/(p.down_observations+1);++p.down_observations;}else{p.up=(p.up*p.up_observations+gain)/(p.up_observations+1);++p.up_observations;}}
int ReliabilityBranching::choose(const Problem&p,const std::vector<double>&x,const std::vector<PseudoCost>&pc)const{int best=-1;double score=-1;for(size_t j=0;j<x.size();++j){if(p.variables()[j].type==VariableType::Continuous)continue;double f=std::abs(x[j]-std::round(x[j]));if(f<1e-9)continue;double s=f*(pc[j].down+pc[j].up);if(s>score){score=s;best=(int)j;}}return best;}
}
