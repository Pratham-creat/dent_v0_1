#pragma once

#include "dent/model/problem.hpp"

#include <string>
#include <vector>

namespace dent {

enum class SolveStatus {
    Optimal,
    Infeasible,
    Unbounded,
    IterationLimit,
    Unsupported
};

struct SolveResult {
    SolveStatus status = SolveStatus::Unsupported;

    double objective_value = 0.0;

    std::vector<double> variable_values;

    int iterations = 0;

    std::string message;
};

class SimplexSolver {
public:
    explicit SimplexSolver(
        double tolerance = 1e-9,
        int max_iterations = 10000
    );

    SolveResult solve(
        const Problem& problem
    ) const;

private:
    double tolerance_;
    int max_iterations_;
};

} // namespace dent