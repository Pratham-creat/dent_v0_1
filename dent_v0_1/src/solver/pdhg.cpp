#include "dent/solver/pdhg.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace dent {

namespace {

constexpr double EPS = 1e-12;

struct CanonicalLP
{
    int variables = 0;
    int rows = 0;

    /*
        Canonical form:

            minimize c^T x

            A x <= b
            x >= 0
    */

    std::vector<std::vector<double>> A;

    std::vector<double> b;

    std::vector<double> c;

    /*
        True when the original objective was maximize.

        Internally PDHG always solves the minimization
        equivalent.
    */
    bool original_maximize = false;
};


bool finite_upper_bound(
    const Variable& variable
)
{
    return
        variable.upper_bound != 0.0 &&
        std::isfinite(
            variable.upper_bound
        );
}


bool approximately_zero(
    double value,
    double tolerance
)
{
    return std::abs(value) <= tolerance;
}


double dot(
    const std::vector<double>& a,
    const std::vector<double>& b
)
{
    const std::size_t n =
        std::min(
            a.size(),
            b.size()
        );

    double result = 0.0;

    for (
        std::size_t i = 0;
        i < n;
        ++i
    ) {
        result +=
            a[i] * b[i];
    }

    return result;
}


double squared_norm(
    const std::vector<double>& values
)
{
    double result = 0.0;

    for (double value : values) {
        result +=
            value * value;
    }

    return result;
}


double norm(
    const std::vector<double>& values
)
{
    return std::sqrt(
        squared_norm(
            values
        )
    );
}


CanonicalLP canonicalize(
    const Problem& problem,
    double tolerance
)
{
    CanonicalLP canonical;

    canonical.variables =
        static_cast<int>(
            problem.variables().size()
        );

    canonical.original_maximize =
        problem.objective_sense() ==
        ObjectiveSense::Maximize;

    /*
        PDHG uses minimization internally.

        maximize c^T x

        becomes

        minimize (-c)^T x
    */
    canonical.c.resize(
        canonical.variables,
        0.0
    );

    for (
        int j = 0;
        j < canonical.variables;
        ++j
    ) {
        canonical.c[j] =
            canonical.original_maximize
                ? -problem.objective()[j]
                : problem.objective()[j];
    }

    /*
        Current PDHG implementation uses x >= 0.

        A non-zero lower bound would require a variable
        translation:

            x = l + z

        which changes both the objective and RHS.

        Do that explicitly rather than silently producing
        an incorrect model.
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
            throw std::runtime_error(
                "PDHG currently requires variable "
                "lower bounds of 0."
            );
        }
    }

    auto append_less_equal =
        [&](
            const std::vector<double>& coefficients,
            double rhs
        )
        {
            if (
                rhs < -tolerance
            ) {
                /*
                    PDHG can mathematically handle negative
                    RHS, but the current primal-dual residual
                    implementation is intentionally kept in
                    the canonical nonnegative-RHS form.
                */

                std::vector<double> negated =
                    coefficients;

                for (double& value :
                     negated) {
                    value =
                        -value;
                }

                /*
                    (-a)x >= -b is not a <= row.
                    Therefore negative RHS rows are retained
                    and handled by the operator formulation.
                */
            }

            canonical.A.push_back(
                coefficients
            );

            canonical.b.push_back(
                rhs
            );
        };

    /*
        Convert all constraints to <= form.

            a x <= b

            a x >= b
                becomes
            -a x <= -b

            a x = b
                becomes
            a x <= b
            -a x <= -b
    */
    for (
        std::size_t i = 0;
        i < problem.constraints().size();
        ++i
    ) {
        const Constraint& constraint =
            problem.constraints()[i];

        std::vector<double> row(
            canonical.variables,
            0.0
        );

        for (
            int j = 0;
            j < canonical.variables;
            ++j
        ) {
            row[j] =
                problem.matrix()[i][j];
        }

        if (
            constraint.sense ==
            ConstraintSense::LessEqual
        ) {
            append_less_equal(
                row,
                constraint.rhs
            );
        }
        else if (
            constraint.sense ==
            ConstraintSense::GreaterEqual
        ) {
            for (double& value :
                 row) {
                value =
                    -value;
            }

            append_less_equal(
                row,
                -constraint.rhs
            );
        }
        else {
            append_less_equal(
                row,
                constraint.rhs
            );

            for (double& value :
                 row) {
                value =
                    -value;
            }

            append_less_equal(
                row,
                -constraint.rhs
            );
        }
    }

    /*
        Finite upper bounds:

            x_j <= u_j
    */
    for (
        int j = 0;
        j < canonical.variables;
        ++j
    ) {
        const auto& variable =
            problem.variables()[j];

        if (
            finite_upper_bound(
                variable
            )
        ) {
            std::vector<double> row(
                canonical.variables,
                0.0
            );

            row[j] =
                1.0;

            append_less_equal(
                row,
                variable.upper_bound
            );
        }
    }

    canonical.rows =
        static_cast<int>(
            canonical.A.size()
        );

    return canonical;
}


