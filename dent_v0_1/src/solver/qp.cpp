#include "dent/solver/qp.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace dent {

namespace {

/*
 * Small dense linear-system solver.
 *
 * Solves:
 *
 *     A x = b
 *
 * using Gaussian elimination with partial pivoting.
 *
 * This is intentionally kept inside the QP module.
 * It is suitable for the small prototype problems
 * we are currently testing.
 */
bool solve_linear_system(
    std::vector<std::vector<double>> A,
    std::vector<double> b,
    std::vector<double>& x,
    double tolerance
)
{
    const int n =
        static_cast<int>(b.size());

    if (static_cast<int>(A.size()) != n) {
        return false;
    }

    for (const auto& row : A) {
        if (static_cast<int>(row.size()) != n) {
            return false;
        }
    }

    for (int column = 0;
         column < n;
         ++column) {

        int pivot = column;

        double largest =
            std::abs(A[column][column]);

        for (int row = column + 1;
             row < n;
             ++row) {

            const double value =
                std::abs(A[row][column]);

            if (value > largest) {
                largest = value;
                pivot = row;
            }
        }

        if (largest <= tolerance) {
            return false;
        }

        if (pivot != column) {
            std::swap(
                A[pivot],
                A[column]
            );

            std::swap(
                b[pivot],
                b[column]
            );
        }

        /*
         * Eliminate below the pivot.
         */
        for (int row = column + 1;
             row < n;
             ++row) {

            const double factor =
                A[row][column] /
                A[column][column];

            if (std::abs(factor) <= tolerance) {
                continue;
            }

            for (int j = column;
                 j < n;
                 ++j) {

                A[row][j] -=
                    factor *
                    A[column][j];
            }

            b[row] -=
                factor *
                b[column];
        }
    }

    /*
     * Back substitution.
     */
    x.assign(
        n,
        0.0
    );

    for (int row = n - 1;
         row >= 0;
         --row) {

        double value =
            b[row];

        for (int j = row + 1;
             j < n;
             ++j) {

            value -=
                A[row][j] *
                x[j];
        }

        if (std::abs(A[row][row]) <= tolerance) {
            return false;
        }

        x[row] =
            value /
            A[row][row];
    }

    return true;
}

struct LinearConstraint {
    std::vector<double> coefficients;

    double rhs = 0.0;

    ConstraintSense sense =
        ConstraintSense::LessEqual;

    bool equality = false;

    int original_index = -1;

    std::string name;
};

bool check_constraint(
    const LinearConstraint& constraint,
    const std::vector<double>& x,
    double tolerance
)
{
    double lhs = 0.0;

    for (std::size_t j = 0;
         j < x.size();
         ++j) {

        lhs +=
            constraint.coefficients[j] *
            x[j];
    }

    if (constraint.sense ==
        ConstraintSense::LessEqual) {

        return lhs <=
               constraint.rhs +
               tolerance;
    }

    if (constraint.sense ==
        ConstraintSense::GreaterEqual) {

        return lhs >=
               constraint.rhs -
               tolerance;
    }

    return std::abs(
        lhs - constraint.rhs
    ) <= tolerance;
}

double constraint_residual(
    const LinearConstraint& constraint,
    const std::vector<double>& x
)
{
    double lhs = 0.0;

    for (std::size_t j = 0;
         j < x.size();
         ++j) {

        lhs +=
            constraint.coefficients[j] *
            x[j];
    }

    if (constraint.sense ==
        ConstraintSense::LessEqual) {

        return std::max(
            0.0,
            lhs - constraint.rhs
        );
    }

    if (constraint.sense ==
        ConstraintSense::GreaterEqual) {

        return std::max(
            0.0,
            constraint.rhs - lhs
        );
    }

    return std::abs(
        lhs - constraint.rhs
    );
}

double calculate_constraint_violation(
    const std::vector<LinearConstraint>& constraints,
    const std::vector<double>& x,
    double tolerance
)
{
    double maximum = 0.0;

    for (const auto& constraint :
         constraints) {

        maximum =
            std::max(
                maximum,
                constraint_residual(
                    constraint,
                    x
                )
            );
    }

    (void)tolerance;

    return maximum;
}

}

