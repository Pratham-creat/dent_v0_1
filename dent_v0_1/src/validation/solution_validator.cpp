#include "dent/validation/solution_validator.hpp"

#include "dent/model/problem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace dent {

namespace {

double evaluate_linear_objective(
    const Problem& problem,
    const std::vector<double>& values
)
{
    double value = 0.0;

    const auto& objective =
        problem.objective();

    for (
        std::size_t i = 0;
        i < objective.size() &&
        i < values.size();
        ++i
    ) {
        value +=
            objective[i] *
            values[i];
    }

    return value;
}

double evaluate_quadratic_objective(
    const Problem& problem,
    const std::vector<double>& values
)
{
    const auto& quadratic =
        problem.quadratic_matrix();

    double value =
        evaluate_linear_objective(
            problem,
            values
        );

    const std::size_t n =
        values.size();

    /*
     * DENT's QP convention is:
     *
     *     1/2 x^T Q x + c^T x
     *
     * Therefore the quadratic contribution
     * is multiplied by 1/2.
     */
    for (
        std::size_t i = 0;
        i < n &&
        i < quadratic.size();
        ++i
    ) {
        for (
            std::size_t j = 0;
            j < n &&
            j < quadratic[i].size();
            ++j
        ) {
            value +=
                0.5 *
                quadratic[i][j] *
                values[i] *
                values[j];
        }
    }

    return value;
}

double evaluate_objective(
    const Problem& problem,
    const std::vector<double>& values
)
{
    return evaluate_quadratic_objective(
        problem,
        values
    );
}

double constraint_activity(
    const Problem& problem,
    int row,
    const std::vector<double>& values
)
{
    double activity = 0.0;

    const auto& matrix =
        problem.matrix();

    if (
        row < 0 ||
        row >= static_cast<int>(matrix.size())
    ) {
        return activity;
    }

    const auto& coefficients =
        matrix[row];

    for (
        std::size_t j = 0;
        j < coefficients.size() &&
        j < values.size();
        ++j
    ) {
        activity +=
            coefficients[j] *
            values[j];
    }

    return activity;
}

double constraint_violation(
    ConstraintSense sense,
    double activity,
    double rhs
)
{
    switch (sense)
    {
        case ConstraintSense::LessEqual:
            return std::max(
                0.0,
                activity - rhs
            );

        case ConstraintSense::GreaterEqual:
            return std::max(
                0.0,
                rhs - activity
            );

        case ConstraintSense::Equal:
            return std::abs(
                activity - rhs
            );
    }

    return std::numeric_limits<double>::infinity();
}

} // namespace


SolutionValidator::SolutionValidator(
    double tolerance
)
    : tolerance_(
        std::max(
            tolerance,
            0.0
        )
    )
{
}


SolutionValidationResult
SolutionValidator::validate(
    const Problem& problem,
    const SolveResult& result
) const
{
    SolutionValidationResult validation;

    validation.reported_objective =
        result.objective_value;

    if (
        result.status !=
        SolveStatus::Optimal
    ) {
        validation.message =
            "Solution verification skipped because "
            "the solver did not report an optimal solution.";

        return validation;
    }

    return validate_values(
        problem,
        result.variable_values,
        result.objective_value
    );
}


