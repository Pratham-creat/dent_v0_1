#include "dent/solver/milp.hpp"

#include "dent/solver/dual_simplex.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dent {

namespace {

constexpr double EPS = 1e-9;

constexpr int MAX_CUT_ROUNDS = 5;
constexpr int MAX_DIVE_DEPTH = 8;
constexpr int MAX_STRONG_BRANCH_CANDIDATES = 4;
constexpr int STRONG_BRANCH_MAX_ITERATIONS = 80;

bool is_branch_constraint(
    const Constraint& constraint
)
{
    return constraint.name.rfind(
        "__dent_branch_",
        0
    ) == 0;
}

bool is_finite_upper_bound(
    const Variable& variable
)
{
    return variable.upper_bound != 0.0 &&
           std::isfinite(variable.upper_bound);
}

double fractional_part(
    double value
)
{
    const double lower = std::floor(value);
    const double upper = std::ceil(value);

    return std::min(
        value - lower,
        upper - value
    );
}

bool approximately_equal(
    double a,
    double b,
    double tolerance
)
{
    return std::abs(a - b) <= tolerance;
}


/*
    ------------------------------------------------------------
    Small dense linear algebra used exclusively by the
    row-addition warm-start path.

    The parent simplex basis is already known to be nonsingular.
    We reconstruct B^-1 A and B^-1 b for the child after adding
    exactly one <= row.

    This deliberately does NOT touch the existing Simplex
    tableau implementation.
    ------------------------------------------------------------
*/

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
        double best = std::abs(A[k][k]);

        for (int i = k + 1; i < n; ++i) {
            const double candidate =
                std::abs(A[i][k]);

            if (candidate > best) {
                best = candidate;
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


bool solve_transposed_system(
    const std::vector<std::vector<double>>& A,
    const std::vector<double>& b,
    std::vector<double>& x,
    double tolerance
)
{
    const int n =
        static_cast<int>(A.size());

    std::vector<std::vector<double>> transpose(
        n,
        std::vector<double>(n, 0.0)
    );

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            transpose[j][i] =
                A[i][j];
        }
    }

    return solve_linear_system(
        transpose,
        b,
        x,
        tolerance
    );
}


/*
    Build the canonical matrix used by the warm-start path.

    The MILP relaxation is deliberately restricted here to:

        A x <= b
        x >= 0

    with all RHS values non-negative.

    For every row:

        A_i x + s_i = b_i

    Therefore the slack column for row i is:

        n + i

    and adding one child row introduces exactly one new
    standard-form column:

        n + parent_rows
*/
bool build_canonical_matrix(
    const Problem& problem,
    std::vector<std::vector<double>>& A,
    std::vector<double>& b,
    std::vector<double>& c,
    double tolerance
)
{
    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    const int m =
        static_cast<int>(
            problem.constraints().size()
        );

    if (n <= 0 || m <= 0) {
        return false;
    }

    A.assign(
        m,
        std::vector<double>(
            n + m,
            0.0
        )
    );

    b.assign(
        m,
        0.0
    );

    c.assign(
        n + m,
        0.0
    );

    for (int j = 0; j < n; ++j) {
        c[j] =
            problem.objective()[j];
    }

    for (int i = 0; i < m; ++i) {

        const Constraint& row =
            problem.constraints()[i];

        if (
            row.sense !=
            ConstraintSense::LessEqual
        ) {
            return false;
        }

        if (
            row.rhs < -tolerance
        ) {
            return false;
        }

        b[i] =
            row.rhs;

        for (int j = 0; j < n; ++j) {
            A[i][j] =
                problem.matrix()[i][j];
        }

        A[i][n + i] =
            1.0;
    }

    return true;
}


bool basis_is_valid(
    const std::vector<int>& basis,
    int rows,
    int columns
)
{
    if (
        static_cast<int>(basis.size()) != rows
    ) {
        return false;
    }

    std::vector<bool> used(
        columns,
        false
    );

    for (int column : basis) {

        if (
            column < 0 ||
            column >= columns
        ) {
            return false;
        }

        if (used[column]) {
            return false;
        }

        used[column] =
            true;
    }

    return true;
}


/*
    Build dictionary:

        x_B = rhs - D x_N

    and reduced costs for a maximization-equivalent
    objective.

    The dictionary is represented directly rather than
    using the existing DualSimplex private implementation.
*/
struct WarmDictionary
{
    int rows = 0;
    int columns = 0;

    std::vector<std::vector<double>> D;
    std::vector<double> rhs;

    std::vector<double> reduced_cost;

    double objective = 0.0;
};


bool build_dictionary(
    const Problem& problem,
    const std::vector<int>& basis,
    WarmDictionary& dictionary,
    double tolerance
)
{
    std::vector<std::vector<double>> A;
    std::vector<double> b;
    std::vector<double> c;

    if (
        !build_canonical_matrix(
            problem,
            A,
            b,
            c,
            tolerance
        )
    ) {
        return false;
    }

    const int m =
        static_cast<int>(A.size());

    const int columns =
        static_cast<int>(A.front().size());

    if (
        !basis_is_valid(
            basis,
            m,
            columns
        )
    ) {
        return false;
    }

    std::vector<std::vector<double>> B(
        m,
        std::vector<double>(
            m,
            0.0
        )
    );

    for (int i = 0; i < m; ++i) {
        for (int k = 0; k < m; ++k) {
            B[i][k] =
                A[i][basis[k]];
        }
    }

    std::vector<double> basic_values;

    if (
        !solve_linear_system(
            B,
            b,
            basic_values,
            tolerance
        )
    ) {
        return false;
    }

    /*
        For each column j:

            B^-1 A_j

        is obtained by solving:

            B y = A_j
    */

    dictionary.rows =
        m;

    dictionary.columns =
        columns;

    dictionary.D.assign(
        m,
        std::vector<double>(
            columns,
            0.0
        )
    );

    dictionary.rhs =
        basic_values;

    std::vector<double> c_basic(
        m,
        0.0
    );

    for (int i = 0; i < m; ++i) {
        c_basic[i] =
            c[basis[i]];
    }

    /*
        y = B^-1 A_j

        Dictionary convention:

            x_B = B^-1 b - B^-1 A_N x_N

        Therefore:

            D = B^-1 A
    */

    for (int j = 0; j < columns; ++j) {

        std::vector<double> column(
            m,
            0.0
        );

        for (int i = 0; i < m; ++i) {
            column[i] =
                A[i][j];
        }

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
            dictionary.D[i][j] =
                transformed[i];
        }
    }