double frobenius_norm(
    const std::vector<std::vector<double>>& A
)
{
    double result = 0.0;

    for (
        const auto& row :
        A
    ) {
        for (double value :
             row) {
            result +=
                value * value;
        }
    }

    return std::sqrt(
        result
    );
}


void multiply_A(
    const CanonicalLP& model,
    const std::vector<double>& x,
    std::vector<double>& result
)
{
    result.assign(
        model.rows,
        0.0
    );

    for (
        int i = 0;
        i < model.rows;
        ++i
    ) {
        result[i] =
            dot(
                model.A[i],
                x
            );
    }
}


void multiply_AT(
    const CanonicalLP& model,
    const std::vector<double>& y,
    std::vector<double>& result
)
{
    result.assign(
        model.variables,
        0.0
    );

    for (
        int i = 0;
        i < model.rows;
        ++i
    ) {
        const double multiplier =
            y[i];

        if (
            std::abs(multiplier) <= EPS
        ) {
            continue;
        }

        for (
            int j = 0;
            j < model.variables;
            ++j
        ) {
            result[j] +=
                model.A[i][j] *
                multiplier;
        }
    }
}


void project_nonnegative(
    std::vector<double>& values
)
{
    for (double& value :
         values) {
        if (
            value < 0.0
        ) {
            value =
                0.0;
        }
    }
}


double primal_objective(
    const CanonicalLP& model,
    const std::vector<double>& x
)
{
    return dot(
        model.c,
        x
    );
}


double dual_objective(
    const CanonicalLP& model,
    const std::vector<double>& y
)
{
    /*
        Primal:

            min c^T x
            A x <= b
            x >= 0

        Dual:

            max -b^T y
            A^T y + c >= 0
            y >= 0
    */
    return
        -dot(
            model.b,
            y
        );
}


double primal_violation(
    const CanonicalLP& model,
    const std::vector<double>& Ax
)
{
    double maximum =
        0.0;

    for (
        int i = 0;
        i < model.rows;
        ++i
    ) {
        maximum =
            std::max(
                maximum,
                Ax[i] -
                    model.b[i]
            );
    }

    return std::max(
        0.0,
        maximum
    );
}


double dual_violation(
    const CanonicalLP& model,
    const std::vector<double>& ATy
)
{
    /*
        Dual feasibility:

            c + A^T y >= 0

        Violation is:

            max(0, -(c + A^T y))
    */
    double maximum =
        0.0;

    for (
        int j = 0;
        j < model.variables;
        ++j
    ) {
        const double value =
            model.c[j] +
            ATy[j];

        maximum =
            std::max(
                maximum,
                -value
            );
    }

    return std::max(
        0.0,
        maximum
    );
}


double duality_gap(
    double primal,
    double dual
)
{
    /*
        For a feasible minimization primal / dual pair:

            dual <= primal

        Therefore:

            gap = primal - dual
    */
    return std::max(
        0.0,
        primal - dual
    );
}


std::vector<double> recover_original_objective_values(
    const CanonicalLP& model,
    const std::vector<double>& x
)
{
    /*
        The variable values themselves do not change when
        converting maximize -> minimize(-c).
    */
    return x;
}

} // namespace


PDHGSolver::PDHGSolver(
    double tolerance,
    int max_iterations
)
    : tolerance_(tolerance),
      max_iterations_(max_iterations)
{
}


void PDHGSolver::set_warm_start(
    const WarmStart& warm_start
)
{
    warm_start_ =
        warm_start;
}


void PDHGSolver::clear_warm_start()
{
    warm_start_.clear();
}


