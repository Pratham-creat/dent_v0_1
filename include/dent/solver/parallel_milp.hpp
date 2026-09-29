#pragma once
#include "dent/model/problem.hpp"
#include "dent/solver/milp.hpp"
namespace dent {
struct ParallelMILPOptions { int workers=0; int max_nodes=1000; double tolerance=1e-9; };
class ParallelMILPSolver {
public:
 explicit ParallelMILPSolver(ParallelMILPOptions options={}):options_(options){}
 MILPSolution solve(const Problem&) const;
private: ParallelMILPOptions options_;
};
}