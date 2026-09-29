#pragma once
#include "dent/model/problem.hpp"
#include <vector>
namespace dent {
struct FeasibilityPumpResult { bool feasible=false; std::vector<double> values; int iterations=0; };
class FeasibilityPump { public: explicit FeasibilityPump(int max_iterations=30,double tolerance=1e-8):max_iterations_(max_iterations),tolerance_(tolerance){} FeasibilityPumpResult solve(const Problem&,const std::vector<double>&) const; private:int max_iterations_;double tolerance_; };
}
