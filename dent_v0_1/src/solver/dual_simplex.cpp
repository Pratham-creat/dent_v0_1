#include "dent/solver/dual_simplex.hpp"

#include "dent/solver/simplex.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace dent {

namespace {

struct Dictionary {
    int rows = 0;
    int columns = 0;

    // x_B = rhs - D * x_N
    std::vector<std::vector<double>> d;

    std::vector<double> rhs;

    // Reduced costs for the maximization-equivalent problem:
    // z = objective + reduced_cost[j] * x_j
    std::vector<double> reduced_cost;

    double objective = 0.0;
};

bool solve_linear_system(
    std::vector<std::vector<double>> A,
    std::vector<double> b,
    std::vector<double>& x,
    double tolerance
)
{
    const int n =
        static_cast<int>(A.size());

    if (
        static_cast<int>(b.size()) != n
    ) {
        return false;
    }

    if (n == 0) {
        x.clear();
        return true;
    }

    for (int k = 0; k < n; ++k) {

        int pivot = k;
        double best =
            std::abs(A[k][k]);

        for (int i = k + 1; i < n; ++i) {

            const double value =
                std::abs(A[i][k]);

            if (value > best) {
                best = value;
                pivot = i;
            }
        }

        if (best <= tolerance) {
            return false;
        }

        if (pivot != k) {
            std::swap(A[pivot], A[k]);
            std::swap(b[pivot], b[k]);
        }

        for (int i = k + 1; i < n; ++i) {

            const double factor =
                A[i][k] / A[k][k];

            if (std::abs(factor) <= tolerance) {
                continue;
            }

            A[i][k] = 0.0;

            for (int j = k + 1; j < n; ++j) {
                A[i][j] -=
                    factor * A[k][j];
            }

            b[i] -=
                factor * b[k];
        }
    }

    x.assign(
        n,
        0.0
    );

    for (int i = n - 1; i >= 0; --i) {

        double value = b[i];

        for (int j = i + 1; j < n; ++j) {
            value -=
                A[i][j] * x[j];
        }

        if (
            std::abs(A[i][i]) <= tolerance
        ) {
            return false;
        }

        x[i] =
            value / A[i][i];
    }

    return true;
}

bool is_basic(
    const std::vector<int>& basis,
    int column
)
{
    return std::find(
        basis.begin(),
        basis.end(),
        column
    ) != basis.end();
}

std::vector<double> column_of(
    const Problem& problem,
    int column
)
{
    const int m =
        static_cast<int>(
            problem.constraints().size()
        );

    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    std::vector<double> result(
        m,
        0.0
    );

    if (column < n) {

        for (int i = 0; i < m; ++i) {
            result[i] =
                problem.matrix()[i][column];
        }

        return result;
    }

    const int slack =
        column - n;

    if (
        slack >= 0 &&
        slack < m
    ) {
        result[slack] = 1.0;
    }

    return result;
}

bool validate_basis(
    const Problem& problem,
    const std::vector<int>& basis
)
{
    const int m =
        static_cast<int>(
            problem.constraints().size()
        );

    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    if (
        static_cast<int>(
            basis.size()
        ) != m
    ) {
        return false;
    }

    std::vector<bool> used(
        n + m,
        false
    );

    for (int variable : basis) {

        if (
            variable < 0 ||
            variable >= n + m
        ) {
            return false;
        }

        if (used[variable]) {
            return false;
        }

        used[variable] = true;
    }

    return true;
}

bool build_basis_matrix(
    const Problem& problem,
    const std::vector<int>& basis,
    std::vector<std::vector<double>>& B
)
{
    const int m =
        static_cast<int>(
            problem.constraints().size()
        );

    if (
        static_cast<int>(
            basis.size()
        ) != m
    ) {
        return false;
    }

    B.assign(
        m,
        std::vector<double>(
            m,
            0.0
        )
    );

    for (int column = 0; column < m; ++column) {

        const std::vector<double> source =
            column_of(
                problem,
                basis[column]
            );

        for (int row = 0; row < m; ++row) {
            B[row][column] =
                source[row];
        }
    }

    return true;
}

bool build_dictionary(
    const Problem& problem,
    const std::vector<int>& basis,
    Dictionary& dictionary,
    double tolerance
)
{
    const int m =
        static_cast<int>(
            problem.constraints().size()
        );

    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    const int total_columns =
        n + m;

    if (!validate_basis(problem, basis)) {
        return false;
    }

    std::vector<std::vector<double>> B;

    if (
        !build_basis_matrix(
            problem,
            basis,
            B
        )
    ) {
        return false;
    }

    /*
        Build B^-1 b.
    */

    std::vector<double> rhs(
        m,
        0.0
    );

    for (int i = 0; i < m; ++i) {
        rhs[i] =
            problem.constraints()[i].rhs;
    }

    std::vector<double> basic_values;

    if (
        !solve_linear_system(
            B,
            rhs,
            basic_values,
            tolerance
        )
    ) {
        return false;
    }

    dictionary.rows = m;
    dictionary.columns = total_columns;
    dictionary.rhs = basic_values;

    /*
        Dictionary convention:

            x_B = B^-1 b - B^-1 A_N x_N

        Therefore:

            D = B^-1 A_N

        and the actual equation is:

            x_B = rhs - D x_N
    */

    dictionary.d.assign(
        m,
        std::vector<double>(
            total_columns,
            0.0
        )
    );

    for (int j = 0; j < total_columns; ++j) {

        if (is_basic(basis, j)) {
            continue;
        }

        const std::vector<double> column =
            column_of(
                problem,
                j
            );

        std::vector<double> transformed;

        if (
            !solve_linear_system(
                B,
                column,
                transformed,
                tolerance
            )
        ) {
            return false;
        }

        for (int i = 0; i < m; ++i) {
            dictionary.d[i][j] =
                transformed[i];
        }
    }

    /*
        Convert the original objective to
        maximization form.

        Max:
            c^T x

        Min:
            -c^T x
    */

    std::vector<double> c(
        total_columns,
        0.0
    );

    const double objective_sign =
        problem.objective_sense()
            == ObjectiveSense::Maximize
            ? 1.0
            : -1.0;

    for (int j = 0; j < n; ++j) {

        c[j] =
            objective_sign *
            problem.objective()[j];
    }

    /*
        c_B.
    */

    std::vector<double> cB(
        m,
        0.0
    );

    for (int i = 0; i < m; ++i) {
        cB[i] =
            c[basis[i]];
    }

    /*
        Solve:

            B^T y = c_B
    */

    std::vector<std::vector<double>> BT(
        m,
        std::vector<double>(
            m,
            0.0
        )
    );

    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < m; ++j) {
            BT[i][j] =
                B[j][i];
        }
    }

    std::vector<double> y;

    if (
        !solve_linear_system(
            BT,
            cB,
            y,
            tolerance
        )
    ) {
        return false;
    }

    /*
        Reduced cost:

            r_j = c_j - y^T A_j

        For a dual-feasible maximization
        dictionary:

            r_j <= 0
    */

    dictionary.reduced_cost.assign(
        total_columns,
        0.0
    );

    for (int j = 0; j < total_columns; ++j) {

        if (is_basic(basis, j)) {
            continue;
        }

        const std::vector<double> column =
            column_of(
                problem,
                j
            );

        double value =
            c[j];

        for (int i = 0; i < m; ++i) {
            value -=
                y[i] * column[i];
        }

        if (
            std::abs(value) <= tolerance
        ) {
            value = 0.0;
        }

        dictionary.reduced_cost[j] =
            value;
    }

    /*
        Objective value of the
        maximization-equivalent problem.
    */

    dictionary.objective = 0.0;

    for (int i = 0; i < m; ++i) {
        dictionary.objective +=
            cB[i] *
            dictionary.rhs[i];
    }

    return true;
}

