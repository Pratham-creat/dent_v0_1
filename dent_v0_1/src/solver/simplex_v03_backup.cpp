#include "dent/solver/simplex.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dent {
namespace {

std::vector<double> solve_linear_system(
    std::vector<std::vector<double>> A,
    std::vector<double> b,
    double tolerance)
{
    const int n = static_cast<int>(A.size());

    // Gaussian elimination with partial pivoting.
    for (int col = 0; col < n; ++col) {

        int pivot = col;

        for (int row = col + 1; row < n; ++row) {
            if (std::abs(A[row][col]) >
                std::abs(A[pivot][col])) {
                pivot = row;
            }
        }

        if (std::abs(A[pivot][col]) <= tolerance) {
            throw std::runtime_error(
                "Singular basis matrix."
            );
        }

        std::swap(A[pivot], A[col]);
        std::swap(b[pivot], b[col]);

        for (int row = col + 1; row < n; ++row) {

            double factor =
                A[row][col] / A[col][col];

            for (int j = col; j < n; ++j) {
                A[row][j] -=
                    factor * A[col][j];
            }

            b[row] -= factor * b[col];
        }
    }

    std::vector<double> x(n, 0.0);

    // Back substitution.
    for (int row = n - 1; row >= 0; --row) {

        double value = b[row];

        for (int j = row + 1; j < n; ++j) {
            value -=
                A[row][j] * x[j];
        }

        x[row] =
            value / A[row][row];
    }

    return x;
}

std::vector<double> transpose_solve(
    const std::vector<std::vector<double>>& B,
    const std::vector<double>& b,
    double tolerance)
{
    const int n =
        static_cast<int>(B.size());

    std::vector<std::vector<double>> BT(
        n,
        std::vector<double>(n)
    );

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            BT[i][j] = B[j][i];
        }
    }

    return solve_linear_system(
        BT,
        b,
        tolerance
    );
}

} // namespace


SimplexSolver::SimplexSolver(
    double tolerance,
    int max_iterations)
    : tolerance_(tolerance),
      max_iterations_(max_iterations)
{
}


