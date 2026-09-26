#pragma once

#include "dent/solver/solver.hpp"

#include <string>
#include <vector>

namespace dent {

class Problem;

struct SolutionValidationResult
{
    bool valid = false;

    bool dimensions_valid = false;
    bool bounds_valid = false;
    bool constraints_valid = false;
    bool objective_valid = false;

    double computed_objective = 0.0;
    double reported_objective = 0.0;

    double max_bound_violation = 0.0;
    double max_constraint_violation = 0.0;
    double objective_error = 0.0;

    std::vector<double> constraint_residuals;

    std::string message;
};

class SolutionValidator
{
public:
    explicit SolutionValidator(
        double tolerance = 1e-7
    );

    SolutionValidationResult validate(
        const Problem& problem,
        const SolveResult& result
    ) const;

    SolutionValidationResult validate_values(
        const Problem& problem,
        const std::vector<double>& values,
        double reported_objective = 0.0
    ) const;

private:
    double tolerance_;
};

} // namespace dent