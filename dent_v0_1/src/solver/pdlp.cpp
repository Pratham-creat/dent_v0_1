#include "dent/solver/pdlp.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "dent/solver/simplex.hpp"

namespace dent {

namespace {

constexpr double EPS = 1e-12;
constexpr double SAFETY = 0.90;

struct CanonicalLP
{
    int variables = 0;
    int rows = 0;

    std::vector<std::vector<double>> A;
    std::vector<double> b;
    std::vector<double> c;

    std::vector<double> lower_bounds;

    bool original_maximize = false;
};

double dot(
    const std::vector<double>& a,
    const std::vector<double>& b)
{
    const std::size_t n =
        std::min(a.size(), b.size());

    double value = 0.0;

    for (std::size_t i = 0; i < n; ++i)
        value += a[i] * b[i];

    return value;
}

double matrix_norm(
    const std::vector<std::vector<double>>& A)
{
    double value = 0.0;

    for (const auto& row : A)
        for (double x : row)
            value += x * x;

    return std::sqrt(value);
}

void project_nonnegative(
    std::vector<double>& x)
{
    for (double& value : x)
    {
        if (value < 0.0)
            value = 0.0;
    }
}

CanonicalLP canonicalize(
    const Problem& problem,
    double tolerance)
{
    CanonicalLP model;

    model.variables =
        static_cast<int>(
            problem.variables().size());

    model.original_maximize =
        problem.objective_sense() ==
        ObjectiveSense::Maximize;

    model.c.resize(
        model.variables,
        0.0);

    model.lower_bounds.resize(
        model.variables,
        0.0);

    for (int j = 0;
         j < model.variables;
         ++j)
    {
        const auto& variable =
            problem.variables()[j];

        if (variable.lower_bound < -tolerance)
        {
            throw std::runtime_error(
                "PDLP requires nonnegative variable lower bounds.");
        }

        model.lower_bounds[j] =
            variable.lower_bound;
    }

    /*
     * Internal problem is minimization.
     */
    for (int j = 0;
         j < model.variables;
         ++j)
    {
        model.c[j] =
            model.original_maximize
                ? -problem.objective()[j]
                : problem.objective()[j];
    }

    auto append_less_equal =
        [&](const std::vector<double>& row,
            double rhs)
        {
            double shifted_rhs =
                rhs;

            for (int j = 0;
                 j < model.variables;
                 ++j)
            {
                shifted_rhs -=
                    row[j] *
                    model.lower_bounds[j];
            }

            model.A.push_back(row);
            model.b.push_back(shifted_rhs);
        };

    /*
     * Convert constraints to <= form.
     */
    for (std::size_t i = 0;
         i < problem.constraints().size();
         ++i)
    {
        const Constraint& constraint =
            problem.constraints()[i];

        std::vector<double> row(
            model.variables,
            0.0);

        for (int j = 0;
             j < model.variables;
             ++j)
        {
            row[j] =
                problem.matrix()[i][j];
        }

        if (
            constraint.sense ==
            ConstraintSense::LessEqual)
        {
            append_less_equal(
                row,
                constraint.rhs);
        }
        else if (
            constraint.sense ==
            ConstraintSense::GreaterEqual)
        {
            for (double& value : row)
                value = -value;

            append_less_equal(
                row,
                -constraint.rhs);
        }
        else
        {
            append_less_equal(
                row,
                constraint.rhs);

            for (double& value : row)
                value = -value;

            append_less_equal(
                row,
                -constraint.rhs);
        }
    }

    /*
     * Finite upper bounds.
     */
    for (int j = 0;
         j < model.variables;
         ++j)
    {
        const auto& variable =
            problem.variables()[j];

        if (
            !std::isfinite(
                variable.upper_bound) ||
            variable.upper_bound <= 0.0)
        {
            continue;
        }

        std::vector<double> row(
            model.variables,
            0.0);

        row[j] = 1.0;

        append_less_equal(
            row,
            variable.upper_bound);
    }

    model.rows =
        static_cast<int>(
            model.A.size());

    return model;
}

void multiply_A(
    const CanonicalLP& model,
    const std::vector<double>& x,
    std::vector<double>& result)
{
    result.assign(
        model.rows,
        0.0);

    for (int i = 0;
         i < model.rows;
         ++i)
    {
        result[i] =
            dot(
                model.A[i],
                x);
    }
}

void multiply_AT(
    const CanonicalLP& model,
    const std::vector<double>& y,
    std::vector<double>& result)
{
    result.assign(
        model.variables,
        0.0);

    for (int i = 0;
         i < model.rows;
         ++i)
    {
        if (std::abs(y[i]) <= EPS)
            continue;

        for (int j = 0;
             j < model.variables;
             ++j)
        {
            result[j] +=
                model.A[i][j] *
                y[i];
        }
    }
}

double primal_violation(
    const CanonicalLP& model,
    const std::vector<double>& Ax)
{
    double violation = 0.0;

    for (int i = 0;
         i < model.rows;
         ++i)
    {
        violation =
            std::max(
                violation,
                Ax[i] - model.b[i]);
    }

    return std::max(
        0.0,
        violation);
}

double dual_violation(
    const CanonicalLP& model,
    const std::vector<double>& ATy)
{
    double violation = 0.0;

    for (int j = 0;
         j < model.variables;
         ++j)
    {
        const double value =
            model.c[j] +
            ATy[j];

        violation =
            std::max(
                violation,
                -value);
    }

    return std::max(
        0.0,
        violation);
}

std::vector<double> recover_values(
    const CanonicalLP& model,
    const std::vector<double>& z)
{
    std::vector<double> values =
        z;

    for (int j = 0;
         j < model.variables;
         ++j)
    {
        values[j] +=
            model.lower_bounds[j];
    }

    return values;
}

double original_objective(
    const Problem& problem,
    const std::vector<double>& values)
{
    return dot(
        problem.objective(),
        values);
}

bool validate_candidate(
    const Problem& problem,
    const std::vector<double>& values,
    double tolerance)
{
    if (
        values.size() !=
        problem.variables().size())
    {
        return false;
    }

    /*
     * Variable bounds.
     */
    for (std::size_t j = 0;
         j < values.size();
         ++j)
    {
        const auto& variable =
            problem.variables()[j];

        if (
            values[j] <
            variable.lower_bound -
                10.0 * tolerance)
        {
            return false;
        }

        if (
            std::isfinite(
                variable.upper_bound) &&
            variable.upper_bound > 0.0 &&
            values[j] >
                variable.upper_bound +
                    10.0 * tolerance)
        {
            return false;
        }
    }

    /*
     * Constraint feasibility.
     */
    for (std::size_t i = 0;
         i < problem.constraints().size();
         ++i)
    {
        double lhs = 0.0;

        for (std::size_t j = 0;
             j < values.size();
             ++j)
        {
            lhs +=
                problem.matrix()[i][j] *
                values[j];
        }

        const double rhs =
            problem.constraints()[i].rhs;

        const double allowed =
            10.0 * tolerance *
            std::max(
                1.0,
                std::abs(rhs));

        switch (
            problem.constraints()[i].sense)
        {
            case ConstraintSense::LessEqual:
                if (lhs > rhs + allowed)
                    return false;
                break;

            case ConstraintSense::GreaterEqual:
                if (lhs < rhs - allowed)
                    return false;
                break;

            case ConstraintSense::Equal:
                if (
                    std::abs(lhs - rhs) >
                    allowed)
                {
                    return false;
                }
                break;
        }
    }

    return true;
}

} // namespace

PDLPSolver::PDLPSolver(
    double tolerance,
    int max_iterations)
    : tolerance_(tolerance),
      max_iterations_(max_iterations)
{
}

void PDLPSolver::set_warm_start(
    const WarmStart& warm_start)
{
    warm_start_ =
        warm_start;
}

void PDLPSolver::clear_warm_start()
{
    warm_start_.clear();
}

bool PDLPSolver::has_warm_start() const
{
    return warm_start_.available;
}

SolveResult PDLPSolver::solve(
    const Problem& problem) const
{
    SolveResult result;

    try
    {
        if (problem.variables().empty())
        {
            result.status =
                SolveStatus::Unsupported;

            result.message =
                "PDLP requires at least one variable.";

            return result;
        }

        /*
         * Continuous LP only.
         */
        for (const auto& variable :
             problem.variables())
        {
            if (
                variable.type !=
                VariableType::Continuous)
            {
                result.status =
                    SolveStatus::Unsupported;

                result.message =
                    "PDLP supports continuous LPs only.";

                return result;
            }
        }

        /*
         * Linear objective only.
         */
        for (const auto& row :
             problem.quadratic_matrix())
        {
            for (double value : row)
            {
                if (
                    std::abs(value) >
                    tolerance_)
                {
                    result.status =
                        SolveStatus::Unsupported;

                    result.message =
                        "PDLP supports linear programs only.";

                    return result;
                }
            }
        }

        CanonicalLP model =
            canonicalize(
                problem,
                tolerance_);

        /*
         * Unconstrained problem.
         */
        if (model.rows == 0)
        {
            bool unbounded = false;

            for (double coefficient :
                 model.c)
            {
                if (
                    coefficient <
                    -tolerance_)
                {
                    unbounded = true;
                    break;
                }
            }

            if (unbounded)
            {
                result.status =
                    SolveStatus::Unbounded;

                result.message =
                    "PDLP detected an unbounded "
                    "unconstrained direction.";

                return result;
            }

            result.status =
                SolveStatus::Optimal;

            result.variable_values =
                model.lower_bounds;

            result.objective_value =
                original_objective(
                    problem,
                    result.variable_values);

            return result;
        }

        const double operator_norm =
            matrix_norm(model.A);

        if (
            operator_norm <=
            tolerance_)
        {
            result.status =
                SolveStatus::Unsupported;

            result.message =
                "PDLP received a numerically zero constraint matrix.";

            return result;
        }

        /*
         * Conservative PDHG step size.
         */
        const double step =
            SAFETY /
            operator_norm;

        const double tau =
            step;

        const double sigma =
            step;

        std::vector<double> x(
            model.variables,
            0.0);

        std::vector<double> y(
            model.rows,
            0.0);

        /*
         * Warm start.
         */
        if (
            warm_start_.available &&
            warm_start_.variable_values.size() ==
                static_cast<std::size_t>(
                    model.variables))
        {
            for (int j = 0;
                 j < model.variables;
                 ++j)
            {
                x[j] =
                    warm_start_
                        .variable_values[j]
                    -
                    model.lower_bounds[j];
            }

            project_nonnegative(x);

            result.warm_start_used =
                true;
        }

        std::vector<double> Ax;
        std::vector<double> ATy;

        /*
         * Track the best feasible primal point.
         */
        std::vector<double> best_x =
            x;

        double best_objective =
            std::numeric_limits<double>::infinity();

        double best_violation =
            std::numeric_limits<double>::infinity();

        int best_iteration = 0;

        /*
         * PDHG.
         */
        for (
            int iteration = 1;
            iteration <= max_iterations_;
            ++iteration)
        {
            /*
             * Dual ascent.
             */
            multiply_A(
                model,
                x,
                Ax);

            for (int i = 0;
                 i < model.rows;
                 ++i)
            {
                y[i] +=
                    sigma *
                    (
                        Ax[i] -
                        model.b[i]
                    );

                if (y[i] < 0.0)
                    y[i] = 0.0;
            }

            /*
             * Primal descent.
             */
            multiply_AT(
                model,
                y,
                ATy);

            for (int j = 0;
                 j < model.variables;
                 ++j)
            {
                x[j] -=
                    tau *
                    (
                        model.c[j] +
                        ATy[j]
                    );
            }

            project_nonnegative(x);

            /*
             * Diagnostics.
             */
            multiply_A(
                model,
                x,
                Ax);

            multiply_AT(
                model,
                y,
                ATy);

            const double p_violation =
                primal_violation(
                    model,
                    Ax);

            const double d_violation =
                dual_violation(
                    model,
                    ATy);

            const double primal =
                dot(
                    model.c,
                    x);

            const double dual =
                -dot(
                    model.b,
                    y);

            const double gap =
                std::abs(
                    primal -
                    dual);

            const double scale =
                std::max(
                    1.0,
                    std::max(
                        std::abs(primal),
                        std::abs(dual)));

            const double relative_gap =
                gap /
                scale;

            /*
             * Keep the best feasible primal point.
             */
            if (
                p_violation <
                best_violation
            )
            {
                best_violation =
                    p_violation;

                best_x =
                    x;

                best_objective =
                    primal;

                best_iteration =
                    iteration;
            }
            else if (
                p_violation <=
                10.0 * tolerance_ &&
                primal < best_objective
            )
            {
                best_x =
                    x;

                best_objective =
                    primal;

                best_iteration =
                    iteration;
            }

            /*
             * Strict PDLP convergence.
             */
            if (
                p_violation <= tolerance_ &&
                d_violation <= tolerance_ &&
                relative_gap <= tolerance_
            )
            {
                const auto values =
                    recover_values(
                        model,
                        x);

                if (
                    validate_candidate(
                        problem,
                        values,
                        tolerance_)
                )
                {
                    result.status =
                        SolveStatus::Optimal;

                    result.iterations =
                        iteration;

                    result.variable_values =
                        values;

                    result.objective_value =
                        original_objective(
                            problem,
                            values);

                    result.message =
                        "PDLP converged with verified "
                        "primal/dual feasibility.";

                    warm_start_.available =
                        true;

                    warm_start_.variable_values =
                        result.variable_values;

                    warm_start_.basis.clear();

                    warm_start_.rows =
                        model.rows;

                    warm_start_.columns =
                        model.variables;

                    warm_start_.source =
                        "pdlp";

                    return result;
                }
            }
        }

        /*
         * ---------------------------------------------------------
         * SAFE FALLBACK / POLISHING
         * ---------------------------------------------------------
         *
         * PDLP did not independently reach a verified optimum.
         *
         * Do not return the approximate point as OPTIMAL.
         *
         * Use DENT's exact LP solver to obtain a verified solution.
         *
         * This is deliberately marked in the result message so
         * benchmark results remain transparent.
         */
        SimplexSolver fallback(
            1e-9,
            10000);

        SolveResult polished =
            fallback.solve(
                problem);

        if (
            polished.status ==
            SolveStatus::Optimal
        )
        {
            if (
                validate_candidate(
                    problem,
                    polished.variable_values,
                    1e-8)
            )
            {
                result =
                    polished;

                result.warm_start_used =
                    false;

                result.warm_start_iterations =
                    0;

                result.message =
                    "PDLP did not reach a verified "
                    "first-order optimum; result polished "
                    "by DENT Primal Simplex.";

                /*
                 * Keep the PDLP iteration count visible.
                 */
                result.iterations =
                    max_iterations_;

                return result;
            }
        }

        /*
         * If the fallback cannot solve it either,
         * return PDLP's best candidate but NEVER call
         * it OPTIMAL.
         */
        result.status =
            SolveStatus::IterationLimit;

        result.iterations =
            max_iterations_;

        result.variable_values =
            recover_values(
                model,
                best_x);

        result.objective_value =
            original_objective(
                problem,
                result.variable_values);

        result.message =
            "PDLP reached the iteration limit without "
            "a verified optimum. Best primal iteration=" +
            std::to_string(
                best_iteration) +
            ".";

        return result;
    }
    catch (const std::exception& error)
    {
        result.status =
            SolveStatus::Unsupported;

        result.message =
            error.what();

        return result;
    }
}

} // namespace dent