SolveResult SimplexSolver::solve(
    const Problem& problem) const
{
    SolveResult result;

    const int m =
        static_cast<int>(
            problem.constraints().size()
        );

    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    /*
        v0.2 currently supports:

            maximize c^T x

        subject to:

            A x <= b
            x >= 0
            b >= 0
    */

    if (problem.objective_sense()
        != ObjectiveSense::Maximize)
    {
        result.message =
            "v0.2 supports maximization only.";

        return result;
    }

    for (const auto& variable :
         problem.variables())
    {
        if (std::abs(variable.lower_bound)
                > tolerance_
            || variable.upper_bound != 0.0)
        {
            result.message =
                "v0.2 requires x >= 0 "
                "with no finite upper bound.";

            return result;
        }
    }

    for (const auto& constraint :
         problem.constraints())
    {
        if (constraint.sense
                != ConstraintSense::LessEqual
            || constraint.rhs < -tolerance_)
        {
            result.message =
                "v0.2 requires <= constraints "
                "with non-negative RHS.";

            return result;
        }
    }

    /*
        Extended matrix:

              [ A | I ]

        Original variables:
              x1 ... xn

        Slack variables:
              s1 ... sm

        Initial basis:

              s1 ... sm
    */

    const int total_variables =
        n + m;

    std::vector<int> basis(m);

    for (int i = 0; i < m; ++i) {
        basis[i] = n + i;
    }

    std::vector<double> c(
        total_variables,
        0.0
    );

    for (int j = 0; j < n; ++j) {
        c[j] =
            problem.objective()[j];
    }

    auto get_column =
        [&](int column)
    {
        std::vector<double> result_column(
            m,
            0.0
        );

        // Original variable.
        if (column < n) {

            for (int i = 0; i < m; ++i) {
                result_column[i] =
                    problem.matrix()[i][column];
            }

        }
        // Slack variable.
        else {

            result_column[
                column - n
            ] = 1.0;
        }

        return result_column;
    };

    std::vector<double> b(m);

    for (int i = 0; i < m; ++i) {
        b[i] =
            problem.constraints()[i].rhs;
    }

    /*
        Main Revised Simplex loop.
    */

    for (int iteration = 0;
         iteration < max_iterations_;
         ++iteration)
    {
        result.iterations = iteration;

        /*
            Construct basis matrix B.
        */

        std::vector<std::vector<double>> B(
            m,
            std::vector<double>(m, 0.0)
        );

        std::vector<double> cB(m);

        for (int k = 0; k < m; ++k) {

            auto column =
                get_column(basis[k]);

            for (int i = 0; i < m; ++i) {
                B[i][k] =
                    column[i];
            }

            cB[k] =
                c[basis[k]];
        }

        /*
            Step 1:

                B x_B = b

            therefore:

                x_B = B^-1 b
        */

        std::vector<double> xB;

        try {

            xB =
                solve_linear_system(
                    B,
                    b,
                    tolerance_
                );

        }
        catch (const std::exception& e) {

            result.message =
                e.what();

            return result;
        }

        /*
            Check feasibility.
        */

        for (double value : xB) {

            if (value < -tolerance_) {

                result.status =
                    SolveStatus::Infeasible;

                result.message =
                    "Basic solution is infeasible.";

                return result;
            }
        }

        /*
            Step 2:

                B^T y = c_B

            y is the vector of
            simplex multipliers.
        */

        std::vector<double> y;

        try {

            y =
                transpose_solve(
                    B,
                    cB,
                    tolerance_
                );

        }
        catch (const std::exception& e) {

            result.message =
                e.what();

            return result;
        }

        /*
            Step 3:

                reduced_cost =
                    c_j - y^T a_j

            For maximization, a positive
            reduced cost means the variable
            can improve the objective.
        */

        int entering = -1;

        double best_reduced_cost =
            tolerance_;

        for (int j = 0;
             j < total_variables;
             ++j)
        {
            if (std::find(
                    basis.begin(),
                    basis.end(),
                    j)
                != basis.end())
            {
                continue;
            }

            auto column =
                get_column(j);

            double reduced_cost =
                c[j];

            for (int i = 0; i < m; ++i) {

                reduced_cost -=
                    y[i] * column[i];
            }

            if (reduced_cost >
                best_reduced_cost)
            {
                best_reduced_cost =
                    reduced_cost;

                entering = j;
            }
        }

        /*
            No positive reduced cost:

                current solution is optimal.
        */

        if (entering == -1) {

            result.variable_values.assign(
                n,
                0.0
            );

            for (int i = 0; i < m; ++i) {

                if (basis[i] < n) {

                    result.variable_values[
                        basis[i]
                    ] = xB[i];
                }
            }

            for (int j = 0; j < n; ++j) {

                result.objective_value +=
                    c[j] *
                    result.variable_values[j];
            }

            result.status =
                SolveStatus::Optimal;

            result.message =
                "Optimal solution found "
                "by revised simplex.";

            return result;
        }

        /*
            Step 4:

                d = B^-1 a_entering
        */

        std::vector<double> direction;

        try {

            direction =
                solve_linear_system(
                    B,
                    get_column(entering),
                    tolerance_
                );

        }
        catch (const std::exception& e) {

            result.message =
                e.what();

            return result;
        }

        /*
            Step 5:

                Minimum ratio test.

                theta =
                    min(x_Bi / d_i)

                for d_i > 0.
        */

        int leaving = -1;

        double best_ratio =
            std::numeric_limits<double>::infinity();

        for (int i = 0; i < m; ++i) {

            if (direction[i] >
                tolerance_)
            {
                double ratio =
                    xB[i] /
                    direction[i];

                if (ratio <
                    best_ratio -
                    tolerance_)
                {
                    best_ratio =
                        ratio;

                    leaving = i;
                }
            }
        }

        /*
            No valid leaving variable means
            the LP is unbounded.
        */

        if (leaving == -1) {

            result.status =
                SolveStatus::Unbounded;

            result.message =
                "LP is unbounded.";

            return result;
        }

        /*
            Pivot:

                entering variable replaces
                leaving basic variable.
        */

        basis[leaving] =
            entering;
    }

    result.status =
        SolveStatus::IterationLimit;

    result.message =
        "Maximum simplex iterations reached.";

    result.iterations =
        max_iterations_;

    return result;
}

} // namespace dent