SolutionValidationResult
SolutionValidator::validate_values(
    const Problem& problem,
    const std::vector<double>& values,
    double reported_objective
) const
{
    SolutionValidationResult validation;

    validation.reported_objective =
        reported_objective;

    const auto& variables =
        problem.variables();

    const auto& constraints =
        problem.constraints();

    /*
     * ------------------------------------------------------------
     * Dimension validation
     * ------------------------------------------------------------
     */

    validation.dimensions_valid =
        values.size() ==
        variables.size();

    if (
        !validation.dimensions_valid
    ) {
        std::ostringstream message;

        message
            << "Variable vector size mismatch: expected "
            << variables.size()
            << ", received "
            << values.size()
            << ".";

        validation.message =
            message.str();

        return validation;
    }

    /*
     * ------------------------------------------------------------
     * Finite-value validation
     * ------------------------------------------------------------
     */

    for (
        double value :
        values
    ) {
        if (
            !std::isfinite(value)
        ) {
            validation.message =
                "Solution contains a non-finite "
                "variable value.";

            return validation;
        }
    }

    /*
     * ------------------------------------------------------------
     * Variable bound validation
     * ------------------------------------------------------------
     */

    validation.bounds_valid =
        true;

    double max_bound_violation =
        0.0;

    for (
        std::size_t i = 0;
        i < variables.size();
        ++i
    ) {
        const auto& variable =
            variables[i];

        const double value =
            values[i];

        const double lower_violation =
            std::max(
                0.0,
                variable.lower_bound -
                    value
            );

        max_bound_violation =
            std::max(
                max_bound_violation,
                lower_violation
            );

        /*
         * DENT uses upper_bound == 0 as
         * the prototype representation of
         * +infinity.
         */
        if (
            variable.upper_bound != 0.0 &&
            std::isfinite(
                variable.upper_bound
            )
        ) {
            const double upper_violation =
                std::max(
                    0.0,
                    value -
                        variable.upper_bound
                );

            max_bound_violation =
                std::max(
                    max_bound_violation,
                    upper_violation
                );
        }

        if (
            variable.type ==
            VariableType::Binary
        ) {
            const double binary_violation =
                std::min(
                    std::abs(value),
                    std::abs(value - 1.0)
                );

            /*
             * Binary values are checked against
             * the actual allowed set {0, 1}.
             */
            const double distance_to_binary =
                std::min(
                    std::abs(value),
                    std::abs(value - 1.0)
                );

            max_bound_violation =
                std::max(
                    max_bound_violation,
                    distance_to_binary
                );
        }

        if (
            variable.type ==
            VariableType::Integer
        ) {
            const double integer_violation =
                std::abs(
                    value -
                    std::round(value)
                );

            max_bound_violation =
                std::max(
                    max_bound_violation,
                    integer_violation
                );
        }
    }

    validation.max_bound_violation =
        max_bound_violation;

    validation.bounds_valid =
        max_bound_violation <=
        tolerance_;

    /*
     * ------------------------------------------------------------
     * Constraint validation
     * ------------------------------------------------------------
     */

    validation.constraints_valid =
        true;

    validation.max_constraint_violation =
        0.0;

    validation.constraint_residuals.clear();

    validation.constraint_residuals.reserve(
        constraints.size()
    );

    for (
        std::size_t i = 0;
        i < constraints.size();
        ++i
    ) {
        const auto& constraint =
            constraints[i];

        const double activity =
            constraint_activity(
                problem,
                static_cast<int>(i),
                values
            );

        const double violation =
            constraint_violation(
                constraint.sense,
                activity,
                constraint.rhs
            );

        validation.constraint_residuals.push_back(
            violation
        );

        validation.max_constraint_violation =
            std::max(
                validation.max_constraint_violation,
                violation
            );
    }

    validation.constraints_valid =
        validation.max_constraint_violation <=
        tolerance_;

    /*
     * ------------------------------------------------------------
     * Objective validation
     * ------------------------------------------------------------
     */

    validation.computed_objective =
        evaluate_objective(
            problem,
            values
        );

    if (
        std::isfinite(
            reported_objective
        )
    ) {
        validation.objective_error =
            std::abs(
                validation.computed_objective -
                reported_objective
            );
    }
    else {
        validation.objective_error =
            std::numeric_limits<double>::infinity();
    }

    /*
     * Objective values can become large.
     *
     * Use both an absolute and relative tolerance:
     *
     *     |computed - reported|
     *         <= tol * max(1, |computed|, |reported|)
     */
    const double objective_scale =
        std::max(
            {
                1.0,
                std::abs(
                    validation.computed_objective
                ),
                std::abs(
                    reported_objective
                )
            }
        );

    const double objective_tolerance =
        tolerance_ *
        objective_scale;

    validation.objective_valid =
        validation.objective_error <=
        objective_tolerance;

    /*
     * ------------------------------------------------------------
     * Final validation status
     * ------------------------------------------------------------
     */

    validation.valid =
        validation.dimensions_valid &&
        validation.bounds_valid &&
        validation.constraints_valid &&
        validation.objective_valid;

    if (
        validation.valid
    ) {
        validation.message =
            "Solution passed numerical verification.";
    }
    else {
        std::ostringstream message;

        message
            << "Solution verification failed.";

        if (
            !validation.dimensions_valid
        ) {
            message
                << " Variable dimensions are invalid.";
        }

        if (
            !validation.bounds_valid
        ) {
            message
                << " Maximum bound violation = "
                << validation.max_bound_violation
                << ".";
        }

        if (
            !validation.constraints_valid
        ) {
            message
                << " Maximum constraint violation = "
                << validation.max_constraint_violation
                << ".";
        }

        if (
            !validation.objective_valid
        ) {
            message
                << " Objective mismatch = "
                << validation.objective_error
                << ".";
        }

        validation.message =
            message.str();
    }

    return validation;
}

} // namespace dent