/* ============================================================
 * Constructor
 * ============================================================ */

QPSolver::QPSolver(
    double tolerance,
    int max_iterations
)
    : tolerance_(tolerance),
      max_iterations_(max_iterations)
{
}

/* ============================================================
 * Objective
 * ============================================================ */

double QPSolver::objective_value(
    const Problem& problem,
    const std::vector<double>& x
) const
{
    const auto& Q =
        problem.quadratic_matrix();

    const auto& c =
        problem.objective();

    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    double value = 0.0;

    /*
     * 1/2 x^T Q x
     */
    for (int i = 0;
         i < n;
         ++i) {

        for (int j = 0;
             j < n;
             ++j) {

            value +=
                0.5 *
                x[i] *
                Q[i][j] *
                x[j];
        }
    }

    /*
     * c^T x
     */
    for (int i = 0;
         i < n;
         ++i) {

        value +=
            c[i] *
            x[i];
    }

    /*
     * Internally transform maximization
     * into minimization.
     */
    if (problem.objective_sense() ==
        ObjectiveSense::Maximize) {

        value = -value;
    }

    return value;
}

/* ============================================================
 * Gradient
 * ============================================================ */

std::vector<double> QPSolver::gradient(
    const Problem& problem,
    const std::vector<double>& x
) const
{
    const auto& Q =
        problem.quadratic_matrix();

    const auto& c =
        problem.objective();

    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    std::vector<double> g(
        n,
        0.0
    );

    /*
     * gradient =
     *
     *     Qx + c
     *
     * using the symmetric part of Q.
     */
    for (int i = 0;
         i < n;
         ++i) {

        for (int j = 0;
             j < n;
             ++j) {

            const double q =
                0.5 *
                (
                    Q[i][j] +
                    Q[j][i]
                );

            g[i] +=
                q *
                x[j];
        }

        g[i] +=
            c[i];

        if (problem.objective_sense() ==
            ObjectiveSense::Maximize) {

            g[i] = -g[i];
        }
    }

    return g;
}

/* ============================================================
 * Vector norm
 * ============================================================ */

double QPSolver::vector_norm(
    const std::vector<double>& values
) const
{
    double sum = 0.0;

    for (double value :
         values) {

        sum +=
            value *
            value;
    }

    return std::sqrt(sum);
}

/* ============================================================
 * Constraint violation
 * ============================================================ */

double QPSolver::constraint_violation(
    const Problem& problem,
    const std::vector<double>& x
) const
{
    double maximum_violation = 0.0;

    const auto& constraints =
        problem.constraints();

    const auto& A =
        problem.matrix();

    /*
     * Linear constraints.
     */
    for (std::size_t i = 0;
         i < constraints.size();
         ++i) {

        double lhs = 0.0;

        for (std::size_t j = 0;
             j < x.size();
             ++j) {

            lhs +=
                A[i][j] *
                x[j];
        }

        double violation = 0.0;

        switch (
            constraints[i].sense
        ) {

            case ConstraintSense::LessEqual:

                violation =
                    std::max(
                        0.0,
                        lhs -
                        constraints[i].rhs
                    );

                break;

            case ConstraintSense::GreaterEqual:

                violation =
                    std::max(
                        0.0,
                        constraints[i].rhs -
                        lhs
                    );

                break;

            case ConstraintSense::Equal:

                violation =
                    std::abs(
                        lhs -
                        constraints[i].rhs
                    );

                break;
        }

        maximum_violation =
            std::max(
                maximum_violation,
                violation
            );
    }

    /*
     * Variable bounds.
     */
    for (std::size_t i = 0;
         i < problem.variables().size();
         ++i) {

        const double lower =
            problem.variables()[i]
                .lower_bound;

        if (x[i] < lower) {

            maximum_violation =
                std::max(
                    maximum_violation,
                    lower -
                    x[i]
                );
        }

        const double upper =
            problem.variables()[i]
                .upper_bound;

        if (upper != 0.0 &&
            x[i] > upper) {

            maximum_violation =
                std::max(
                    maximum_violation,
                    x[i] -
                    upper
                );
        }
    }

    return maximum_violation;
}

