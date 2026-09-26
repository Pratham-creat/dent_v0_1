#include "dent/presolve/presolve.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace dent
{

namespace
{

constexpr double INF =
    std::numeric_limits<double>::infinity();


double variable_upper_bound(
    const Variable& variable
)
{
    /*
     * DENT convention:
     *
     * upper_bound == 0
     *
     * means +infinity.
     */
    if (variable.upper_bound == 0.0)
    {
        return INF;
    }

    return variable.upper_bound;
}


bool has_finite_upper_bound(
    const Variable& variable
)
{
    return variable.upper_bound != 0.0;
}


double clamp_value(
    double value,
    double lower,
    double upper
)
{
    if (value < lower)
    {
        return lower;
    }

    if (
        std::isfinite(upper) &&
        value > upper
    )
    {
        return upper;
    }

    return value;
}

} // anonymous namespace


// ============================================================
// Constructor
// ============================================================

Presolver::Presolver(
    double tolerance,
    int max_passes
)
    : tolerance_(tolerance),
      max_passes_(max_passes)
{
}


// ============================================================
// Utility functions
// ============================================================

bool Presolver::approximately_equal(
    double a,
    double b
) const
{
    return std::abs(a - b) <= tolerance_;
}


bool Presolver::is_zero(
    double value
) const
{
    return std::abs(value) <= tolerance_;
}


bool Presolver::is_finite(
    double value
) const
{
    return std::isfinite(value);
}


// ============================================================
// Bound tightening
// ============================================================

bool Presolver::tighten_single_variable_constraints(
    Problem& problem,
    PresolveStatistics& statistics,
    bool& infeasible
) const
{
    infeasible = false;

    bool changed = false;

    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    const int m =
        static_cast<int>(
            problem.constraints().size()
        );


    for (
        int row = 0;
        row < m;
        ++row
    )
    {
        int nonzero_column = -1;

        double coefficient = 0.0;

        const auto& matrix =
            problem.matrix()[row];


        /*
         * Detect whether the row has exactly
         * one nonzero coefficient.
         */
        for (
            int column = 0;
            column < n;
            ++column
        )
        {
            if (is_zero(matrix[column]))
            {
                continue;
            }

            if (nonzero_column != -1)
            {
                nonzero_column = -2;
                break;
            }

            nonzero_column = column;

            coefficient =
                matrix[column];
        }


        /*
         * Not a single-variable constraint.
         */
        if (nonzero_column < 0)
        {
            continue;
        }


        const int variable_index =
            nonzero_column;

        const auto& constraint =
            problem.constraints()[row];

        const double rhs =
            constraint.rhs;


        if (
            !is_finite(rhs) ||
            !is_finite(coefficient)
        )
        {
            continue;
        }


        Variable variable =
            problem.variables()[
                variable_index
            ];


        double lower =
            variable.lower_bound;

        double upper =
            variable_upper_bound(variable);


        const double implied =
            rhs / coefficient;


        switch (constraint.sense)
        {
        case ConstraintSense::LessEqual:

            if (coefficient > 0.0)
            {
                upper =
                    std::min(
                        upper,
                        implied
                    );
            }
            else
            {
                lower =
                    std::max(
                        lower,
                        implied
                    );
            }

            break;


        case ConstraintSense::GreaterEqual:

            if (coefficient > 0.0)
            {
                lower =
                    std::max(
                        lower,
                        implied
                    );
            }
            else
            {
                upper =
                    std::min(
                        upper,
                        implied
                    );
            }

            break;


        case ConstraintSense::Equal:

            lower = implied;
            upper = implied;

            break;
        }


        /*
         * Integer variables require integral bounds.
         */
        if (
            variable.type ==
                VariableType::Integer ||
            variable.type ==
                VariableType::Binary
        )
        {
            lower =
                std::ceil(
                    lower - tolerance_
                );

            if (std::isfinite(upper))
            {
                upper =
                    std::floor(
                        upper + tolerance_
                    );
            }
        }


        /*
         * Binary variables always satisfy:
         *
         *     0 <= x <= 1
         */
        if (
            variable.type ==
            VariableType::Binary
        )
        {
            lower =
                std::max(
                    lower,
                    0.0
                );

            upper =
                std::min(
                    upper,
                    1.0
                );
        }


        /*
         * Contradictory bounds.
         */
        if (
            std::isfinite(upper) &&
            lower > upper + tolerance_
        )
        {
            infeasible = true;

            return false;
        }


        const double old_lower =
            variable.lower_bound;

        const double old_upper =
            variable_upper_bound(variable);


        bool bound_changed = false;


        if (
            !approximately_equal(
                old_lower,
                lower
            )
        )
        {
            bound_changed = true;
        }


        if (std::isfinite(upper))
        {
            if (
                !std::isfinite(old_upper) ||
                !approximately_equal(
                    old_upper,
                    upper
                )
            )
            {
                bound_changed = true;
            }
        }


        if (bound_changed)
        {
            /*
             * Keep the tightened bounds inside
             * the working model.
             *
             * They will be converted into explicit
             * constraints when the reduced model
             * is constructed.
             */
            const double stored_upper =
                std::isfinite(upper)
                    ? upper
                    : 0.0;

            problem.set_variable_bounds(
                variable_index,
                lower,
                stored_upper
            );

            ++statistics.bounds_tightened;

            changed = true;
        }
    }


    return changed;
}


// ============================================================
// Trivial constraints
// ============================================================

bool Presolver::detect_trivial_constraints(
    const Problem& problem,
    std::vector<bool>& removable_constraints,
    bool& infeasible
) const
{
    infeasible = false;

    const int m =
        static_cast<int>(
            problem.constraints().size()
        );

    const int n =
        static_cast<int>(
            problem.variables().size()
        );


    removable_constraints.assign(
        m,
        false
    );


    for (
        int row = 0;
        row < m;
        ++row
    )
    {
        bool has_nonzero = false;


        for (
            int column = 0;
            column < n;
            ++column
        )
        {
            if (
                !is_zero(
                    problem.matrix()[row][column]
                )
            )
            {
                has_nonzero = true;
                break;
            }
        }


        if (has_nonzero)
        {
            continue;
        }


        const Constraint& constraint =
            problem.constraints()[row];

        const double rhs =
            constraint.rhs;


        bool satisfied = false;


        switch (constraint.sense)
        {
        case ConstraintSense::LessEqual:

            satisfied =
                0.0 <=
                rhs + tolerance_;

            break;


        case ConstraintSense::Equal:

            satisfied =
                std::abs(rhs) <=
                tolerance_;

            break;


        case ConstraintSense::GreaterEqual:

            satisfied =
                0.0 >=
                rhs - tolerance_;

            break;
        }


        if (!satisfied)
        {
            infeasible = true;

            return false;
        }


        removable_constraints[row] =
            true;
    }


    return true;
}


// ============================================================
// Row scaling
// ============================================================

void Presolver::scale_rows(
    Problem& problem,
    PresolveStatistics& statistics
) const
{
    const int m =
        static_cast<int>(
            problem.constraints().size()
        );

    const int n =
        static_cast<int>(
            problem.variables().size()
        );


    /*
     * NOTE:
     *
     * Problem currently does not expose a direct
     * set_constraint_rhs() operation.
     *
     * Therefore actual scaling is performed while
     * constructing the reduced model.
     *
     * This method remains as the logical scaling
     * stage and statistics hook.
     */
    (void)problem;
    (void)statistics;
    (void)m;
    (void)n;
}


// ============================================================
// Build reduced model
// ============================================================

Problem Presolver::build_reduced_problem(
    const Problem& problem,
    const std::vector<bool>& fixed_variables,
    const std::vector<double>& fixed_values,
    const std::vector<bool>& removable_constraints,
    const Problem& original_problem,
    PresolveResult& result
) const
{
    const int original_n =
        static_cast<int>(
            problem.variables().size()
        );

    const int original_m =
        static_cast<int>(
            problem.constraints().size()
        );


    Problem reduced(
        problem.objective_sense()
    );


    // --------------------------------------------------------
    // Variable mapping
    // --------------------------------------------------------

    result.original_to_reduced.assign(
        original_n,
        -1
    );

    result.reduced_to_original.clear();


    for (
        int i = 0;
        i < original_n;
        ++i
    )
    {
        if (fixed_variables[i])
        {
            continue;
        }


        const Variable& variable =
            problem.variables()[i];


        /*
         * Important:
         *
         * Do NOT pass presolve-generated finite
         * bounds directly to Simplex.
         *
         * Current Simplex works with the default
         * nonnegative variable representation.
         *
         * Tightened bounds are represented below
         * as explicit linear constraints.
         */
        const Variable& original_variable =
            original_problem.variables()[i];


        const int reduced_index =
            reduced.add_variable(
                variable.name,
                original_variable.lower_bound,
                original_variable.upper_bound,
                variable.type
            );


        result.original_to_reduced[i] =
            reduced_index;

        result.reduced_to_original.push_back(i);
    }


    // --------------------------------------------------------
    // Linear objective
    // --------------------------------------------------------

    const auto& c =
        problem.objective();

    const auto& Q =
        problem.quadratic_matrix();


    for (
        int original_i = 0;
        original_i < original_n;
        ++original_i
    )
    {
        if (fixed_variables[original_i])
        {
            continue;
        }


        const int reduced_i =
            result.original_to_reduced[
                original_i
            ];


        double coefficient =
            c[original_i];


        /*
         * Fixed-variable contribution:
         *
         * 1/2 x^T Q x
         *
         * gives:
         *
         * 1/2(Qij + Qji) x_i x_j
         */
        for (
            int fixed_j = 0;
            fixed_j < original_n;
            ++fixed_j
        )
        {
            if (!fixed_variables[fixed_j])
            {
                continue;
            }


            coefficient +=
                0.5 *
                (
                    Q[original_i][fixed_j] +
                    Q[fixed_j][original_i]
                ) *
                fixed_values[fixed_j];
        }


        reduced.set_objective_coefficient(
            reduced_i,
            coefficient
        );
    }


    // --------------------------------------------------------
    // Objective constant
    // --------------------------------------------------------

    double objective_offset =
        0.0;


    for (
        int i = 0;
        i < original_n;
        ++i
    )
    {
        if (!fixed_variables[i])
        {
            continue;
        }


        objective_offset +=
            c[i] *
            fixed_values[i];
    }


    /*
     * Fixed-fixed quadratic contribution.
     */
    for (
        int i = 0;
        i < original_n;
        ++i
    )
    {
        if (!fixed_variables[i])
        {
            continue;
        }


        for (
            int j = 0;
            j < original_n;
            ++j
        )
        {
            if (!fixed_variables[j])
            {
                continue;
            }


            objective_offset +=
                0.5 *
                fixed_values[i] *
                Q[i][j] *
                fixed_values[j];
        }
    }


    result.objective_offset =
        objective_offset;


    // --------------------------------------------------------
    // Quadratic matrix
    // --------------------------------------------------------

    for (
        int original_i = 0;
        original_i < original_n;
        ++original_i
    )
    {
        if (fixed_variables[original_i])
        {
            continue;
        }


        const int reduced_i =
            result.original_to_reduced[
                original_i
            ];


        for (
            int original_j = 0;
            original_j < original_n;
            ++original_j
        )
        {
            if (fixed_variables[original_j])
            {
                continue;
            }


            const int reduced_j =
                result.original_to_reduced[
                    original_j
                ];


            const double coefficient =
                Q[original_i][original_j];


            if (!is_zero(coefficient))
            {
                reduced.set_quadratic_coefficient(
                    reduced_i,
                    reduced_j,
                    coefficient
                );
            }
        }
    }


    // --------------------------------------------------------
    // Original constraints
    // --------------------------------------------------------

    for (
        int original_row = 0;
        original_row < original_m;
        ++original_row
    )
    {
        if (
            removable_constraints[
                original_row
            ]
        )
        {
            continue;
        }


        const Constraint& original_constraint =
            problem.constraints()[
                original_row
            ];


        double rhs =
            original_constraint.rhs;


        /*
         * Move fixed variables to RHS.
         */
        for (
            int original_column = 0;
            original_column < original_n;
            ++original_column
        )
        {
            if (
                !fixed_variables[
                    original_column
                ]
            )
            {
                continue;
            }


            rhs -=
                problem.matrix()[
                    original_row
                ][
                    original_column
                ] *
                fixed_values[
                    original_column
                ];
        }


        /*
         * Find maximum coefficient magnitude.
         */
        double row_scale =
            0.0;


        for (
            int original_column = 0;
            original_column < original_n;
            ++original_column
        )
        {
            if (
                fixed_variables[
                    original_column
                ]
            )
            {
                continue;
            }


            row_scale =
                std::max(
                    row_scale,
                    std::abs(
                        problem.matrix()[
                            original_row
                        ][
                            original_column
                        ]
                    )
                );
        }


        /*
         * Fixed-variable substitution may have
         * converted the row into a constant row.
         */
        if (
            row_scale <= tolerance_
        )
        {
            bool satisfied = false;


            switch (
                original_constraint.sense
            )
            {
            case ConstraintSense::LessEqual:

                satisfied =
                    0.0 <=
                    rhs + tolerance_;

                break;


            case ConstraintSense::Equal:

                satisfied =
                    std::abs(rhs) <=
                    tolerance_;

                break;


            case ConstraintSense::GreaterEqual:

                satisfied =
                    0.0 >=
                    rhs - tolerance_;

                break;
            }


            if (!satisfied)
            {
                result.status =
                    PresolveStatus::Infeasible;

                result.message =
                    "Presolve detected an infeasible "
                    "constraint after fixed-variable "
                    "elimination.";

                return reduced;
            }


            ++result.statistics.constraints_removed;

            continue;
        }


        /*
         * ----------------------------------------------------
         * Row scaling
         *
         *     A x <= b
         *
         * becomes
         *
         *     (A/s)x <= b/s
         *
         * where
         *
         *     s = max |Aij|
         * ----------------------------------------------------
         */
        double scale =
            row_scale;


        if (
            scale <= tolerance_
        )
        {
            scale = 1.0;
        }


        if (
            std::abs(scale - 1.0) >
            tolerance_
        )
        {
            ++result.statistics.rows_scaled;
        }
        else
        {
            scale = 1.0;
        }


        const int reduced_row =
            reduced.add_constraint(
                original_constraint.name,
                original_constraint.sense,
                rhs / scale
            );


        for (
            int original_column = 0;
            original_column < original_n;
            ++original_column
        )
        {
            if (
                fixed_variables[
                    original_column
                ]
            )
            {
                continue;
            }


            const double coefficient =
                problem.matrix()[
                    original_row
                ][
                    original_column
                ];


            if (is_zero(coefficient))
            {
                continue;
            }


            const int reduced_column =
                result.original_to_reduced[
                    original_column
                ];


            reduced.set_constraint_coefficient(
                reduced_row,
                reduced_column,
                coefficient / scale
            );
        }
    }


    // --------------------------------------------------------
    // Preserve presolve-tightened bounds as explicit
    // constraints instead of passing them directly to
    // the current Simplex implementation.
    // --------------------------------------------------------

    for (
        int original_i = 0;
        original_i < original_n;
        ++original_i
    )
    {
        if (fixed_variables[original_i])
        {
            continue;
        }


        const int reduced_i =
            result.original_to_reduced[
                original_i
            ];


        const Variable& working_variable =
            problem.variables()[original_i];

        const Variable& original_variable =
            original_problem.variables()[original_i];


        const double tightened_lower =
            working_variable.lower_bound;

        const double tightened_upper =
            variable_upper_bound(
                working_variable
            );


        const double original_lower =
            original_variable.lower_bound;

        const double original_upper =
            variable_upper_bound(
                original_variable
            );


        /*
         * Tightened lower bound.
         *
         * x >= L
         */
        if (
            tightened_lower >
            original_lower + tolerance_
        )
        {
            const int row =
                reduced.add_constraint(
                    working_variable.name +
                        "_presolve_lower",
                    ConstraintSense::GreaterEqual,
                    tightened_lower
                );


            reduced.set_constraint_coefficient(
                row,
                reduced_i,
                1.0
            );
        }


        /*
         * Tightened finite upper bound.
         *
         * x <= U
         */
        if (
            std::isfinite(tightened_upper) &&
            (
                !std::isfinite(original_upper) ||
                tightened_upper <
                    original_upper -
                    tolerance_
            )
        )
        {
            const int row =
                reduced.add_constraint(
                    working_variable.name +
                        "_presolve_upper",
                    ConstraintSense::LessEqual,
                    tightened_upper
                );


            reduced.set_constraint_coefficient(
                row,
                reduced_i,
                1.0
            );
        }
    }


    return reduced;
}


// ============================================================
// Main presolve routine
// ============================================================

PresolveResult Presolver::run(
    const Problem& problem
) const
{
    PresolveResult result{
        PresolveStatus::Success,
        Problem(problem.objective_sense())
    };


    result.statistics.original_variables =
        static_cast<int>(
            problem.variables().size()
        );

    result.statistics.original_constraints =
        static_cast<int>(
            problem.constraints().size()
        );


    const int n =
        result.statistics.original_variables;

    const int m =
        result.statistics.original_constraints;


    // --------------------------------------------------------
    // Basic validation
    // --------------------------------------------------------

    if (n < 0 || m < 0)
    {
        result.status =
            PresolveStatus::Unsupported;

        result.message =
            "Invalid problem dimensions.";

        return result;
    }


    if (
        static_cast<int>(
            problem.objective().size()
        ) != n
    )
    {
        result.status =
            PresolveStatus::Unsupported;

        result.message =
            "Objective dimension does not match "
            "the number of variables.";

        return result;
    }


    if (
        static_cast<int>(
            problem.matrix().size()
        ) != m
    )
    {
        result.status =
            PresolveStatus::Unsupported;

        result.message =
            "Constraint matrix row count does "
            "not match the number of constraints.";

        return result;
    }


    for (
        const auto& row :
        problem.matrix()
    )
    {
        if (
            static_cast<int>(
                row.size()
            ) != n
        )
        {
            result.status =
                PresolveStatus::Unsupported;

            result.message =
                "Constraint matrix column count "
                "does not match the number of variables.";

            return result;
        }
    }


    // --------------------------------------------------------
    // Validate Q
    // --------------------------------------------------------

    if (
        static_cast<int>(
            problem.quadratic_matrix().size()
        ) != n
    )
    {
        result.status =
            PresolveStatus::Unsupported;

        result.message =
            "Quadratic matrix dimension does not "
            "match the number of variables.";

        return result;
    }


    for (
        const auto& row :
        problem.quadratic_matrix()
    )
    {
        if (
            static_cast<int>(
                row.size()
            ) != n
        )
        {
            result.status =
                PresolveStatus::Unsupported;

            result.message =
                "Quadratic matrix must be square.";

            return result;
        }
    }


    // --------------------------------------------------------
    // Work on a copy
    // --------------------------------------------------------

    Problem working =
        problem;


    // --------------------------------------------------------
    // Phase 1: bound tightening
    // --------------------------------------------------------

    for (
        int pass = 0;
        pass < max_passes_;
        ++pass
    )
    {
        bool infeasible =
            false;


        const bool changed =
            tighten_single_variable_constraints(
                working,
                result.statistics,
                infeasible
            );


        if (infeasible)
        {
            result.status =
                PresolveStatus::Infeasible;

            result.message =
                "Presolve detected contradictory "
                "variable bounds.";

            result.fixed_values.assign(
                n,
                0.0
            );

            result.original_to_reduced.assign(
                n,
                -1
            );

            return result;
        }


        if (!changed)
        {
            break;
        }
    }


    // --------------------------------------------------------
    // Detect fixed variables
    // --------------------------------------------------------

    std::vector<bool> fixed_variables(
        n,
        false
    );

    std::vector<double> fixed_values(
        n,
        0.0
    );


    for (
        int i = 0;
        i < n;
        ++i
    )
    {
        const Variable& variable =
            working.variables()[i];


        const double upper =
            variable_upper_bound(variable);


        if (!std::isfinite(upper))
        {
            continue;
        }


        if (
            approximately_equal(
                variable.lower_bound,
                upper
            )
        )
        {
            fixed_variables[i] =
                true;

            fixed_values[i] =
                clamp_value(
                    variable.lower_bound,
                    variable.lower_bound,
                    upper
                );

            ++result.statistics.fixed_variables;
        }
    }


    // --------------------------------------------------------
    // Detect trivial constraints
    // --------------------------------------------------------

    std::vector<bool>
        removable_constraints;

    bool infeasible =
        false;


    detect_trivial_constraints(
        working,
        removable_constraints,
        infeasible
    );


    if (infeasible)
    {
        result.status =
            PresolveStatus::Infeasible;

        result.message =
            "Presolve detected an infeasible "
            "trivial constraint.";

        result.fixed_values =
            fixed_values;

        result.original_to_reduced.assign(
            n,
            -1
        );

        return result;
    }


    for (
        bool removable :
        removable_constraints
    )
    {
        if (removable)
        {
            ++result.statistics
                .trivial_constraints_removed;
        }
    }


    // --------------------------------------------------------
    // Build reduced model
    // --------------------------------------------------------

    result.fixed_values =
        fixed_values;


    Problem reduced =
        build_reduced_problem(
            working,
            fixed_variables,
            fixed_values,
            removable_constraints,
            problem,
            result
        );


    if (
        result.status ==
        PresolveStatus::Infeasible
    )
    {
        return result;
    }


    result.statistics.variables_removed =
        result.statistics.original_variables -
        static_cast<int>(
            result.reduced_to_original.size()
        );


    result.statistics.constraints_removed =
        result.statistics.original_constraints -
        static_cast<int>(
            reduced.constraints().size()
        );


    result.statistics.reduced_variables =
        static_cast<int>(
            reduced.variables().size()
        );


    result.statistics.reduced_constraints =
        static_cast<int>(
            reduced.constraints().size()
        );


    result.reduced_problem =
        reduced;


    // --------------------------------------------------------
    // Final message
    // --------------------------------------------------------

    const bool changed =
        result.statistics.variables_removed > 0 ||
        result.statistics.constraints_removed > 0 ||
        result.statistics.bounds_tightened > 0 ||
        result.statistics.rows_scaled > 0;


    if (changed)
    {
        result.message =
            "Presolve and scaling reduced "
            "the optimization model.";
    }
    else
    {
        result.message =
            "Presolve found no removable structure.";
    }


    return result;
}


// ============================================================
// Postsolve variable reconstruction
// ============================================================

std::vector<double>
PresolveResult::postsolve_values(
    const std::vector<double>& reduced_values
) const
{
    const std::size_t original_size =
        fixed_values.size();


    std::vector<double> result(
        original_size,
        0.0
    );


    // --------------------------------------------------------
    // Restore fixed variables
    // --------------------------------------------------------

    for (
        std::size_t i = 0;
        i < original_size;
        ++i
    )
    {
        if (
            i < original_to_reduced.size() &&
            original_to_reduced[i] >= 0
        )
        {
            continue;
        }


        result[i] =
            fixed_values[i];
    }


    // --------------------------------------------------------
    // Restore surviving variables
    // --------------------------------------------------------

    for (
        std::size_t reduced_i = 0;
        reduced_i < reduced_to_original.size();
        ++reduced_i
    )
    {
        const int original_i =
            reduced_to_original[
                reduced_i
            ];


        if (
            reduced_i >=
            reduced_values.size()
        )
        {
            continue;
        }


        if (
            original_i >= 0 &&
            original_i <
                static_cast<int>(
                    result.size()
                )
        )
        {
            result[
                original_i
            ] =
                reduced_values[
                    reduced_i
                ];
        }
    }


    return result;
}


// ============================================================
// Postsolve objective
// ============================================================

double PresolveResult::postsolve_objective(
    double reduced_objective
) const
{
    return
        reduced_objective +
        objective_offset;
}


} // namespace dent