#include "dent/solver/pdhg.hpp"
#include "dent/solver/simplex.hpp"

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

    std::vector<std::vector<double>> A;
    std::vector<double> b;
    std::vector<double> c;

    bool original_maximize = false;
};

double dot(
    const std::vector<double>& a,
    const std::vector<double>& b)
{
    const std::size_t n =
        std::min(a.size(), b.size());

    double result = 0.0;

    for (std::size_t i = 0; i < n; ++i)
        result += a[i] * b[i];

    return result;
}

double norm(
    const std::vector<double>& values)
{
    double sum = 0.0;

    for (double value : values)
        sum += value * value;

    return std::sqrt(sum);
}

double frobenius_norm(
    const std::vector<std::vector<double>>& A)
{
    double sum = 0.0;

    for (const auto& row : A)
    {
        for (double value : row)
            sum += value * value;
    }

    return std::sqrt(sum);
}

bool finite_upper_bound(
    const Variable& variable)
{
    return
        variable.upper_bound != 0.0 &&
        std::isfinite(variable.upper_bound);
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

    for (int j = 0;
         j < model.variables;
         ++j)
    {
        model.c[j] =
            model.original_maximize
                ? -problem.objective()[j]
                : problem.objective()[j];
    }

    for (const auto& variable :
         problem.variables())
    {
        if (std::abs(variable.lower_bound) >
            tolerance)
        {
            throw std::runtime_error(
                "PDHG requires variable lower bounds of 0.");
        }
    }

    auto append_row =
        [&](const std::vector<double>& row,
            double rhs)
        {
            model.A.push_back(row);
            model.b.push_back(rhs);
        };

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

        if (constraint.sense ==
            ConstraintSense::LessEqual)
        {
            append_row(
                row,
                constraint.rhs);
        }
        else if (constraint.sense ==
                 ConstraintSense::GreaterEqual)
        {
            for (double& value : row)
                value = -value;

            append_row(
                row,
                -constraint.rhs);
        }
        else
        {
            append_row(
                row,
                constraint.rhs);

            for (double& value : row)
                value = -value;

            append_row(
                row,
                -constraint.rhs);
        }
    }

    for (int j = 0;
         j < model.variables;
         ++j)
    {
        const auto& variable =
            problem.variables()[j];

        if (finite_upper_bound(variable))
        {
            std::vector<double> row(
                model.variables,
                0.0);

            row[j] = 1.0;

            append_row(
                row,
                variable.upper_bound);
        }
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

void project_nonnegative(
    std::vector<double>& values)
{
    for (double& value : values)
    {
        if (value < 0.0)
            value = 0.0;
    }
}

double primal_violation(
    const CanonicalLP& model,
    const std::vector<double>& Ax)
{
    double maximum = 0.0;

    for (int i = 0;
         i < model.rows;
         ++i)
    {
        maximum =
            std::max(
                maximum,
                Ax[i] - model.b[i]);
    }

    return std::max(
        0.0,
        maximum);
}

double dual_violation(
    const CanonicalLP& model,
    const std::vector<double>& ATy)
{
    double maximum = 0.0;

    for (int j = 0;
         j < model.variables;
         ++j)
    {
        const double value =
            model.c[j] +
            ATy[j];

        maximum =
            std::max(
                maximum,
                -value);
    }

    return std::max(
        0.0,
        maximum);
}

double primal_objective(
    const CanonicalLP& model,
    const std::vector<double>& x)
{
    return dot(
        model.c,
        x);
}

double dual_objective(
    const CanonicalLP& model,
    const std::vector<double>& y)
{
    return -dot(
        model.b,
        y);
}

} // namespace


PDHGSolver::PDHGSolver(
    double tolerance,
    int max_iterations)
    : tolerance_(tolerance),
      max_iterations_(max_iterations)
{
}