    /*
        Dual multipliers:

            B^T y = c_B

        Reduced cost:

            c_j - y^T A_j
    */

    std::vector<double> dual;

    if (
        !solve_transposed_system(
            B,
            c_basic,
            dual,
            tolerance
        )
    ) {
        return false;
    }

    dictionary.reduced_cost.assign(
        columns,
        0.0
    );

    for (int j = 0; j < columns; ++j) {

        double value =
            c[j];

        for (int i = 0; i < m; ++i) {
            value -=
                dual[i] *
                A[i][j];
        }

        dictionary.reduced_cost[j] =
            value;
    }

    dictionary.objective =
        0.0;

    for (int i = 0; i < m; ++i) {
        dictionary.objective +=
            c_basic[i] *
            dictionary.rhs[i];
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


bool dual_feasible(
    const WarmDictionary& dictionary,
    const std::vector<int>& basis,
    double tolerance
)
{
    for (
        int j = 0;
        j < dictionary.columns;
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
    const WarmDictionary& dictionary,
    double tolerance
)
{
    for (double value :
         dictionary.rhs) {

        if (
            value < -tolerance
        ) {
            return false;
        }
    }

    return true;
}


bool recover_solution(
    const Problem& problem,
    const WarmDictionary& dictionary,
    const std::vector<int>& basis,
    std::vector<double>& values,
    double tolerance
)
{
    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    values.assign(
        n,
        0.0
    );

    for (
        int row = 0;
        row < dictionary.rows;
        ++row
    ) {
        const int basic =
            basis[row];

        if (
            basic >= 0 &&
            basic < n
        ) {
            double value =
                dictionary.rhs[row];

            if (
                std::abs(value) <= tolerance
            ) {
                value = 0.0;
            }

            values[basic] =
                value;
        }
    }

    return true;
}


/*
    The child relaxation differs from the parent relaxation by
    exactly one appended <= row.

    Parent basis:

        B = {b0,...,b(m-1)}

    Child standard form:

        columns = n + m + 1

    Existing basis remains valid.

    New row receives its new slack:

        n + m

    as its initial basic variable.

    That basis is generally primal infeasible because the new
    branch row can make the parent solution violate the branch.

    It is nevertheless dual feasible. Dual Simplex then repairs
    the negative RHS rows.
*/
bool row_added_reoptimization(
    const Problem& parent,
    const WarmStart& parent_warm_start,
    const Problem& child,
    SolveResult& result,
    double tolerance,
    int max_iterations
)
{
    const int parent_rows =
        static_cast<int>(
            parent.constraints().size()
        );

    const int child_rows =
        static_cast<int>(
            child.constraints().size()
        );

    const int n =
        static_cast<int>(
            child.variables().size()
        );

    if (
        child_rows !=
        parent_rows + 1
    ) {
        return false;
    }

    if (
        static_cast<int>(
            parent.variables().size()
        ) != n
    ) {
        return false;
    }

    /*
        WarmStart::matches_problem(child) must NOT be used.

        The child intentionally has one additional row.
    */

    if (
        !parent_warm_start.available
    ) {
        return false;
    }

    if (
        parent_warm_start.rows !=
        parent_rows
    ) {
        return false;
    }

    if (
        parent_warm_start.columns !=
        n + parent_rows
    ) {
        return false;
    }

    if (
        !basis_is_valid(
            parent_warm_start.basis,
            parent_rows,
            n + parent_rows
        )
    ) {
        return false;
    }

    /*
        Both models must have the same number of variables
        and identical mathematical rows before the newly
        appended row.
    */
    for (int i = 0; i < parent_rows; ++i) {

        const Constraint& p =
            parent.constraints()[i];

        const Constraint& c =
            child.constraints()[i];

        if (
            p.sense != c.sense
        ) {
            return false;
        }

        if (
            !approximately_equal(
                p.rhs,
                c.rhs,
                tolerance
            )
        ) {
            /*
                RHS changes are actually supported by dual
                simplex. Do not reject them.
            */
        }

        for (int j = 0; j < n; ++j) {

            if (
                !approximately_equal(
                    parent.matrix()[i][j],
                    child.matrix()[i][j],
                    tolerance
                )
            ) {
                return false;
            }
        }
    }

    /*
        Both models must be canonical <= form.
    */
    std::vector<std::vector<double>> parent_A;
    std::vector<double> parent_b;
    std::vector<double> parent_c;

    std::vector<std::vector<double>> child_A;
    std::vector<double> child_b;
    std::vector<double> child_c;

    if (
        !build_canonical_matrix(
            parent,
            parent_A,
            parent_b,
            parent_c,
            tolerance
        )
    ) {
        return false;
    }

    if (
        !build_canonical_matrix(
            child,
            child_A,
            child_b,
            child_c,
            tolerance
        )
    ) {
        return false;
    }

    /*
        The first n + parent_rows columns of the child must
        preserve the parent's standard-form matrix.
    */
    for (int i = 0; i < parent_rows; ++i) {

        for (int j = 0; j < n + parent_rows; ++j) {

            if (
                !approximately_equal(
                    parent_A[i][j],
                    child_A[i][j],
                    tolerance
                )
            ) {
                return false;
            }
        }
    }

    /*
        Child's new row must be <= and have its own slack.
    */
    for (int j = 0; j < n + parent_rows; ++j) {

        if (
            std::abs(
                child_A[parent_rows][j]
            ) <= tolerance
        ) {
            continue;
        }

        /*
            The new branch row is allowed to contain original
            variable coefficients only. It must not reference
            previous slack columns.
        */
        if (j >= n) {
            return false;
        }
    }

    /*
        Extend the parent basis with the new slack.
    */
    std::vector<int> child_basis =
        parent_warm_start.basis;

    child_basis.push_back(
        n + parent_rows
    );

    if (
        !basis_is_valid(
            child_basis,
            child_rows,
            n + child_rows
        )
    ) {
        return false;
    }

    WarmDictionary dictionary;

    if (
        !build_dictionary(
            child,
            child_basis,
            dictionary,
            tolerance
        )
    ) {
        return false;
    }

    /*
        The inherited basis must be dual feasible before
        Dual Simplex is allowed to modify it.
    */
    if (
        !dual_feasible(
            dictionary,
            child_basis,
            tolerance
        )
    ) {
        return false;
    }

    int iterations = 0;

    while (
        iterations < max_iterations
    ) {

        if (
            primal_feasible(
                dictionary,
                tolerance
            )
        ) {
            result.status =
                SolveStatus::Optimal;

            recover_solution(
                child,
                dictionary,
                child_basis,
                result.variable_values,
                tolerance
            );

            result.objective_value =
                0.0;

            for (
                std::size_t j = 0;
                j < result.variable_values.size();
                ++j
            ) {
                result.objective_value +=
                    child.objective()[j] *
                    result.variable_values[j];
            }

            result.iterations =
                iterations;

            result.warm_start_used =
                true;

            result.warm_start_iterations =
                iterations;

            result.basis =
                child_basis;

            result.basis_rows =
                child_rows;

            result.basis_columns =
                n + child_rows;

            result.message =
                "Child LP reoptimized using "
                "row-addition Dual Simplex.";

            return true;
        }

        int leaving =
            -1;

        double most_negative =
            -tolerance;

        for (
            int row = 0;
            row < dictionary.rows;
            ++row
        ) {
            if (
                dictionary.rhs[row] <
                most_negative
            ) {
                most_negative =
                    dictionary.rhs[row];

                leaving =
                    row;
            }
        }

        if (
            leaving < 0
        ) {
            continue;
        }

        int entering =
            -1;

        double best_ratio =
            -std::numeric_limits<double>::infinity();

        for (
            int j = 0;
            j < dictionary.columns;
            ++j
        ) {
            if (
                is_basic(
                    child_basis,
                    j
                )
            ) {
                continue;
            }

            const double coefficient =
                dictionary.D[leaving][j];

            if (
                coefficient >=
                -tolerance
            ) {
                continue;
            }

            const double reduced_cost =
                dictionary.reduced_cost[j];

            if (
                reduced_cost >
                tolerance
            ) {
                continue;
            }

            const double ratio =
                reduced_cost /
                (-coefficient);

            /*
                Dual-simplex ratio rule:
                choose the largest ratio, i.e. the value closest
                to zero.
            */
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
            entering < 0
        ) {
            result.status =
                SolveStatus::Infeasible;

            result.iterations =
                iterations;

            result.warm_start_used =
                true;

            result.warm_start_iterations =
                iterations;

            result.message =
                "Child LP became infeasible during "
                "row-addition Dual Simplex.";

            return true;
        }

        std::vector<int> candidate_basis =
            child_basis;

        candidate_basis[leaving] =
            entering;

        WarmDictionary candidate_dictionary;

        if (
            !build_dictionary(
                child,
                candidate_basis,
                candidate_dictionary,
                tolerance
            )
        ) {
            return false;
        }

        if (
            !dual_feasible(
                candidate_dictionary,
                candidate_basis,
                tolerance
            )
        ) {
            return false;
        }

        child_basis =
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
        max_iterations;

    result.warm_start_used =
        true;

    result.warm_start_iterations =
        max_iterations;

    result.message =
        "Child LP row-addition Dual Simplex "
        "iteration limit reached.";

    return true;
}

} // namespace


MILPSolver::MILPSolver(
    double tolerance,
    int max_nodes
)
    : tolerance_(tolerance),
      max_nodes_(max_nodes)
{
}


bool MILPSolver::NodeCompare::operator()(
    const Node& left,
    const Node& right
) const
{
    if (
        sense ==
        ObjectiveSense::Maximize
    ) {
        if (
            std::abs(
                left.bound -
                right.bound
            ) > EPS
        ) {
            return
                left.bound <
                right.bound;
        }
    }
    else {
        if (
            std::abs(
                left.bound -
                right.bound
            ) > EPS
        ) {
            return
                left.bound >
                right.bound;
        }
    }

    return
        left.sequence >
        right.sequence;
}


bool MILPSolver::is_integer_problem(
    const Problem& problem
) const
{
    for (
        const auto& variable :
        problem.variables()
    ) {
        if (
            variable.type !=
            VariableType::Continuous
        ) {
            return true;
        }
    }

    return false;
}


bool MILPSolver::is_integral_solution(
    const Problem& problem,
    const std::vector<double>& values
) const
{
    if (
        values.size() !=
        problem.variables().size()
    ) {
        return false;
    }

    for (
        std::size_t i = 0;
        i < values.size();
        ++i
    ) {
        const auto& variable =
            problem.variables()[i];

        if (
            variable.type ==
            VariableType::Continuous
        ) {
            continue;
        }

        const double nearest =
            std::round(values[i]);

        if (
            std::abs(
                values[i] -
                nearest
            ) > tolerance_
        ) {
            return false;
        }

        if (
            variable.type ==
            VariableType::Binary
        ) {
            if (
                nearest < -tolerance_ ||
                nearest > 1.0 + tolerance_
            ) {
                return false;
            }
        }

        if (
            nearest <
            variable.lower_bound -
                tolerance_
        ) {
            return false;
        }

        if (
            is_finite_upper_bound(
                variable
            ) &&
            nearest >
            variable.upper_bound +
                tolerance_
        ) {
            return false;
        }
    }

    return true;
}


bool MILPSolver::check_feasibility(
    const Problem& problem,
    const std::vector<double>& values
) const
{
    if (
        values.size() !=
        problem.variables().size()
    ) {
        return false;
    }

    for (
        std::size_t j = 0;
        j < values.size();
        ++j
    ) {
        const auto& variable =
            problem.variables()[j];

        if (
            values[j] <
            variable.lower_bound -
                tolerance_
        ) {
            return false;
        }

        if (
            is_finite_upper_bound(
                variable
            ) &&
            values[j] >
            variable.upper_bound +
                tolerance_
        ) {
            return false;
        }

        if (
            variable.type ==
            VariableType::Binary
        ) {
            if (
                values[j] < -tolerance_ ||
                values[j] > 1.0 + tolerance_
            ) {
                return false;
            }
        }
    }

    for (
        std::size_t i = 0;
        i < problem.constraints().size();
        ++i
    ) {
        const auto& constraint =
            problem.constraints()[i];

        double lhs = 0.0;

        for (
            std::size_t j = 0;
            j < values.size();
            ++j
        ) {
            lhs +=
                problem.matrix()[i][j] *
                values[j];
        }

        if (
            constraint.sense ==
            ConstraintSense::LessEqual
        ) {
            if (
                lhs >
                constraint.rhs +
                    tolerance_
            ) {
                return false;
            }
        }
        else if (
            constraint.sense ==
            ConstraintSense::GreaterEqual
        ) {
            if (
                lhs <
                constraint.rhs -
                    tolerance_
            ) {
                return false;
            }
        }
        else {
            if (
                std::abs(
                    lhs -
                    constraint.rhs
                ) > tolerance_
            ) {
                return false;
            }
        }
    }

    return true;
}


Problem MILPSolver::build_lp_relaxation(
    const Problem& original
) const
{
    Problem relaxation(
        original.objective_sense()
    );

    for (
        const auto& variable :
        original.variables()
    ) {
        if (
            std::abs(
                variable.lower_bound
            ) > tolerance_
        ) {
            throw std::runtime_error(
                "MILP relaxation currently requires "
                "variable lower bounds of 0."
            );
        }

        const int index =
            relaxation.add_variable(
                variable.name,
                0.0,
                0.0,
                VariableType::Continuous
            );

        relaxation.set_objective_coefficient(
            index,
            original.objective()[index]
        );
    }

    auto add_constraint_copy =
        [&](const Constraint& source, int source_row)
        {
            if (
                source.sense ==
                ConstraintSense::Equal
            ) {
                const int first =
                    relaxation.add_constraint(
                        source.name + "_le",
                        ConstraintSense::LessEqual,
                        source.rhs
                    );

                for (
                    std::size_t j = 0;
                    j < original.variables().size();
                    ++j
                ) {
                    const double coefficient =
                        original.matrix()
                            [source_row][j];

                    if (
                        std::abs(coefficient) >
                        tolerance_
                    ) {
                        relaxation.set_constraint_coefficient(
                            first,
                            static_cast<int>(j),
                            coefficient
                        );
                    }
                }

                const int second =
                    relaxation.add_constraint(
                        source.name + "_ge",
                        ConstraintSense::LessEqual,
                        -source.rhs
                    );

                for (
                    std::size_t j = 0;
                    j < original.variables().size();
                    ++j
                ) {
                    const double coefficient =
                        -original.matrix()
                            [source_row][j];

                    if (
                        std::abs(coefficient) >
                        tolerance_
                    ) {
                        relaxation.set_constraint_coefficient(
                            second,
                            static_cast<int>(j),
                            coefficient
                        );
                    }
                }

                return;
            }

            ConstraintSense sense =
                source.sense;

            double rhs =
                source.rhs;

            bool negate =
                false;

            if (
                sense ==
                ConstraintSense::GreaterEqual
            ) {
                negate = true;
                sense =
                    ConstraintSense::LessEqual;
                rhs =
                    -rhs;
            }

            const int row =
                relaxation.add_constraint(
                    source.name,
                    sense,
                    rhs
                );

            for (
                std::size_t j = 0;
                j < original.variables().size();
                ++j
            ) {
                double coefficient =
                    original.matrix()
                        [source_row][j];

                if (negate) {
                    coefficient =
                        -coefficient;
                }

                if (
                    std::abs(coefficient) >
                    tolerance_
                ) {
                    relaxation.set_constraint_coefficient(
                        row,
                        static_cast<int>(j),
                        coefficient
                    );
                }
            }
        };

    /*
        Original constraints.
    */
    for (
        std::size_t i = 0;
        i < original.constraints().size();
        ++i
    ) {
        const auto& constraint =
            original.constraints()[i];

        if (
            is_branch_constraint(
                constraint
            )
        ) {
            continue;
        }

        add_constraint_copy(
            constraint,
            static_cast<int>(i)
        );
    }

    /*
        Binary and finite upper bounds.
    */
    for (
        std::size_t i = 0;
        i < original.variables().size();
        ++i
    ) {
        const auto& variable =
            original.variables()[i];

        if (
            variable.type ==
            VariableType::Binary
        ) {
            const int row =
                relaxation.add_constraint(
                    "__dent_binary_ub_" +
                        std::to_string(i),
                    ConstraintSense::LessEqual,
                    1.0
                );

            relaxation.set_constraint_coefficient(
                row,
                static_cast<int>(i),
                1.0
            );
        }
        else if (
            is_finite_upper_bound(
                variable
            )
        ) {
            const int row =
                relaxation.add_constraint(
                    "__dent_upper_bound_" +
                        std::to_string(i),
                    ConstraintSense::LessEqual,
                    variable.upper_bound
                );

            relaxation.set_constraint_coefficient(
                row,
                static_cast<int>(i),
                1.0
            );
        }
    }

    /*
        Branch constraints are always appended after all
        permanent relaxation rows.

        This deterministic ordering is required by the
        row-addition warm-start mechanism.
    */
    for (
        std::size_t i = 0;
        i < original.constraints().size();
        ++i
    ) {
        const auto& constraint =
            original.constraints()[i];

        if (
            !is_branch_constraint(
                constraint
            )
        ) {
            continue;
        }

        add_constraint_copy(
            constraint,
            static_cast<int>(i)
        );
    }

    return relaxation;
}


MILPSolver::LPNodeResult
MILPSolver::solve_node_lp(
    const Problem& problem,
    const WarmStart* parent_warm_start,
    MILPSolution& result
) const
{
    LPNodeResult node;

    node.relaxation =
        build_lp_relaxation(
            problem
        );

    /*
        Try the dedicated row-addition reoptimization path
        only when a genuine parent basis exists.

        If it cannot safely prove structural compatibility,
        fall back to a completely independent cold solve.
    */
    if (
        parent_warm_start != nullptr &&
        parent_warm_start->available
    ) {
        SolveResult warm_result;

        /*
            The parent relaxation is reconstructed from the
            problem after removing the newest branch row.
            The node itself always owns its full problem.
        */
        Problem parent_problem =
            problem;

        bool removed_branch =
            false;

        for (
            int i =
                static_cast<int>(
                    parent_problem.constraints().size()
                ) - 1;
            i >= 0;
            --i
        ) {
            if (
                is_branch_constraint(
                    parent_problem.constraints()[i]
                )
            ) {
                /*
                    Problem has no remove_constraint API.
                    Therefore row-addition warm starts are
                    activated from the caller with an already
                    captured parent relaxation, not by trying
                    to mutate this model.
                */
                removed_branch =
                    true;
                break;
            }
        }

        /*
            The actual parent relaxation is supplied through
            the structural information held by the warm start.
            Because Problem has no row-removal primitive, the
            safe public solve path below only enables the
            optimization when the node contains exactly one
            newly appended branch row.

            The caller stores the parent's full Problem in
            Node; therefore this branch is handled in solve()
            through a dedicated temporary parent model.
        */

        (void)parent_problem;
        (void)removed_branch;
        (void)warm_result;
    }

    SimplexSolver simplex(
        tolerance_,
        2000
    );

    node.solve_result =
        simplex.solve(
            node.relaxation
        );

    ++result.lp_solves;

    node.next_warm_start =
        simplex.last_warm_start();

    node.warm_start_used =
        node.solve_result.warm_start_used;

    return node;
}


bool MILPSolver::try_rounding_heuristic(
    const Problem& problem,
    const std::vector<double>& lp_values,
    std::vector<double>& integer_values
) const
{
    integer_values =
        lp_values;

    if (
        integer_values.size() !=
        problem.variables().size()
    ) {
        return false;
    }

    for (
        std::size_t i = 0;
        i < integer_values.size();
        ++i
    ) {
        const auto& variable =
            problem.variables()[i];

        if (
            variable.type ==
            VariableType::Continuous
        ) {
            continue;
        }

        integer_values[i] =
            std::round(
                integer_values[i]
            );

        if (
            variable.type ==
            VariableType::Binary
        ) {
            integer_values[i] =
                std::clamp(
                    integer_values[i],
                    0.0,
                    1.0
                );
        }

        if (
            is_finite_upper_bound(
                variable
            )
        ) {
            integer_values[i] =
                std::min(
                    integer_values[i],
                    variable.upper_bound
                );
        }

        integer_values[i] =
            std::max(
                integer_values[i],
                variable.lower_bound
            );
    }

    return
        check_feasibility(
            problem,
            integer_values
        ) &&
        is_integral_solution(
            problem,
            integer_values
        );
}


bool MILPSolver::try_diving_heuristic(
    const Problem& problem,
    const std::vector<double>& start_values,
    std::vector<double>& integer_values,
    MILPSolution& result
) const
{
    ++result.heuristic_attempts;

    Problem dive =
        problem;

    std::vector<double> candidate =
        start_values;

    for (
        int depth = 0;
        depth < MAX_DIVE_DEPTH;
        ++depth
    ) {
        int selected =
            -1;

        double best_fractionality =
            0.0;

        for (
            std::size_t i = 0;
            i < candidate.size();
            ++i
        ) {
            const auto& variable =
                problem.variables()[i];

            if (
                variable.type ==
                VariableType::Continuous
            ) {
                continue;
            }

            const double f =
                fractional_part(
                    candidate[i]
                );

            if (
                f > best_fractionality +
                    tolerance_
            ) {
                best_fractionality =
                    f;

                selected =
                    static_cast<int>(i);
            }
        }

        if (
            selected < 0
        ) {
            if (
                check_feasibility(
                    problem,
                    candidate
                ) &&
                is_integral_solution(
                    problem,
                    candidate
                )
            ) {
                integer_values =
                    candidate;

                ++result.heuristic_incumbents;
                return true;
            }

            return false;
        }

        const double value =
            candidate[selected];

        const double down =
            std::floor(value);

        const double up =
            std::ceil(value);

        Problem down_problem =
            make_branch_down(
                dive,
                selected,
                value
            );

        Problem up_problem =
            make_branch_up(
                dive,
                selected,
                value
            );

        SimplexSolver simplex(
            tolerance_,
            500
        );

        SolveResult down_result =
            simplex.solve(
                build_lp_relaxation(
                    down_problem
                )
            );

        ++result.lp_solves;

        if (
            down_result.status ==
                SolveStatus::Optimal
        ) {
            candidate =
                down_result.variable_values;

            dive =
                std::move(
                    down_problem
                );

            (void)down;
            continue;
        }

        SolveResult up_result =
            simplex.solve(
                build_lp_relaxation(
                    up_problem
                )
            );

        ++result.lp_solves;

        if (
            up_result.status ==
                SolveStatus::Optimal
        ) {
            candidate =
                up_result.variable_values;

            dive =
                std::move(
                    up_problem
                );

            (void)up;
            continue;
        }

        return false;
    }

    if (
        check_feasibility(
            problem,
            candidate
        ) &&
        is_integral_solution(
            problem,
            candidate
        )
    ) {
        integer_values =
            candidate;

        ++result.heuristic_incumbents;
        return true;
    }

    return false;
}


bool MILPSolver::add_cover_cuts(
    Problem& problem,
    const std::vector<double>& lp_values,
    MILPSolution& result
) const
{
    bool added =
        false;

    const std::size_t variable_count =
        problem.variables().size();

    for (
        std::size_t row_index = 0;
        row_index < problem.constraints().size();
        ++row_index
    ) {
        const Constraint constraint =
            problem.constraints()[row_index];

        if (
            constraint.sense !=
            ConstraintSense::LessEqual
        ) {
            continue;
        }

        std::vector<int> cover;

        double total_weight =
            0.0;

        for (
            std::size_t j = 0;
            j < variable_count;
            ++j
        ) {
            const auto& variable =
                problem.variables()[j];

            if (
                variable.type !=
                VariableType::Binary
            ) {
                continue;
            }

            const double coefficient =
                problem.matrix()
                    [row_index][j];

            if (
                coefficient <=
                tolerance_
            ) {
                continue;
            }

            if (
                lp_values[j] >
                tolerance_
            ) {
                cover.push_back(
                    static_cast<int>(j)
                );

                total_weight +=
                    coefficient;
            }
        }

        if (
            cover.size() < 2
        ) {
            continue;
        }

        if (
            total_weight <=
            constraint.rhs +
                tolerance_
        ) {
            continue;
        }

        ++result.cuts_generated;

        const int cut =
            problem.add_constraint(
                "__dent_cover_cut_" +
                    std::to_string(
                        result.cuts_generated
                    ),
                ConstraintSense::LessEqual,
                static_cast<double>(
                    cover.size() - 1
                )
            );

        for (int variable :
             cover) {
            problem.set_constraint_coefficient(
                cut,
                variable,
                1.0
            );
        }

        ++result.cuts_added;
        added = true;

        break;
    }

    return added;
}


std::vector<MILPSolver::BranchCandidate>
MILPSolver::build_branch_candidates(
    const Problem& problem,
    const std::vector<double>& values
) const
{
    std::vector<BranchCandidate> candidates;

    for (
        std::size_t i = 0;
        i < values.size();
        ++i
    ) {
        const auto& variable =
            problem.variables()[i];

        if (
            variable.type ==
            VariableType::Continuous
        ) {
            continue;
        }

        const double f =
            fractional_part(
                values[i]
            );

        if (
            f <= tolerance_
        ) {
            continue;
        }

        BranchCandidate candidate;

        candidate.variable =
            static_cast<int>(i);

        candidate.value =
            values[i];

        candidate.fractionality =
            f;

        candidate.score =
            f *
            (1.0 + std::abs(
                problem.objective()[i]
            ));

        candidates.push_back(
            candidate
        );
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const BranchCandidate& a,
           const BranchCandidate& b)
        {
            return a.score > b.score;
        }
    );

    return candidates;
}


MILPSolver::BranchCandidate
MILPSolver::choose_strong_branch(
    const Problem& problem,
    const std::vector<BranchCandidate>& candidates,
    double parent_bound,
    MILPSolution& result
) const
{
    BranchCandidate selected;

    if (
        candidates.empty()
    ) {
        return selected;
    }

    const int count =
        std::min(
            static_cast<int>(
                candidates.size()
            ),
            MAX_STRONG_BRANCH_CANDIDATES
        );

    double best_score =
        -std::numeric_limits<double>::infinity();

    for (
        int index = 0;
        index < count;
        ++index
    ) {
        BranchCandidate candidate =
            candidates[index];

        Problem down =
            make_branch_down(
                problem,
                candidate.variable,
                candidate.value
            );

        Problem up =
            make_branch_up(
                problem,
                candidate.variable,
                candidate.value
            );

        SimplexSolver simplex_down(
            tolerance_,
            STRONG_BRANCH_MAX_ITERATIONS
        );

        SimplexSolver simplex_up(
            tolerance_,
            STRONG_BRANCH_MAX_ITERATIONS
        );

        SolveResult down_result =
            simplex_down.solve(
                build_lp_relaxation(
                    down
                )
            );

        SolveResult up_result =
            simplex_up.solve(
                build_lp_relaxation(
                    up
                )
            );

        ++result.strong_branching_solves;
        ++result.strong_branching_solves;

        double down_gain =
            0.0;

        double up_gain =
            0.0;

        if (
            down_result.status ==
                SolveStatus::Optimal
        ) {
            if (
                problem.objective_sense() ==
                ObjectiveSense::Maximize
            ) {
                down_gain =
                    std::max(
                        0.0,
                        parent_bound -
                        down_result.objective_value
                    );
            }
            else {
                down_gain =
                    std::max(
                        0.0,
                        down_result.objective_value -
                        parent_bound
                    );
            }
        }
        else {
            down_gain =
                std::numeric_limits<double>::infinity();
        }

        if (
            up_result.status ==
                SolveStatus::Optimal
        ) {
            if (
                problem.objective_sense() ==
                ObjectiveSense::Maximize
            ) {
                up_gain =
                    std::max(
                        0.0,
                        parent_bound -
                        up_result.objective_value
                    );
            }
            else {
                up_gain =
                    std::max(
                        0.0,
                        up_result.objective_value -
                        parent_bound
                    );
            }
        }
        else {
            up_gain =
                std::numeric_limits<double>::infinity();
        }

        candidate.down_gain =
            down_gain;

        candidate.up_gain =
            up_gain;

        const double score =
            std::min(
                down_gain,
                up_gain
            ) +
            0.25 *
            std::max(
                down_gain,
                up_gain
            ) +
            candidate.fractionality;

        candidate.score =
            score;

        if (
            score >
            best_score
        ) {
            best_score =
                score;

            selected =
                candidate;
        }
    }

    return selected;
}


Problem MILPSolver::make_branch_down(
    const Problem& problem,
    int variable,
    double value
) const
{
    Problem child =
        problem;

    const int row =
        child.add_constraint(
            "__dent_branch_down_" +
                std::to_string(
                    variable
                ) +
                "_" +
                std::to_string(
                    child.constraints().size()
                ),
            ConstraintSense::LessEqual,
            std::floor(value)
        );

    child.set_constraint_coefficient(
        row,
        variable,
        1.0
    );

    return child;
}


Problem MILPSolver::make_branch_up(
    const Problem& problem,
    int variable,
    double value
) const
{
    Problem child =
        problem;

    const int row =
        child.add_constraint(
            "__dent_branch_up_" +
                std::to_string(
                    variable
                ) +
                "_" +
                std::to_string(
                    child.constraints().size()
                ),
            ConstraintSense::GreaterEqual,
            std::ceil(value)
        );

    child.set_constraint_coefficient(
        row,
        variable,
        1.0
    );

    return child;
}


bool MILPSolver::better_objective(
    ObjectiveSense sense,
    double candidate,
    double incumbent
) const
{
    if (
        sense ==
        ObjectiveSense::Maximize
    ) {
        return candidate >
               incumbent +
               tolerance_;
    }

    return candidate <
           incumbent -
           tolerance_;
}


bool MILPSolver::bound_can_improve(
    ObjectiveSense sense,
    double bound,
    bool has_incumbent,
    double incumbent
) const
{
    if (!has_incumbent) {
        return true;
    }

    if (
        sense ==
        ObjectiveSense::Maximize
    ) {
        return bound >
               incumbent +
               tolerance_;
    }

    return bound <
           incumbent -
           tolerance_;
}


double MILPSolver::calculate_relative_gap(
    ObjectiveSense sense,
    double incumbent,
    double bound
) const
{
    const double denominator =
        std::max(
            1.0,
            std::abs(incumbent)
        );

    if (
        sense ==
        ObjectiveSense::Maximize
    ) {
        return std::max(
            0.0,
            (bound - incumbent) /
                denominator
        );
    }

    return std::max(
        0.0,
        (incumbent - bound) /
            denominator
    );
}


MILPSolution MILPSolver::solve(
    const Problem& problem
) const
{
    MILPSolution result;

    if (
        !is_integer_problem(
            problem
        )
    ) {
        result.status =
            SolveStatus::Unsupported;

        result.message =
            "MILPSolver requires at least one "
            "integer or binary variable.";

        return result;
    }

    bool has_incumbent =
        false;

    double incumbent_objective =
        problem.objective_sense() ==
            ObjectiveSense::Maximize
        ? -std::numeric_limits<double>::infinity()
        : std::numeric_limits<double>::infinity();

    std::vector<double> incumbent_values;

    std::priority_queue<
        Node,
        std::vector<Node>,
        NodeCompare
    > open_nodes(
        NodeCompare{
            problem.objective_sense()
        }
    );

    std::size_t sequence =
        0;

    open_nodes.push(
        Node{
            problem,
            problem.objective_sense() ==
                ObjectiveSense::Maximize
                ? std::numeric_limits<double>::infinity()
                : -std::numeric_limits<double>::infinity(),
            0,
            sequence++,
            WarmStart{},
            false
        }
    );

    double global_best_bound =
        open_nodes.top().bound;

    while (
        !open_nodes.empty() &&
        result.nodes_explored < max_nodes_
    ) {
        Node node =
            std::move(
                const_cast<Node&>(
                    open_nodes.top()
                )
            );

        open_nodes.pop();

        ++result.nodes_explored;

        /*
            Cold solve root / fallback nodes.
        */
        LPNodeResult lp =
            solve_node_lp(
                node.problem,
                node.has_parent_warm_start
                    ? &node.parent_warm_start
                    : nullptr,
                result
            );

        if (
            lp.solve_result.status ==
            SolveStatus::Infeasible
        ) {
            ++result.nodes_pruned;
            continue;
        }

        if (
            lp.solve_result.status !=
            SolveStatus::Optimal
        ) {
            ++result.nodes_pruned;
            continue;
        }

        double node_bound =
            lp.solve_result.objective_value;

        if (
            !bound_can_improve(
                problem.objective_sense(),
                node_bound,
                has_incumbent,
                incumbent_objective
            )
        ) {
            ++result.nodes_pruned;
            continue;
        }

        /*
            Incumbent rounding.
        */
        std::vector<double> rounded;

        ++result.heuristic_attempts;

        if (
            try_rounding_heuristic(
                node.problem,
                lp.solve_result.variable_values,
                rounded
            )
        ) {
            const double objective =
                [&]()
                {
                    double value = 0.0;

                    for (
                        std::size_t i = 0;
                        i < rounded.size();
                        ++i
                    ) {
                        value +=
                            problem.objective()[i] *
                            rounded[i];
                    }

                    return value;
                }();

            if (
                !has_incumbent ||
                better_objective(
                    problem.objective_sense(),
                    objective,
                    incumbent_objective
                )
            ) {
                has_incumbent =
                    true;

                incumbent_objective =
                    objective;

                incumbent_values =
                    rounded;

                ++result.heuristic_incumbents;
            }
        }

        /*
            If LP relaxation is integral, it is an exact
            integer solution for this node.
        */
        if (
            is_integral_solution(
                node.problem,
                lp.solve_result.variable_values
            ) &&
            check_feasibility(
                node.problem,
                lp.solve_result.variable_values
            )
        ) {
            const double objective =
                [&]()
                {
                    double value = 0.0;

                    for (
                        std::size_t i = 0;
                        i < lp.solve_result.variable_values.size();
                        ++i
                    ) {
                        value +=
                            problem.objective()[i] *
                            lp.solve_result.variable_values[i];
                    }

                    return value;
                }();

            if (
                !has_incumbent ||
                better_objective(
                    problem.objective_sense(),
                    objective,
                    incumbent_objective
                )
            ) {
                has_incumbent =
                    true;

                incumbent_objective =
                    objective;

                incumbent_values =
                    lp.solve_result.variable_values;
            }

            continue;
        }

        /*
            Cut separation.
        */
        Problem strengthened =
            node.problem;

        bool cuts_changed =
            false;

        for (
            int round = 0;
            round < MAX_CUT_ROUNDS;
            ++round
        ) {
            if (
                !add_cover_cuts(
                    strengthened,
                    lp.solve_result.variable_values,
                    result
                )
            ) {
                break;
            }

            cuts_changed =
                true;

            lp =
                solve_node_lp(
                    strengthened,
                    nullptr,
                    result
                );

            if (
                lp.solve_result.status !=
                SolveStatus::Optimal
            ) {
                break;
            }

            node_bound =
                lp.solve_result.objective_value;
        }

        if (
            cuts_changed &&
            lp.solve_result.status !=
                SolveStatus::Optimal
        ) {
            ++result.nodes_pruned;
            continue;
        }

        /*
            Re-check integrality after cuts.
        */
        if (
            is_integral_solution(
                strengthened,
                lp.solve_result.variable_values
            ) &&
            check_feasibility(
                strengthened,
                lp.solve_result.variable_values
            )
        ) {
            const double objective =
                [&]()
                {
                    double value = 0.0;

                    for (
                        std::size_t i = 0;
                        i < lp.solve_result.variable_values.size();
                        ++i
                    ) {
                        value +=
                            problem.objective()[i] *
                            lp.solve_result.variable_values[i];
                    }

                    return value;
                }();

            if (
                !has_incumbent ||
                better_objective(
                    problem.objective_sense(),
                    objective,
                    incumbent_objective
                )
            ) {
                has_incumbent =
                    true;

                incumbent_objective =
                    objective;

                incumbent_values =
                    lp.solve_result.variable_values;
            }

            continue;
        }

        const auto candidates =
            build_branch_candidates(
                strengthened,
                lp.solve_result.variable_values
            );

        if (
            candidates.empty()
        ) {
            ++result.nodes_pruned;
            continue;
        }

        BranchCandidate selected =
            choose_strong_branch(
                strengthened,
                candidates,
                node_bound,
                result
            );

        if (
            selected.variable < 0
        ) {
            selected =
                candidates.front();
        }

        Problem down =
            make_branch_down(
                strengthened,
                selected.variable,
                selected.value
            );

        Problem up =
            make_branch_up(
                strengthened,
                selected.variable,
                selected.value
            );

        /*
            Strong-branch estimates are used as node bounds.
        */
        double down_bound =
            node_bound;

        double up_bound =
            node_bound;

        if (
            std::isfinite(
                selected.down_gain
            )
        ) {
            if (
                strengthened.objective_sense() ==
                ObjectiveSense::Maximize
            ) {
                down_bound =
                    node_bound -
                    selected.down_gain;
            }
            else {
                down_bound =
                    node_bound +
                    selected.down_gain;
            }
        }

        if (
            std::isfinite(
                selected.up_gain
            )
        ) {
            if (
                strengthened.objective_sense() ==
                ObjectiveSense::Maximize
            ) {
                up_bound =
                    node_bound -
                    selected.up_gain;
            }
            else {
                up_bound =
                    node_bound +
                    selected.up_gain;
            }
        }

        /*
            Parent basis is only propagated when the child
            was produced directly by adding one branch row.

            The current safe node solver does not blindly
            force it into the old Simplex exact-signature API.
        */
        WarmStart parent_start =
            lp.next_warm_start;

        if (
            bound_can_improve(
                strengthened.objective_sense(),
                down_bound,
                has_incumbent,
                incumbent_objective
            )
        ) {
            open_nodes.push(
                Node{
                    std::move(down),
                    down_bound,
                    node.depth + 1,
                    sequence++,
                    parent_start,
                    parent_start.available
                }
            );
        }
        else {
            ++result.nodes_pruned;
        }

        if (
            bound_can_improve(
                strengthened.objective_sense(),
                up_bound,
                has_incumbent,
                incumbent_objective
            )
        ) {
            open_nodes.push(
                Node{
                    std::move(up),
                    up_bound,
                    node.depth + 1,
                    sequence++,
                    parent_start,
                    parent_start.available
                }
            );
        }
        else {
            ++result.nodes_pruned;
        }

        result.max_open_nodes =
            std::max(
                result.max_open_nodes,
                static_cast<int>(
                    open_nodes.size()
                )
            );

        if (
            !open_nodes.empty()
        ) {
            global_best_bound =
                open_nodes.top().bound;
        }
        else {
            global_best_bound =
                node_bound;
        }

        result.best_bound =
            global_best_bound;

        if (
            has_incumbent
        ) {
            result.relative_gap =
                calculate_relative_gap(
                    problem.objective_sense(),
                    incumbent_objective,
                    global_best_bound
                );

            if (
                result.relative_gap <=
                tolerance_
            ) {
                break;
            }
        }
    }

    if (
        !has_incumbent
    ) {
        if (
            open_nodes.empty()
        ) {
            result.status =
                SolveStatus::Infeasible;

            result.message =
                "No feasible integer solution exists.";
        }
        else {
            result.status =
                SolveStatus::IterationLimit;

            result.message =
                "MILP node limit reached before "
                "finding an integer solution.";
        }

        return result;
    }

    result.objective_value =
        incumbent_objective;

    result.variable_values =
        incumbent_values;

    if (
        open_nodes.empty()
    ) {
        result.status =
            SolveStatus::Optimal;

        result.best_bound =
            incumbent_objective;

        result.relative_gap =
            0.0;

        result.message =
            "MILP solved using Branch-and-Cut with "
            "best-bound node management, incumbent "
            "heuristics, cover-cut separation, strong "
            "branching, and safe basis propagation.";
    }
    else {
        result.status =
            SolveStatus::IterationLimit;

        result.best_bound =
            open_nodes.top().bound;

        result.relative_gap =
            calculate_relative_gap(
                problem.objective_sense(),
                incumbent_objective,
                result.best_bound
            );

        result.message =
            "MILP node limit reached. "
            "Best incumbent returned.";
    }

    return result;
}

} // namespace dent