/* ============================================================
 * Feasibility
 * ============================================================ */

bool QPSolver::satisfies_constraints(
    const Problem& problem,
    const std::vector<double>& x
) const
{
    return constraint_violation(
        problem,
        x
    ) <= tolerance_;
}

/* ============================================================
 * Projection
 *
 * Kept for API compatibility with qp.hpp.
 *
 * The active-set solver does NOT rely on this
 * function for optimality.
 * ============================================================ */

std::vector<double>
QPSolver::project_to_constraints(
    const Problem& problem,
    const std::vector<double>& x
) const
{
    std::vector<double> result =
        x;

    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    const auto& constraints =
        problem.constraints();

    const auto& A =
        problem.matrix();

    /*
     * Enforce bounds.
     */
    for (int j = 0;
         j < n;
         ++j) {

        const double lower =
            problem.variables()[j]
                .lower_bound;

        if (result[j] < lower) {
            result[j] = lower;
        }

        const double upper =
            problem.variables()[j]
                .upper_bound;

        if (upper != 0.0 &&
            result[j] > upper) {

            result[j] = upper;
        }
    }

    /*
     * Simple alternating projection.
     */
    for (int pass = 0;
         pass < 200;
         ++pass) {

        bool changed = false;

        for (std::size_t i = 0;
             i < constraints.size();
             ++i) {

            double lhs = 0.0;
            double norm_squared = 0.0;

            for (int j = 0;
                 j < n;
                 ++j) {

                lhs +=
                    A[i][j] *
                    result[j];

                norm_squared +=
                    A[i][j] *
                    A[i][j];
            }

            if (norm_squared <=
                tolerance_) {

                continue;
            }

            double difference = 0.0;
            bool violated = false;

            switch (
                constraints[i].sense
            ) {

                case ConstraintSense::LessEqual:

                    if (lhs >
                        constraints[i].rhs +
                        tolerance_) {

                        difference =
                            lhs -
                            constraints[i].rhs;

                        violated = true;
                    }

                    break;

                case ConstraintSense::GreaterEqual:

                    if (lhs <
                        constraints[i].rhs -
                        tolerance_) {

                        difference =
                            lhs -
                            constraints[i].rhs;

                        violated = true;
                    }

                    break;

                case ConstraintSense::Equal:

                    if (std::abs(
                            lhs -
                            constraints[i].rhs
                        ) > tolerance_) {

                        difference =
                            lhs -
                            constraints[i].rhs;

                        violated = true;
                    }

                    break;
            }

            if (!violated) {
                continue;
            }

            for (int j = 0;
                 j < n;
                 ++j) {

                result[j] -=
                    (
                        difference /
                        norm_squared
                    ) *
                    A[i][j];
            }

            changed = true;
        }

        /*
         * Re-enforce bounds.
         */
        for (int j = 0;
             j < n;
             ++j) {

            const double lower =
                problem.variables()[j]
                    .lower_bound;

            if (result[j] < lower) {
                result[j] = lower;
            }

            const double upper =
                problem.variables()[j]
                    .upper_bound;

            if (upper != 0.0 &&
                result[j] > upper) {

                result[j] = upper;
            }
        }

        if (!changed) {
            break;
        }
    }

    return result;
}

/* ============================================================
 * Main QP solver
 *
 * Active-set / KKT enumeration
 * ============================================================ */