bool dual_feasible(
    const Dictionary& dictionary,
    const std::vector<int>& basis,
    double tolerance
)
{
    for (
        int j = 0;
        j < dictionary.columns;
        ++j
    ) {

        if (is_basic(basis, j)) {
            continue;
        }

        if (
            dictionary.reduced_cost[j]
            > tolerance
        ) {
            return false;
        }
    }

    return true;
}

bool primal_feasible(
    const Dictionary& dictionary,
    double tolerance
)
{
    for (
        double value :
        dictionary.rhs
    ) {

        if (
            value < -tolerance
        ) {
            return false;
        }
    }

    return true;
}

std::vector<double> recover_solution(
    const Problem& problem,
    const Dictionary& dictionary,
    const std::vector<int>& basis,
    double tolerance
)
{
    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    std::vector<double> values(
        n,
        0.0
    );

    for (
        int row = 0;
        row < static_cast<int>(
            basis.size()
        );
        ++row
    ) {

        const int variable =
            basis[row];

        if (
            variable >= 0 &&
            variable < n
        ) {

            double value =
                dictionary.rhs[row];

            if (
                std::abs(value) <= tolerance
            ) {
                value = 0.0;
            }

            values[variable] =
                value;
        }
    }

    return values;
}

double calculate_objective(
    const Problem& problem,
    const std::vector<double>& values
)
{
    double value = 0.0;

    for (
        std::size_t i = 0;
        i < values.size();
        ++i
    ) {
        value +=
            problem.objective()[i] *
            values[i];
    }

    return value;
}

