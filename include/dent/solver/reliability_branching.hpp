#pragma once
#include "dent/model/problem.hpp"
#include <vector>
namespace dent {
struct PseudoCost { double down=1, up=1; int down_observations=0, up_observations=0; };
class ReliabilityBranching {
public:
    explicit ReliabilityBranching(int reliability=4):reliability_(reliability){}
    int choose(const Problem&,const std::vector<double>&,const std::vector<PseudoCost>&) const;
    bool needs_strong(const PseudoCost&) const;
    void update(PseudoCost&,bool,double) const;
private:int reliability_;
};
}
