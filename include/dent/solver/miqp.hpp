#pragma once

#include "dent/model/problem.hpp"
#include "dent/solver/qp.hpp"

#include <string>
#include <vector>

namespace dent {

struct MIQPSolution {
    SolveStatus status = SolveStatus::Unsupported;
    double objective_value = 0.0;
    std::vector<double> variable_values;
    int nodes_explored = 0;
    int nodes_pruned = 0;
    double relative_gap = 0.0;
    std::string message;
};

class MIQPSolver {
public:
    explicit MIQPSolver(double tolerance = 1e-8, int max_nodes = 1000);
    MIQPSolution solve(const Problem& problem) const;

private:
    double tolerance_;
    int max_nodes_;
    bool integral(const Problem&, const std::vector<double>&) const;
    int branch_variable(const Problem&, const std::vector<double>&) const;
    bool better(ObjectiveSense, double, double) const;
    bool can_improve(ObjectiveSense, double, double) const;
    Problem branch_down(const Problem&, int, double) const;
    Problem branch_up(const Problem&, int, double) const;
};

} // namespace dent
