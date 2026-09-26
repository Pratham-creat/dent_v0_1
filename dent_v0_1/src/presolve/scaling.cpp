#include "dent/presolve/scaling.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace dent
{

namespace
{

constexpr double EPS = 1e-12;

bool is_finite_upper_bound(
    double upper_bound
)
{
    return
        upper_bound != 0.0 &&
        std::isfinite(upper_bound);
}

} // namespace


GeometricScaler::GeometricScaler(
    double tolerance,
    int max_iterations
)
    : tolerance_(tolerance),
      max_iterations_(max_iterations)
{
    if (tolerance_ <= 0.0)
    {
        throw std::invalid_argument(
            "Scaling tolerance must be positive."
        );
    }

    if (max_iterations_ <= 0)
    {
        throw std::invalid_argument(
            "Scaling iteration count must be positive."
        );
    }
}


bool GeometricScaler::approximately_one(
    double value
) const
{
    return
        std::abs(value - 1.0) <=
        tolerance_;
}


std::vector<double>
GeometricScaler::compute_row_scales(
    const Problem& problem,
    const std::vector<double>& column_scale
) const
{
    const auto& matrix =
        problem.matrix();

    const int rows =
        static_cast<int>(
            problem.constraints().size()
        );

    const int columns =
        static_cast<int>(
            problem.variables().size()
        );

    std::vector<double> row_scale(
        rows,
        1.0
    );

    for (int i = 0;
         i < rows;
         ++i)
    {
        double norm = 0.0;

        for (int j = 0;
             j < columns;
             ++j)
        {
            const double value =
                std::abs(
                    matrix[i][j] *
                    column_scale[j]
                );

            norm =
                std::max(
                    norm,
                    value
                );
        }

        if (
            norm > EPS &&
            std::isfinite(norm)
        )
        {
            row_scale[i] =
                1.0 /
                std::sqrt(norm);
        }
    }

    return row_scale;
}


std::vector<double>
GeometricScaler::compute_column_scales(
    const Problem& problem,
    const std::vector<double>& row_scale
) const
{
    const auto& matrix =
        problem.matrix();

    const auto& variables =
        problem.variables();

    const auto& quadratic =
        problem.quadratic_matrix();

    const int rows =
        static_cast<int>(
            problem.constraints().size()
        );

    const int columns =
        static_cast<int>(
            variables.size()
        );

    std::vector<double> column_scale(
        columns,
        1.0
    );

    for (int j = 0;
         j < columns;
         ++j)
    {
        /*
         * Never scale integer/binary columns.
         * Scaling them would change their integer
         * semantics.
         */
        if (
            variables[j].type !=
            VariableType::Continuous
        )
        {
            continue;
        }

        double norm = 0.0;

        for (int i = 0;
             i < rows;
             ++i)
        {
            const double value =
                std::abs(
                    row_scale[i] *
                    matrix[i][j]
                );

            norm =
                std::max(
                    norm,
                    value
                );
        }

        /*
         * Include Q when this is a QP.
         */
        for (int k = 0;
             k < columns;
             ++k)
        {
            const double value =
                std::abs(
                    quadratic[k][j]
                );

            norm =
                std::max(
                    norm,
                    value
                );
        }

        if (
            norm > EPS &&
            std::isfinite(norm)
        )
        {
            column_scale[j] =
                1.0 /
                std::sqrt(norm);
        }
    }

    return column_scale;
}


Problem GeometricScaler::build_scaled_problem(
    const Problem& problem,
    const std::vector<double>& row_scale,
    const std::vector<double>& column_scale
) const
{
    Problem scaled(
        problem.objective_sense()
    );

    const auto& variables =
        problem.variables();

    const auto& constraints =
        problem.constraints();

    const auto& matrix =
        problem.matrix();

    const auto& objective =
        problem.objective();

    const auto& quadratic =
        problem.quadratic_matrix();

    const int rows =
        static_cast<int>(
            constraints.size()
        );

    const int columns =
        static_cast<int>(
            variables.size()
        );

    if (
        static_cast<int>(
            row_scale.size()
        ) != rows
    )
    {
        throw std::invalid_argument(
            "Row scaling dimension mismatch."
        );
    }

    if (
        static_cast<int>(
            column_scale.size()
        ) != columns
    )
    {
        throw std::invalid_argument(
            "Column scaling dimension mismatch."
        );
    }

    /*
     * x = D z
     *
     * Therefore:
     *
     * lower(z) = lower(x) / D
     * upper(z) = upper(x) / D
     */
    for (int j = 0;
         j < columns;
         ++j)
    {
        const Variable& variable =
            variables[j];

        const double scale =
            column_scale[j];

        if (
            scale <= 0.0 ||
            !std::isfinite(scale)
        )
        {
            throw std::runtime_error(
                "Invalid column scaling factor."
            );
        }

        const double lower =
            variable.lower_bound /
            scale;

        double upper = 0.0;

        if (
            is_finite_upper_bound(
                variable.upper_bound
            )
        )
        {
            upper =
                variable.upper_bound /
                scale;
        }

        /*
         * Integer and binary variables use
         * scale 1.0, so their semantics remain
         * unchanged.
         */
        scaled.add_variable(
            variable.name,
            lower,
            upper,
            variable.type
        );
    }

    /*
     * c' = D c
     */
    for (int j = 0;
         j < columns;
         ++j)
    {
        scaled.set_objective_coefficient(
            j,
            objective[j] *
            column_scale[j]
        );
    }

    /*
     * A' = D_r A D_c
     * b' = D_r b
     */
    for (int i = 0;
         i < rows;
         ++i)
    {
        const Constraint& constraint =
            constraints[i];

        const int new_constraint =
            scaled.add_constraint(
                constraint.name,
                constraint.sense,
                constraint.rhs *
                row_scale[i]
            );

        for (int j = 0;
             j < columns;
             ++j)
        {
            const double value =
                matrix[i][j] *
                row_scale[i] *
                column_scale[j];

            if (
                std::abs(value) >
                EPS
            )
            {
                scaled.set_constraint_coefficient(
                    new_constraint,
                    j,
                    value
                );
            }
        }
    }

    /*
     * Q' = D Q D
     */
    for (int i = 0;
         i < columns;
         ++i)
    {
        for (int j = 0;
             j < columns;
             ++j)
        {
            const double value =
                quadratic[i][j] *
                column_scale[i] *
                column_scale[j];

            if (
                std::abs(value) >
                EPS
            )
            {
                scaled.set_quadratic_coefficient(
                    i,
                    j,
                    value
                );
            }
        }
    }

    return scaled;
}


ScalingResult GeometricScaler::scale(
    const Problem& problem
) const
{
    ScalingResult result{
        Problem(problem.objective_sense())
    };

    const int rows =
        static_cast<int>(
            problem.constraints().size()
        );

    const int columns =
        static_cast<int>(
            problem.variables().size()
        );

    result.row_scale.assign(
        rows,
        1.0
    );

    result.column_scale.assign(
        columns,
        1.0
    );

    if (
        rows == 0 ||
        columns == 0
    )
    {
        result.scaled_problem =
            problem;

        result.message =
            "Scaling skipped: empty model.";

        return result;
    }

    std::vector<double> accumulated_rows(
        rows,
        1.0
    );

    std::vector<double> accumulated_columns(
        columns,
        1.0
    );

    Problem current =
        problem;

    for (
        int iteration = 0;
        iteration < max_iterations_;
        ++iteration
    )
    {
        /*
         * IMPORTANT:
         *
         * current already contains all scaling
         * from previous iterations.
         *
         * Therefore compute the next row step
         * from current with NO accumulated
         * column factor applied again.
         */
        const std::vector<double> row_step =
            compute_row_scales(
                current,
                std::vector<double>(
                    columns,
                    1.0
                )
            );

        /*
         * Apply only the new row step.
         */
        for (int i = 0;
             i < rows;
             ++i)
        {
            accumulated_rows[i] *=
                row_step[i];
        }

        /*
         * Build the row-scaled intermediate
         * problem before calculating columns.
         */
        std::vector<double> current_rows =
            accumulated_rows;

        current =
            build_scaled_problem(
                problem,
                current_rows,
                accumulated_columns
            );

        /*
         * Now calculate the incremental column
         * step from the row-scaled model.
         */
        const std::vector<double> column_step =
            compute_column_scales(
                current,
                std::vector<double>(
                    rows,
                    1.0
                )
            );

        for (int j = 0;
             j < columns;
             ++j)
        {
            accumulated_columns[j] *=
                column_step[j];
        }

        /*
         * Build the complete transformed model.
         */
        current =
            build_scaled_problem(
                problem,
                accumulated_rows,
                accumulated_columns
            );

        result.iterations =
            iteration + 1;

        bool converged = true;

        for (double value : row_step)
        {
            if (
                !approximately_one(value)
            )
            {
                converged = false;
                break;
            }
        }

        if (converged)
        {
            for (double value : column_step)
            {
                if (
                    !approximately_one(value)
                )
                {
                    converged = false;
                    break;
                }
            }
        }

        if (converged)
        {
            break;
        }
    }

    result.row_scale =
        accumulated_rows;

    result.column_scale =
        accumulated_columns;

    result.scaled_problem =
        build_scaled_problem(
            problem,
            result.row_scale,
            result.column_scale
        );

    result.applied = true;

    for (double value :
         result.row_scale)
    {
        if (
            !approximately_one(value)
        )
        {
            ++result.rows_scaled;
        }
    }

    for (double value :
         result.column_scale)
    {
        if (
            !approximately_one(value)
        )
        {
            ++result.columns_scaled;
        }
    }

    result.message =
        "Geometric scaling applied successfully.";

    return result;
}


std::vector<double>
ScalingResult::postsolve_values(
    const std::vector<double>& scaled_values
) const
{
    if (
        scaled_values.size() !=
        column_scale.size()
    )
    {
        throw std::invalid_argument(
            "Scaled solution size does not "
            "match scaling metadata."
        );
    }

    std::vector<double> original_values(
        scaled_values.size(),
        0.0
    );

    for (
        std::size_t j = 0;
        j < scaled_values.size();
        ++j
    )
    {
        if (
            std::abs(
                column_scale[j]
            ) <= EPS
        )
        {
            throw std::runtime_error(
                "Invalid zero column "
                "scaling factor."
            );
        }

        /*
         * x = D z
         */
        original_values[j] =
            scaled_values[j] *
            column_scale[j];
    }

    return original_values;
}


} // namespace dent