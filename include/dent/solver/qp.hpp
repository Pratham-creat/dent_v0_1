#pragma once

#include "dent/model/problem.hpp"
#include "dent/solver/simplex.hpp"

#include <string>
#include <vector>

namespace dent {

struct QPSolution {
    SolveStatus status = SolveStatus::Unsupported;

    double objective_value = 0.0;

    std::vector<double> variable_values;

    int iterations = 0;

    double gradient_norm = 0.0;

    double constraint_violation = 0.0;

    std::string message;
};

class QPSolver {
public:
    explicit QPSolver(
        double tolerance = 1e-8,
        int max_iterations = 10000
    );

    QPSolution solve(
        const Problem& problem
    ) const;

private:
    double tolerance_;
    int max_iterations_;

    double objective_value(
        const Problem& problem,
        const std::vector<double>& x
    ) const;

    std::vector<double> gradient(
        const Problem& problem,
        const std::vector<double>& x
    ) const;

    double vector_norm(
        const std::vector<double>& values
    ) const;

    double constraint_violation(
        const Problem& problem,
        const std::vector<double>& x
    ) const;

    bool satisfies_constraints(
        const Problem& problem,
        const std::vector<double>& x
    ) const;

    std::vector<double> project_to_constraints(
        const Problem& problem,
        const std::vector<double>& x
    ) const;
};

} // namespace dent