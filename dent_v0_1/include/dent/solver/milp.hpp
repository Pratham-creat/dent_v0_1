#pragma once

#include "dent/model/problem.hpp"
#include "dent/solver/simplex.hpp"

#include <string>
#include <vector>

namespace dent {

struct MILPSolution {
    SolveStatus status = SolveStatus::Unsupported;

    double objective_value = 0.0;

    std::vector<double> variable_values;

    int nodes_explored = 0;

    int nodes_pruned = 0;

    int lp_solves = 0;

    std::string message;
};

class MILPSolver {
public:
    explicit MILPSolver(
        double tolerance = 1e-9,
        int max_nodes = 1000
    );

    MILPSolution solve(
        const Problem& problem
    ) const;

private:
    double tolerance_;

    int max_nodes_;

    bool is_integer_problem(
        const Problem& problem
    ) const;

    bool is_integral_solution(
        const Problem& problem,
        const std::vector<double>& values
    ) const;

    int choose_branch_variable(
        const Problem& problem,
        const std::vector<double>& values
    ) const;

    void branch_and_bound(
        const Problem& problem,
        MILPSolution& result,
        bool& has_incumbent,
        double& incumbent_objective,
        std::vector<double>& incumbent_values
    ) const;
};

} // namespace dent