QPSolution QPSolver::solve(
    const Problem& problem
) const
{
    QPSolution result;

    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    if (n == 0) {

        result.status =
            SolveStatus::Infeasible;

        result.message =
            "QP contains no decision variables.";

        return result;
    }

    /*
     * We currently support continuous QP only.
     */
    for (const auto& variable :
         problem.variables()) {

        if (variable.type !=
            VariableType::Continuous) {

            result.status =
                SolveStatus::Unsupported;

            result.message =
                "QPSolver currently supports "
                "continuous variables only.";

            return result;
        }
    }

    const auto& Q =
        problem.quadratic_matrix();

    if (static_cast<int>(Q.size()) != n) {

        result.status =
            SolveStatus::Unsupported;

        result.message =
            "Quadratic matrix dimension does "
            "not match the number of variables.";

        return result;
    }

    for (const auto& row : Q) {

        if (static_cast<int>(row.size()) != n) {

            result.status =
                SolveStatus::Unsupported;

            result.message =
                "Quadratic matrix must be square.";

            return result;
        }
    }

    /*
     * We solve the internally transformed minimization
     * problem:
     *
     *     min 1/2 x^T H x + f^T x
     *
     * For minimization:
     *
     *     H = Q
     *     f = c
     *
     * For maximization:
     *
     *     H = -Q
     *     f = -c
     */
    std::vector<std::vector<double>> H(
        n,
        std::vector<double>(
            n,
            0.0
        )
    );

    std::vector<double> f(
        n,
        0.0
    );

    const bool maximize =
        problem.objective_sense() ==
        ObjectiveSense::Maximize;

    for (int i = 0;
         i < n;
         ++i) {

        f[i] =
            maximize
                ? -problem.objective()[i]
                : problem.objective()[i];

        for (int j = 0;
             j < n;
             ++j) {

            const double symmetric_q =
                0.5 *
                (
                    Q[i][j] +
                    Q[j][i]
                );

            H[i][j] =
                maximize
                    ? -symmetric_q
                    : symmetric_q;
        }
    }

    /*
     * Build all constraints in the form:
     *
     *     a^T x <= b
     *
     * Equalities remain marked as equalities.
     *
     * Greater-than constraints are multiplied by -1.
     */
    std::vector<LinearConstraint>
        constraints;

    const auto& original_constraints =
        problem.constraints();

    const auto& A =
        problem.matrix();

    for (std::size_t i = 0;
         i < original_constraints.size();
         ++i) {

        LinearConstraint constraint;

        constraint.coefficients =
            A[i];

        constraint.rhs =
            original_constraints[i].rhs;

        constraint.sense =
            original_constraints[i].sense;

        constraint.original_index =
            static_cast<int>(i);

        constraint.name =
            original_constraints[i].name;

        if (constraint.sense ==
            ConstraintSense::GreaterEqual) {

            /*
             * a^T x >= b
             *
             * becomes
             *
             * -a^T x <= -b
             */
            for (double& coefficient :
                 constraint.coefficients) {

                coefficient =
                    -coefficient;
            }

            constraint.rhs =
                -constraint.rhs;

            constraint.sense =
                ConstraintSense::LessEqual;
        }

        constraint.equality =
            original_constraints[i].sense ==
            ConstraintSense::Equal;

        constraints.push_back(
            constraint
        );
    }

    /*
     * Convert finite variable bounds into
     * explicit constraints.
     *
     * lower <= x
     *
     * becomes
     *
     * -x <= -lower
     *
     * upper:
     *
     * x <= upper
     */
    for (int j = 0;
         j < n;
         ++j) {

        const double lower =
            problem.variables()[j]
                .lower_bound;

        if (std::isfinite(lower)) {

            LinearConstraint constraint;

            constraint.coefficients.assign(
                n,
                0.0
            );

            constraint.coefficients[j] =
                -1.0;

            constraint.rhs =
                -lower;

            constraint.sense =
                ConstraintSense::LessEqual;

            constraint.equality =
                false;

            constraint.name =
                "lower_bound_" +
                std::to_string(j);

            constraints.push_back(
                constraint
            );
        }

        const double upper =
            problem.variables()[j]
                .upper_bound;

        /*
         * In DENT, upper_bound == 0 means
         * positive infinity.
         *
         * But a real upper bound of zero is
         * therefore not representable in the
         * current Problem model.
         */
        if (upper != 0.0 &&
            std::isfinite(upper)) {

            LinearConstraint constraint;

            constraint.coefficients.assign(
                n,
                0.0
            );

            constraint.coefficients[j] =
                1.0;

            constraint.rhs =
                upper;

            constraint.sense =
                ConstraintSense::LessEqual;

            constraint.equality =
                false;

            constraint.name =
                "upper_bound_" +
                std::to_string(j);

            constraints.push_back(
                constraint
            );
        }
    }

    /*
     * Separate equalities and inequalities.
     *
     * Equalities are always active.
     */
    std::vector<int> equality_indices;
    std::vector<int> inequality_indices;

    for (int i = 0;
         i < static_cast<int>(
                 constraints.size());
         ++i) {

        if (constraints[i].equality) {
            equality_indices.push_back(i);
        } else {
            inequality_indices.push_back(i);
        }
    }

    /*
     * Active-set enumeration grows exponentially.
     *
     * This is deliberately a small-problem
     * prototype implementation.
     */
    if (inequality_indices.size() > 20) {

        result.status =
            SolveStatus::Unsupported;

        result.message =
            "Too many inequality constraints for "
            "the current active-set prototype.";

        return result;
    }

    const std::uint64_t
        total_active_sets =
            std::uint64_t(1)
            << inequality_indices.size();

    bool found_solution = false;

    double best_value =
        std::numeric_limits<double>::infinity();

    std::vector<double> best_x;

    double best_violation =
        std::numeric_limits<double>::infinity();

    int best_active_count = 0;

    int systems_attempted = 0;

    /*
     * Enumerate possible active sets.
     *
     * Every candidate is obtained from:
     *
     *     [ H  A^T ] [x]   [-f]
     *     [ A   0  ] [λ] = [ b]
     *
     * which represents:
     *
     *     Hx + f + A^T λ = 0
     *     Ax = b
     *
     * for active constraints.
     */
    for (std::uint64_t mask = 0;
         mask < total_active_sets;
         ++mask) {

        std::vector<int> active_indices =
            equality_indices;

        for (std::size_t k = 0;
             k < inequality_indices.size();
             ++k) {

            if ((mask &
                 (std::uint64_t(1) << k))
                != 0) {

                active_indices.push_back(
                    inequality_indices[k]
                );
            }
        }

        const int active_count =
            static_cast<int>(
                active_indices.size()
            );

        /*
         * KKT system dimension.
         */
        const int system_size =
            n +
            active_count;

        std::vector<std::vector<double>>
            KKT(
                system_size,
                std::vector<double>(
                    system_size,
                    0.0
                )
            );

        std::vector<double> rhs(
            system_size,
            0.0
        );

        /*
         * Top-left: H
         */
        for (int i = 0;
             i < n;
             ++i) {

            for (int j = 0;
                 j < n;
                 ++j) {

                KKT[i][j] =
                    H[i][j];
            }

            rhs[i] =
                -f[i];
        }

        /*
         * Constraint rows / columns.
         */
        for (int k = 0;
             k < active_count;
             ++k) {

            const int constraint_index =
                active_indices[k];

            const auto& constraint =
                constraints[
                    constraint_index
                ];

            const int row =
                n + k;

            for (int j = 0;
                 j < n;
                 ++j) {

                const double a =
                    constraint.coefficients[j];

                KKT[j][row] =
                    a;

                KKT[row][j] =
                    a;
            }

            rhs[row] =
                constraint.rhs;
        }

        ++systems_attempted;

        std::vector<double> solution;

        if (!solve_linear_system(
                KKT,
                rhs,
                solution,
                tolerance_ * 0.1
            )) {

            continue;
        }

        /*
         * Extract x.
         */
        std::vector<double> x(
            n,
            0.0
        );

        for (int i = 0;
             i < n;
             ++i) {

            x[i] =
                solution[i];
        }

        /*
         * Check primal feasibility.
         */
        bool feasible = true;

        double violation = 0.0;

        for (const auto& constraint :
             constraints) {

            const double residual =
                constraint_residual(
                    constraint,
                    x
                );

            violation =
                std::max(
                    violation,
                    residual
                );

            if (!check_constraint(
                    constraint,
                    x,
                    tolerance_ * 10.0
                )) {

                feasible = false;
                break;
            }
        }

        if (!feasible) {
            continue;
        }

        /*
         * Check inactive inequalities.
         *
         * If an inequality was NOT selected as active,
         * its multiplier is zero.
         */
        for (std::size_t k = 0;
             k < inequality_indices.size();
             ++k) {

            const int constraint_index =
                inequality_indices[k];

            const bool active =
                (
                    mask &
                    (std::uint64_t(1) << k)
                ) != 0;

            if (active) {
                continue;
            }

            const double residual =
                constraint_residual(
                    constraints[
                        constraint_index
                    ],
                    x
                );

            if (residual >
                tolerance_ * 10.0) {

                feasible = false;
                break;
            }
        }

        if (!feasible) {
            continue;
        }

        /*
         * KKT multiplier check.
         *
         * For constraints in the form:
         *
         *     Ax <= b
         *
         * the multiplier must satisfy:
         *
         *     λ >= 0
         *
         * Equality multipliers are unrestricted.
         */
        bool dual_feasible = true;

        for (int k = 0;
             k < active_count;
             ++k) {

            const int constraint_index =
                active_indices[k];

            if (constraints[
                    constraint_index
                ].equality) {

                continue;
            }

            const double lambda =
                solution[n + k];

            if (lambda <
                -tolerance_ * 10.0) {

                dual_feasible = false;
                break;
            }
        }

        if (!dual_feasible) {
            continue;
        }

        /*
         * Evaluate objective.
         */
        const double value =
            objective_value(
                problem,
                x
            );

        /*
         * Keep the best feasible KKT point.
         */
        if (!found_solution ||
            value <
            best_value -
            tolerance_ * 10.0) {

            found_solution = true;

            best_value =
                value;

            best_x =
                x;

            best_violation =
                violation;

            best_active_count =
                active_count;
        }
    }

    /*
     * No KKT point was found.
     */
    if (!found_solution) {

        result.status =
            SolveStatus::Infeasible;

        result.message =
            "No feasible KKT solution was found "
            "by the active-set solver.";

        result.iterations =
            systems_attempted;

        return result;
    }

    /*
     * Final verification.
     */
    const double final_violation =
        constraint_violation(
            problem,
            best_x
        );

    const std::vector<double>
        final_gradient =
            gradient(
                problem,
                best_x
            );

    result.variable_values =
        best_x;

    result.objective_value =
        best_value;

    result.constraint_violation =
        final_violation;

    result.gradient_norm =
        vector_norm(
            final_gradient
        );

    result.iterations =
        systems_attempted;

    /*
     * KKT active-set solver has verified
     * primal feasibility and multiplier
     * conditions.
     */
    if (final_violation <=
        tolerance_ * 10.0) {

        result.status =
            SolveStatus::Optimal;

        result.message =
            "QP solved using active-set "
            "KKT conditions.";

        (void)best_active_count;

        return result;
    }

    result.status =
        SolveStatus::IterationLimit;

    result.message =
        "A candidate QP solution was found, "
        "but final feasibility verification "
        "failed.";

    return result;
}

} // namespace dent