bool supported_model(
    const Problem& problem,
    double tolerance
)
{
    /*
        Current Dual Simplex implementation:

            Ax <= b
            x >= 0

        Upper bounds other than +infinity
        are not handled directly here.
    */

    for (
        const auto& variable :
        problem.variables()
    ) {

        if (
            std::abs(
                variable.lower_bound
            ) > tolerance
        ) {
            return false;
        }

        if (
            std::isfinite(
                variable.upper_bound
            ) &&
            std::abs(
                variable.upper_bound
            ) > tolerance
        ) {
            return false;
        }

        if (
            variable.type !=
            VariableType::Continuous
        ) {
            return false;
        }
    }

    for (
        const auto& constraint :
        problem.constraints()
    ) {

        if (
            constraint.sense !=
            ConstraintSense::LessEqual
        ) {
            return false;
        }
    }

    return true;
}

bool dual_pivot(
    const Problem& problem,
    std::vector<int>& basis,
    int leaving_row,
    int entering,
    double tolerance
)
{
    const int m =
        static_cast<int>(
            problem.constraints().size()
        );

    if (
        leaving_row < 0 ||
        leaving_row >= m
    ) {
        return false;
    }

    if (
        entering < 0 ||
        entering >=
            static_cast<int>(
                problem.variables().size()
            ) + m
    ) {
        return false;
    }

    if (
        is_basic(
            basis,
            entering
        )
    ) {
        return false;
    }

    /*
        The basis itself is represented by the
        row ordering.

        Leaving row r means the basic variable
        currently occupying row r leaves and
        `entering` becomes the new basic
        variable for that row.
    */

    basis[leaving_row] =
        entering;

    /*
        The caller rebuilds B^-1 A and B^-1 b
        from this new basis. This avoids mixing
        two different dictionary update
        conventions.
    */

    std::vector<std::vector<double>> B;

    if (
        !build_basis_matrix(
            problem,
            basis,
            B
        )
    ) {
        return false;
    }

    std::vector<double> test;

    if (
        !solve_linear_system(
            B,
            std::vector<double>(
                m,
                0.0
            ),
            test,
            tolerance
        )
    ) {
        /*
            A zero RHS still exercises the basis
            matrix. If it is singular, reject it.
        */
        return false;
    }

    return true;
}

} // namespace


DualSimplexSolver::DualSimplexSolver(
    double tolerance,
    int max_iterations
)
    : tolerance_(tolerance),
      max_iterations_(max_iterations)
{
}


void DualSimplexSolver::set_warm_start(
    const WarmStart& warm_start
)
{
    warm_start_ =
        warm_start;
}


void DualSimplexSolver::clear_warm_start()
{
    warm_start_.clear();
}


bool DualSimplexSolver::has_warm_start() const
{
    return warm_start_.available;
}