void PDHGSolver::set_warm_start(
    const WarmStart& warm_start)
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
                "PDHG requires at least one variable.";

            return result;
        }

        CanonicalLP model =
            canonicalize(
                problem,
                tolerance_);

        if (model.rows == 0)
        {
            SimplexSolver simplex(
                tolerance_,
                max_iterations_);

            SolveResult fallback =
                simplex.solve(problem);

            fallback.message =
                "PDHG delegated unconstrained LP "
                "to DENT Primal Simplex.";

            return fallback;
        }

        const double operator_norm =
            frobenius_norm(model.A);

        if (operator_norm <= tolerance_)
        {
            SimplexSolver simplex(
                tolerance_,
                max_iterations_);

            SolveResult fallback =
                simplex.solve(problem);

            fallback.message =
                "PDHG received a numerically zero "
                "constraint matrix; result solved by "
                "DENT Primal Simplex.";

            return fallback;
        }

        /*
         * Conservative PDHG step sizes.
         */
        constexpr double SAFETY = 0.85;

        const double tau =
            SAFETY / operator_norm;

        const double sigma =
            SAFETY / operator_norm;

        std::vector<double> x(
            model.variables,
            0.0);

        std::vector<double> y(
            model.rows,
            0.0);

        /*
         * Reuse an existing PDHG primal warm start.
         */
        if (warm_start_.available &&
            warm_start_.variable_values.size() ==
                static_cast<std::size_t>(
                    model.variables))
        {
            x =
                warm_start_.variable_values;

            project_nonnegative(x);

            result.warm_start_used = true;
        }

        std::vector<double> x_bar =
            x;

        std::vector<double> Ax;
        std::vector<double> ATy;

        constexpr double theta = 1.0;

        int stable_iterations = 0;

        constexpr int REQUIRED_STABLE = 5;

        /*
         * First-order PDHG phase.
         */
        for (int iteration = 1;
             iteration <= max_iterations_;
             ++iteration)
        {
            const std::vector<double> old_x =
                x;

            /*
             * Dual update.
             */
            multiply_A(
                model,
                x_bar,
                Ax);

            for (int i = 0;
                 i < model.rows;
                 ++i)
            {
                y[i] +=
                    sigma *
                    (Ax[i] - model.b[i]);
            }

            project_nonnegative(y);

            /*
             * Primal update.
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
                    (model.c[j] + ATy[j]);
            }

            project_nonnegative(x);

            /*
             * Extrapolation.
             */
            for (int j = 0;
                 j < model.variables;
                 ++j)
            {
                x_bar[j] =
                    x[j] +
                    theta *
                    (x[j] - old_x[j]);
            }

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

            const double primal =
                primal_objective(
                    model,
                    x);

            const double dual =
                dual_objective(
                    model,
                    y);

            const double gap =
                std::max(
                    0.0,
                    primal - dual);

            const double primal_residual =
                primal_violation(
                    model,
                    Ax);

            const double dual_residual =
                dual_violation(
                    model,
                    ATy);

            std::vector<double> difference(
                model.variables,
                0.0);

            for (int j = 0;
                 j < model.variables;
                 ++j)
            {
                difference[j] =
                    x[j] - old_x[j];
            }

            const double step_norm =
                norm(difference);

            const double scale =
                std::max(
                    1.0,
                    std::max(
                        std::abs(primal),
                        std::abs(dual)));

            const double scaled_gap =
                gap / scale;

            const bool converged =
                primal_residual <= tolerance_ &&
                dual_residual <= tolerance_ &&
                (
                    scaled_gap <= tolerance_ ||
                    step_norm <= tolerance_
                );

            if (converged)
                ++stable_iterations;
            else
                stable_iterations = 0;

            if (stable_iterations >=
                REQUIRED_STABLE)
            {
                result.status =
                    SolveStatus::Optimal;

                result.iterations =
                    iteration;

                result.variable_values =
                    x;

                const double internal_objective =
                    primal_objective(
                        model,
                        x);

                result.objective_value =
                    model.original_maximize
                        ? -internal_objective
                        : internal_objective;

                result.message =
                    "PDHG converged with verified "
                    "primal/dual feasibility.";

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
         * PDHG did not independently verify convergence.
         *
         * Do not return its approximate result as OPTIMAL.
         *
         * Instead, use the existing DENT Simplex implementation
         * to produce a verified LP solution.
         */
        SimplexSolver simplex(
            tolerance_,
            max_iterations_);

        SolveResult fallback =
            simplex.solve(problem);

        if (fallback.status ==
            SolveStatus::Optimal)
        {
            fallback.message =
                "PDHG reached its iteration limit; "
                "result verified by DENT Primal Simplex.";

            return fallback;
        }

        /*
         * If even the fallback cannot solve the problem,
         * preserve that status rather than inventing success.
         */
        fallback.message =
            "PDHG reached its iteration limit and "
            "Primal Simplex fallback did not obtain "
            "an optimal solution.";

        return fallback;
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