bool PDHGSolver::has_warm_start() const
{
    return warm_start_.available;
}


SolveResult PDHGSolver::solve(
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
                "PDHG requires at least one variable.";

            return result;
        }

        CanonicalLP model =
            canonicalize(
                problem,
                tolerance_
            );

        if (
            model.rows == 0
        ) {
            /*
                No constraints.

                The optimum can only be finite when all
                minimization coefficients are nonnegative.
            */
            bool unbounded =
                false;

            for (double coefficient :
                 model.c) {
                if (
                    coefficient <
                    -tolerance_
                ) {
                    unbounded =
                        true;
                    break;
                }
            }

            if (unbounded) {
                result.status =
                    SolveStatus::Unbounded;

                result.message =
                    "PDHG detected an unbounded "
                    "unconstrained direction.";

                return result;
            }

            result.status =
                SolveStatus::Optimal;

            result.variable_values.assign(
                model.variables,
                0.0
            );

            result.objective_value =
                problem.objective_sense() ==
                    ObjectiveSense::Maximize
                    ? 0.0
                    : 0.0;

            result.message =
                "PDHG solved an unconstrained LP.";

            return result;
        }

        /*
            Operator norm estimate.

            We use the Frobenius norm:

                ||A||_2 <= ||A||_F

            and choose

                tau * sigma * ||A||_F^2 < 1

            with a conservative safety factor.
        */
        const double operator_norm =
            frobenius_norm(
                model.A
            );

        if (
            operator_norm <=
            tolerance_
        ) {
            result.status =
                SolveStatus::Unsupported;

            result.message =
                "PDHG received a numerically zero "
                "constraint matrix.";

            return result;
        }

        constexpr double SAFETY =
            0.95;

        const double tau =
            SAFETY /
            operator_norm;

        const double sigma =
            SAFETY /
            operator_norm;

        /*
            x = primal variable
            y = dual variable
        */
        std::vector<double> x(
            model.variables,
            0.0
        );

        std::vector<double> y(
            model.rows,
            0.0
        );

        /*
            Warm start.

            PDHG warm starts use primal variable values.
            We intentionally do not reinterpret a simplex
            basis as a first-order warm start.
        */
        if (
            warm_start_.available &&
            warm_start_.variable_values.size() ==
                static_cast<std::size_t>(
                    model.variables
                )
        ) {
            x =
                warm_start_.variable_values;

            project_nonnegative(
                x
            );

            result.warm_start_used =
                true;
        }

        std::vector<double> x_bar =
            x;

        std::vector<double> Ax;
        std::vector<double> ATy;

        double best_gap =
            std::numeric_limits<double>::infinity();

        int stable_iterations =
            0;

        constexpr int REQUIRED_STABLE =
            5;

        /*
            Main Chambolle-Pock / PDHG iteration:

                y(k+1)
                  = P_+( y(k)
                      + sigma(A x_bar(k) - b) )

                x(k+1)
                  = P_+( x(k)
                      - tau(c + A^T y(k+1)) )

                x_bar(k+1)
                  = x(k+1)
                    + theta(x(k+1)-x(k))

            theta = 1 gives the standard extrapolated
            primal-dual iteration.
        */
        constexpr double theta =
            1.0;

        for (
            int iteration = 1;
            iteration <= max_iterations_;
            ++iteration
        ) {
            std::vector<double> old_x =
                x;

            multiply_A(
                model,
                x_bar,
                Ax
            );

            /*
                Dual ascent.
            */
            for (
                int i = 0;
                i < model.rows;
                ++i
            ) {
                y[i] +=
                    sigma *
                    (
                        Ax[i] -
                        model.b[i]
                    );
            }

            project_nonnegative(
                y
            );

            /*
                Primal descent.
            */
            multiply_AT(
                model,
                y,
                ATy
            );

            for (
                int j = 0;
                j < model.variables;
                ++j
            ) {
                x[j] -=
                    tau *
                    (
                        model.c[j] +
                        ATy[j]
                    );
            }

            project_nonnegative(
                x
            );

            /*
                Extrapolation.
            */
            for (
                int j = 0;
                j < model.variables;
                ++j
            ) {
                x_bar[j] =
                    x[j] +
                    theta *
                    (
                        x[j] -
                        old_x[j]
                    );
            }

            /*
                Diagnostics.
            */
            multiply_A(
                model,
                x,
                Ax
            );

            multiply_AT(
                model,
                y,
                ATy
            );

            const double primal =
                primal_objective(
                    model,
                    x
                );

            const double dual =
                dual_objective(
                    model,
                    y
                );

            const double gap =
                duality_gap(
                    primal,
                    dual
                );

            const double primal_residual =
                primal_violation(
                    model,
                    Ax
                );

            const double dual_residual =
                dual_violation(
                    model,
                    ATy
                );

            std::vector<double> step_difference(
                model.variables,
                0.0
            );

            for (
                int j = 0;
                j < model.variables;
                ++j
            ) {
                step_difference[j] =
                    x[j] -
                    old_x[j];
            }

            const double step_norm =
                norm(
                    step_difference
                );

            const double scale =
                std::max(
                    1.0,
                    std::max(
                        std::abs(primal),
                        std::abs(dual)
                    )
                );

            const double scaled_gap =
                gap /
                scale;

            const bool converged =
                primal_residual <= tolerance_ &&
                dual_residual <= tolerance_ &&
                (
                    scaled_gap <= tolerance_ ||
                    step_norm <= tolerance_
                );

            if (
                gap <
                best_gap
            ) {
                best_gap =
                    gap;
            }

            if (
                converged
            ) {
                ++stable_iterations;
            }
            else {
                stable_iterations =
                    0;
            }

            /*
                Requiring several consecutive successful
                iterations avoids declaring convergence from
                one numerically lucky iterate.
            */
            if (
                stable_iterations >=
                REQUIRED_STABLE
            ) {
                result.status =
                    SolveStatus::Optimal;

                result.iterations =
                    iteration;

                result.variable_values =
                    recover_original_objective_values(
                        model,
                        x
                    );

                const double internal_objective =
                    primal_objective(
                        model,
                        x
                    );

                result.objective_value =
                    model.original_maximize
                        ? -internal_objective
                        : internal_objective;

                result.warm_start_iterations =
                    iteration;

                result.message =
                    "PDHG converged with primal residual " +
                    std::to_string(
                        primal_residual
                    ) +
                    ", dual residual " +
                    std::to_string(
                        dual_residual
                    ) +
                    ", and scaled duality gap " +
                    std::to_string(
                        scaled_gap
                    ) +
                    ".";

                /*
                    Store the primal iterate as a PDHG warm
                    start. The generic WarmStart structure is
                    used here only for the variable vector;
                    no simplex basis is claimed.
                */
                warm_start_.available =
                    true;

                warm_start_.variable_values =
                    x;

                warm_start_.basis.clear();

                warm_start_.rows =
                    model.rows;

                warm_start_.columns =
                    model.variables;

                warm_start_.source =
                    "pdhg";

                return result;
            }
        }

        /*
            Iteration limit.

            Return the best available primal iterate, but do
            not label it Optimal merely because the iteration
            counter expired.
        */
        multiply_A(
            model,
            x,
            Ax
        );

        multiply_AT(
            model,
            y,
            ATy
        );

        const double primal =
            primal_objective(
                model,
                x
            );

        const double dual =
            dual_objective(
                model,
                y
            );

        const double gap =
            duality_gap(
                primal,
                dual
            );

        const double primal_residual =
            primal_violation(
                model,
                Ax
            );

        const double dual_residual =
            dual_violation(
                model,
                ATy
            );

        result.status =
            SolveStatus::IterationLimit;

        result.iterations =
            max_iterations_;

        result.variable_values =
            recover_original_objective_values(
                model,
                x
            );

        result.objective_value =
            model.original_maximize
                ? -primal
                : primal;

        result.warm_start_iterations =
            max_iterations_;

        result.message =
            "PDHG reached the iteration limit. "
            "Primal residual=" +
            std::to_string(
                primal_residual
            ) +
            ", dual residual=" +
            std::to_string(
                dual_residual
            ) +
            ", duality gap=" +
            std::to_string(
                gap
            ) +
            ".";

        warm_start_.available =
            true;

        warm_start_.variable_values =
            x;

        warm_start_.basis.clear();

        warm_start_.rows =
            model.rows;

        warm_start_.columns =
            model.variables;

        warm_start_.source =
            "pdhg";

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