SolveResult DualSimplexSolver::solve(
    const Problem& problem
) const
{
    SolveResult result;

    try {

        if (
            problem.variables().empty()
        ) {
            result.status =
                SolveStatus::Unsupported;

            result.message =
                "Problem contains no variables.";

            return result;
        }

        /*
            Dual Simplex currently handles:

                maximize/minimize c^T x
                Ax <= b
                x >= 0
                continuous variables
        */

        if (
            !supported_model(
                problem,
                tolerance_
            )
        ) {

            SimplexSolver fallback(
                tolerance_,
                max_iterations_
            );

            result =
                fallback.solve(
                    problem
                );

            result.message =
                "Dual Simplex requires canonical "
                "continuous Ax <= b, x >= 0 form; "
                "Primal Simplex handled this model.";

            return result;
        }

        const int m =
            static_cast<int>(
                problem.constraints().size()
            );

        const int n =
            static_cast<int>(
                problem.variables().size()
            );

        if (m == 0) {

            SimplexSolver fallback(
                tolerance_,
                max_iterations_
            );

            return fallback.solve(
                problem
            );
        }

        /*
            A valid persisted basis is mandatory
            for the actual Dual Simplex path.
        */

        bool valid_warm_start =
            false;

        if (
            warm_start_.available &&
            warm_start_.matches_problem(
                problem
            ) &&
            warm_start_.rows == m &&
            warm_start_.columns == n + m &&
            static_cast<int>(
                warm_start_.basis.size()
            ) == m
        ) {

            valid_warm_start = true;

            if (
                !validate_basis(
                    problem,
                    warm_start_.basis
                )
            ) {
                valid_warm_start = false;
            }
        }

        /*
            Cold Dual Simplex basis construction
            is intentionally delegated to the
            existing Primal Simplex engine.

            The resulting basis can then be
            supplied explicitly by the caller on
            a later reoptimization.
        */

        if (!valid_warm_start) {

            SimplexSolver fallback(
                tolerance_,
                max_iterations_
            );

            result =
                fallback.solve(
                    problem
                );

            result.message =
                "Cold solve completed by Primal "
                "Simplex; a valid persisted basis "
                "is required for Dual Simplex "
                "reoptimization.";

            return result;
        }

        std::vector<int> basis =
            warm_start_.basis;

        Dictionary dictionary;

        if (
            !build_dictionary(
                problem,
                basis,
                dictionary,
                tolerance_
            )
        ) {

            result.status =
                SolveStatus::Unsupported;

            result.message =
                "Dual Simplex could not reconstruct "
                "the stored basis.";

            return result;
        }

        /*
            The persisted basis must remain dual
            feasible. RHS changes are allowed to
            destroy primal feasibility, which is
            exactly what Dual Simplex repairs.
        */

        if (
            !dual_feasible(
                dictionary,
                basis,
                tolerance_
            )
        ) {

            result.status =
                SolveStatus::Unsupported;

            result.message =
                "Stored basis is not dual feasible.";

            return result;
        }

        int iterations = 0;

        while (
            iterations <
            max_iterations_
        ) {

            /*
                If primal feasibility has already
                been restored, dual feasibility +
                primal feasibility implies optimality.
            */

            if (
                primal_feasible(
                    dictionary,
                    tolerance_
                )
            ) {

                result.status =
                    SolveStatus::Optimal;

                result.variable_values =
                    recover_solution(
                        problem,
                        dictionary,
                        basis,
                        tolerance_
                    );

                result.objective_value =
                    calculate_objective(
                        problem,
                        result.variable_values
                    );

                result.iterations =
                    iterations;

                result.warm_start_used =
                    true;

                result.warm_start_iterations =
                    iterations;

                result.message =
                    "Optimal solution found using "
                    "Dual Simplex warm-start "
                    "reoptimization.";

                warm_start_.available =
                    true;

                warm_start_.variable_values =
                    result.variable_values;

                warm_start_.basis =
                    basis;

                warm_start_.rows =
                    m;

                warm_start_.columns =
                    n + m;

                warm_start_.structural_signature =
                    structural_signature_for_problem(
                        problem
                    );

                warm_start_.source =
                    "dual-simplex";

                return result;
            }

            /*
                Select the most negative basic
                variable.

                For:

                    x_B = rhs - D x_N

                a negative rhs requires repair.
            */

            int leaving =
                -1;

            double most_negative =
                -tolerance_;

            for (
                int row = 0;
                row < m;
                ++row
            ) {

                const double value =
                    dictionary.rhs[row];

                if (
                    value <
                    most_negative
                ) {
                    most_negative =
                        value;

                    leaving =
                        row;
                }
            }

            if (
                leaving == -1
            ) {
                continue;
            }

            /*
                Dual Simplex entering rule.

                For the leaving row:

                    x_B =
                        rhs - D x_N

                D[j] < 0 means increasing x_j
                increases the negative basic
                variable.

                To preserve dual feasibility:

                    theta_j =
                        reduced_cost[j] /
                        (-D[leaving][j])

                Select the smallest ratio.
            */

            int entering =
                -1;

            // Dual Simplex ratio rule:
            //
            //     theta_j = reduced_cost[j] / (-D[row][j])
            //
            // Both quantities are non-positive/non-negative
            // in the canonical formulation, so the valid
            // ratios are typically non-positive. We must select
            // the LARGEST ratio (the one closest to zero), not
            // the smallest ratio.
            //
            // Example for the regression case:
            //
            //     x2     -> -1
            //     slack1 -> -3
            //
            // x2 must enter, so -1 is selected over -3.
            double best_ratio =
                -std::numeric_limits<double>::infinity();

            for (
                int j = 0;
                j < n + m;
                ++j
            ) {

                if (
                    is_basic(
                        basis,
                        j
                    )
                ) {
                    continue;
                }

                const double coefficient =
                    dictionary.d[
                        leaving
                    ][j];

                if (
                    coefficient >=
                    -tolerance_
                ) {
                    continue;
                }

                const double reduced_cost =
                    dictionary.reduced_cost[j];

                /*
                    The current dictionary is
                    dual feasible. Numerical noise
                    around zero is tolerated.
                */

                if (
                    reduced_cost >
                    tolerance_
                ) {
                    continue;
                }

                const double ratio =
                    reduced_cost /
                    (-coefficient);

                if (
                    ratio >
                    best_ratio
                ) {
                    best_ratio =
                        ratio;

                    entering =
                        j;
                }
            }

            if (
                entering == -1
            ) {

                /*
                    No entering variable can repair
                    the negative basic variable while
                    preserving dual feasibility.

                    For this canonical LP form this
                    certifies primal infeasibility.
                */

                result.status =
                    SolveStatus::Infeasible;

                result.iterations =
                    iterations;

                result.warm_start_used =
                    true;

                result.warm_start_iterations =
                    iterations;

                result.message =
                    "Dual Simplex detected "
                    "infeasibility.";

                return result;
            }

            /*
                Verify the proposed new basis before
                committing the change.

                Do not mutate the active basis until
                the new basis has been shown nonsingular.
            */

            std::vector<int> candidate_basis =
                basis;

            candidate_basis[leaving] =
                entering;

            Dictionary candidate_dictionary;

            if (
                !build_dictionary(
                    problem,
                    candidate_basis,
                    candidate_dictionary,
                    tolerance_
                )
            ) {

                result.status =
                    SolveStatus::Unsupported;

                result.iterations =
                    iterations;

                result.warm_start_used =
                    true;

                result.warm_start_iterations =
                    iterations;

                result.message =
                    "Dual Simplex rejected a "
                    "singular candidate basis.";

                return result;
            }

            /*
                The pivot must preserve dual
                feasibility.
            */

            if (
                !dual_feasible(
                    candidate_dictionary,
                    candidate_basis,
                    tolerance_
                )
            ) {

                result.status =
                    SolveStatus::Unsupported;

                result.iterations =
                    iterations;

                result.warm_start_used =
                    true;

                result.warm_start_iterations =
                    iterations;

                result.message =
                    "Dual Simplex candidate pivot "
                    "violated dual feasibility.";

                return result;
            }

            /*
                Commit the pivot.
            */

            basis =
                std::move(
                    candidate_basis
                );

            dictionary =
                std::move(
                    candidate_dictionary
                );

            ++iterations;
        }

        result.status =
            SolveStatus::IterationLimit;

        result.iterations =
            max_iterations_;

        result.warm_start_used =
            true;

        result.warm_start_iterations =
            max_iterations_;

        result.message =
            "Dual Simplex iteration limit reached.";

        return result;
    }
    catch (
        const std::exception& error
    ) {

        result.status =
            SolveStatus::Unsupported;

        result.message =
            error.what();

        return result;
    }
}

} // namespace dent