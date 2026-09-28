#include "dent/dispatch/fingerprint.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace dent {

namespace {

constexpr double kEpsilon = 1e-12;
constexpr double kPoorScalingRatio = 1e6;

double safe_ratio(double minimum, double maximum)
{
    if (minimum <= kEpsilon || maximum <= kEpsilon) {
        return 1.0;
    }

    return maximum / minimum;
}

double vector_norm(const std::vector<double>& values)
{
    double sum = 0.0;

    for (double value : values) {
        sum += value * value;
    }

    return std::sqrt(sum);
}

} // namespace

ProblemFingerprint fingerprint_problem(const Problem& problem)
{
    ProblemFingerprint fingerprint;

    const auto& variables = problem.variables();
    const auto& constraints = problem.constraints();
    const auto& matrix = problem.matrix();
    const auto& quadratic = problem.quadratic_matrix();

    fingerprint.variables =
        static_cast<int>(variables.size());

    fingerprint.constraints =
        static_cast<int>(constraints.size());

    /*
     * ------------------------------------------------------------
     * Matrix statistics
     * ------------------------------------------------------------
     */

    double min_nonzero = std::numeric_limits<double>::infinity();
    double max_nonzero = 0.0;

    int nonzeros = 0;

    for (const auto& row : matrix) {
        for (double value : row) {
            if (std::abs(value) > kEpsilon) {
                ++nonzeros;

                const double magnitude = std::abs(value);

                min_nonzero =
                    std::min(min_nonzero, magnitude);

                max_nonzero =
                    std::max(max_nonzero, magnitude);
            }
        }
    }

    fingerprint.nonzeros = nonzeros;

    const long long total_entries =
        static_cast<long long>(fingerprint.variables) *
        static_cast<long long>(fingerprint.constraints);

    if (total_entries > 0) {
        fingerprint.density =
            static_cast<double>(nonzeros) /
            static_cast<double>(total_entries);
    }

    fingerprint.highly_sparse =
        fingerprint.density < 0.10;

    fingerprint.large_problem =
        fingerprint.variables >= 1000 ||
        fingerprint.constraints >= 1000;

    if (nonzeros > 0) {
        fingerprint.min_nonzero_coefficient = min_nonzero;
        fingerprint.max_nonzero_coefficient = max_nonzero;

        fingerprint.coefficient_ratio =
            safe_ratio(min_nonzero, max_nonzero);
    }

    /*
     * ------------------------------------------------------------
     * Row norms
     * ------------------------------------------------------------
     */

    double min_row_norm = std::numeric_limits<double>::infinity();
    double max_row_norm = 0.0;

    for (const auto& row : matrix) {
        const double norm = vector_norm(row);

        if (norm > kEpsilon) {
            min_row_norm =
                std::min(min_row_norm, norm);

            max_row_norm =
                std::max(max_row_norm, norm);
        }
    }

    if (max_row_norm > 0.0) {
        fingerprint.min_row_norm =
            min_row_norm;

        fingerprint.max_row_norm =
            max_row_norm;

        fingerprint.row_norm_ratio =
            safe_ratio(min_row_norm, max_row_norm);
    }

    /*
     * ------------------------------------------------------------
     * Column norms
     * ------------------------------------------------------------
     */

    std::vector<double> column_squared_norms(
        fingerprint.variables,
        0.0
    );

    for (const auto& row : matrix) {
        for (std::size_t j = 0;
             j < row.size() &&
             j < column_squared_norms.size();
             ++j) {

            column_squared_norms[j] +=
                row[j] * row[j];
        }
    }

    double min_column_norm =
        std::numeric_limits<double>::infinity();

    double max_column_norm = 0.0;

    for (double squared_norm : column_squared_norms) {
        const double norm =
            std::sqrt(squared_norm);

        if (norm > kEpsilon) {
            min_column_norm =
                std::min(min_column_norm, norm);

            max_column_norm =
                std::max(max_column_norm, norm);
        }
    }

    if (max_column_norm > 0.0) {
        fingerprint.min_column_norm =
            min_column_norm;

        fingerprint.max_column_norm =
            max_column_norm;

        fingerprint.column_norm_ratio =
            safe_ratio(
                min_column_norm,
                max_column_norm
            );
    }

    /*
     * ------------------------------------------------------------
     * RHS statistics
     * ------------------------------------------------------------
     */

    double min_rhs = std::numeric_limits<double>::infinity();
    double max_rhs = 0.0;

    for (const auto& constraint : constraints) {
        const double magnitude =
            std::abs(constraint.rhs);

        if (magnitude > kEpsilon) {
            min_rhs =
                std::min(min_rhs, magnitude);

            max_rhs =
                std::max(max_rhs, magnitude);
        }
    }

    if (max_rhs > 0.0) {
        fingerprint.min_rhs = min_rhs;
        fingerprint.max_rhs = max_rhs;

        fingerprint.rhs_ratio =
            safe_ratio(min_rhs, max_rhs);
    }

    /*
     * ------------------------------------------------------------
     * Numerical scaling classification
     * ------------------------------------------------------------
     */

    fingerprint.poorly_scaled =
        fingerprint.coefficient_ratio >= kPoorScalingRatio ||
        fingerprint.row_norm_ratio >= kPoorScalingRatio ||
        fingerprint.column_norm_ratio >= kPoorScalingRatio;

    /*
     * ------------------------------------------------------------
     * Quadratic objective
     * ------------------------------------------------------------
     */

    int quadratic_nonzeros = 0;

    for (const auto& row : quadratic) {
        for (double value : row) {
            if (std::abs(value) > kEpsilon) {
                ++quadratic_nonzeros;
            }
        }
    }

    fingerprint.quadratic_nonzeros =
        quadratic_nonzeros;

    fingerprint.has_quadratic_objective =
        quadratic_nonzeros > 0;

    fingerprint.is_quadratic =
        fingerprint.has_quadratic_objective;

    /*
     * ------------------------------------------------------------
     * Variable types
     * ------------------------------------------------------------
     */

    for (const auto& variable : variables) {
        switch (variable.type) {

        case VariableType::Continuous:
            ++fingerprint.continuous_variables;
            break;

        case VariableType::Integer:
            ++fingerprint.integer_variables;
            break;

        case VariableType::Binary:
            ++fingerprint.binary_variables;
            break;
        }

        /*
         * In this project an upper bound of 0 means
         * +infinity for the prototype model representation.
         */
        if (variable.upper_bound != 0.0) {
            ++fingerprint.bounded_variables;
        } else {
            ++fingerprint.unbounded_variables;
        }
    }

    fingerprint.is_mixed_integer =
        fingerprint.integer_variables > 0 ||
        fingerprint.binary_variables > 0;

    /*
     * ------------------------------------------------------------
     * Constraint senses
     * ------------------------------------------------------------
     */

    for (const auto& constraint : constraints) {
        switch (constraint.sense) {

        case ConstraintSense::LessEqual:
            ++fingerprint.less_equal_constraints;
            break;

        case ConstraintSense::Equal:
            ++fingerprint.equal_constraints;
            break;

        case ConstraintSense::GreaterEqual:
            ++fingerprint.greater_equal_constraints;
            break;
        }
    }

    /*
     * ------------------------------------------------------------
     * Overall classification
     * ------------------------------------------------------------
     */

    fingerprint.is_linear =
        !fingerprint.has_quadratic_objective;

    /*
     * ------------------------------------------------------------
     * Human-readable structure string
     * ------------------------------------------------------------
     */

    std::ostringstream structure;

    if (fingerprint.is_mixed_integer &&
        fingerprint.is_quadratic) {

        structure << "MIQP";

    } else if (fingerprint.is_mixed_integer) {

        structure << "MILP";

    } else if (fingerprint.is_quadratic) {

        structure << "QP";

    } else {

        structure << "LP";
    }

    structure
        << ", vars=" << fingerprint.variables
        << ", constraints=" << fingerprint.constraints
        << ", nonzeros=" << fingerprint.nonzeros
        << ", density=" << fingerprint.density;

    if (fingerprint.highly_sparse) {
        structure << ", highly_sparse";
    }

    if (fingerprint.large_problem) {
        structure << ", large";
    }

    if (fingerprint.poorly_scaled) {
        structure << ", poorly_scaled";
    }

    fingerprint.structure =
        structure.str();

    return fingerprint;
}

} // namespace dent