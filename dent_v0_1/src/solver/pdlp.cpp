#include "dent/solver/pdlp.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace dent {

namespace {

constexpr double EPS = 1e-12;
constexpr double SAFETY = 0.90;
constexpr double MIN_SCALE = 1e-8;
constexpr double MAX_SCALE = 1e8;

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

struct PreconditionedLP
{
    int variables = 0;
    int rows = 0;

    std::vector<std::vector<double>> A;
    std::vector<double> b;
    std::vector<double> c;

    std::vector<double> row_scale;
    std::vector<double> column_scale;

    std::vector<double> lower_bounds;

    bool original_maximize = false;
};


double dot(
    const std::vector<double>& a,
    const std::vector<double>& b
)
{
    const std::size_t n =
        std::min(a.size(), b.size());

    double value = 0.0;

    for (std::size_t i = 0; i < n; ++i) {
        value +=
            a[i] * b[i];
    }

    return value;
}


double norm(
    const std::vector<double>& values
)
{
    double value = 0.0;

    for (double x : values) {
        value +=
            x * x;
    }

    return std::sqrt(value);
}


double frobenius_norm(
    const std::vector<std::vector<double>>& A
)
{
    double value = 0.0;

    for (const auto& row : A) {
        for (double x : row) {
            value +=
                x * x;
        }
    }

    return std::sqrt(value);
}


bool finite_upper_bound(
    const Variable& variable
)
{
    return
        variable.upper_bound != 0.0 &&
        std::isfinite(variable.upper_bound);
}


void project_nonnegative(
    std::vector<double>& values
)
{
    for (double& value : values) {
        if (value < 0.0) {
            value = 0.0;
        }
    }
}


CanonicalLP canonicalize(
    const Problem& problem,
    double tolerance
)
{
    CanonicalLP model;

    model.variables =
        static_cast<int>(
            problem.variables().size()
        );

    model.original_maximize =
        problem.objective_sense() ==
        ObjectiveSense::Maximize;

    model.c.resize(
        model.variables,
        0.0
    );

    model.lower_bounds.resize(
        model.variables,
        0.0
    );

    /*
        Internally PDLP works with:

            z >= 0

        using:

            x = z + lower_bound

        Therefore the lower bound is removed from the
        optimization variable and restored after solving.
    */
    for (int j = 0; j < model.variables; ++j) {
        const auto& variable =
            problem.variables()[j];

        if (
            variable.lower_bound <
            -tolerance
        ) {
            throw std::runtime_error(
                "PDLP currently requires "
                "nonnegative variable lower bounds."
            );
        }

        model.lower_bounds[j] =
            variable.lower_bound;
    }

    /*
        Convert the objective to minimization internally.

            maximize c^T x
            ->
            minimize (-c)^T x
    */
    for (int j = 0; j < model.variables; ++j) {
        model.c[j] =
            model.original_maximize
                ? -problem.objective()[j]
                : problem.objective()[j];
    }

    /*
        For:

            x = z + l

        a constraint:

            A x <= b

        becomes:

            A z <= b - A l
    */
    auto append_less_equal =
        [&](
            const std::vector<double>& row,
            double rhs
        )
        {
            double shifted_rhs =
                rhs;

            for (int j = 0;
                 j < model.variables;
                 ++j) {
                shifted_rhs -=
                    row[j] *
                    model.lower_bounds[j];
            }

            model.A.push_back(row);
            model.b.push_back(shifted_rhs);
        };


    /*
        Convert all constraints to <= form.
    */
    for (
        std::size_t i = 0;
        i < problem.constraints().size();
        ++i
    ) {
        const Constraint& constraint =
            problem.constraints()[i];

        std::vector<double> row(
            model.variables,
            0.0
        );

        for (int j = 0;
             j < model.variables;
             ++j) {
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
            for (double& value : row) {
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

            for (double& value : row) {
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
        Finite upper bounds.

            x_j <= u_j

        becomes:

            z_j <= u_j - l_j
    */
    for (int j = 0;
         j < model.variables;
         ++j) {

        const auto& variable =
            problem.variables()[j];

        if (
            finite_upper_bound(variable)
        ) {
            std::vector<double> row(
                model.variables,
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


    model.rows =
        static_cast<int>(
            model.A.size()
        );

    return model;
}


PreconditionedLP precondition(
    const CanonicalLP& original
)
{
    PreconditionedLP model;

    model.variables =
        original.variables;

    model.rows =
        original.rows;

    model.original_maximize =
        original.original_maximize;

    model.lower_bounds =
        original.lower_bounds;

    model.row_scale.assign(
        model.rows,
        1.0
    );

    model.column_scale.assign(
        model.variables,
        1.0
    );


    /*
        Lightweight Ruiz-style equilibration.

        Several passes are enough for the current CPU
        first-order implementation while keeping the
        preprocessing inexpensive.
    */
    for (int pass = 0;
         pass < 3;
         ++pass) {

        for (int i = 0;
             i < model.rows;
             ++i) {

            double row_norm = 0.0;

            for (int j = 0;
                 j < model.variables;
                 ++j) {

                const double value =
                    original.A[i][j] *
                    model.column_scale[j];

                row_norm +=
                    value *
                    value;
            }

            row_norm =
                std::sqrt(row_norm);

            if (row_norm > EPS) {
                const double factor =
                    1.0 /
                    std::max(
                        row_norm,
                        1.0
                    );

                model.row_scale[i] *=
                    factor;

                model.row_scale[i] =
                    std::clamp(
                        model.row_scale[i],
                        MIN_SCALE,
                        MAX_SCALE
                    );
            }
        }


        for (int j = 0;
             j < model.variables;
             ++j) {

            double column_norm = 0.0;

            for (int i = 0;
                 i < model.rows;
                 ++i) {

                const double value =
                    original.A[i][j] *
                    model.row_scale[i];

                column_norm +=
                    value *
                    value;
            }

            column_norm =
                std::sqrt(column_norm);

            if (column_norm > EPS) {
                const double factor =
                    1.0 /
                    std::max(
                        column_norm,
                        1.0
                    );

                model.column_scale[j] *=
                    factor;

                model.column_scale[j] =
                    std::clamp(
                        model.column_scale[j],
                        MIN_SCALE,
                        MAX_SCALE
                    );
            }
        }
    }


    model.A.assign(
        model.rows,
        std::vector<double>(
            model.variables,
            0.0
        )
    );

    model.b.assign(
        model.rows,
        0.0
    );

    model.c.assign(
        model.variables,
        0.0
    );


    /*
        x_preconditioned is z in:

            original x =
                lower_bound +
                column_scale * z

        Therefore:

            A_original x <= b

        becomes:

            D_r A D_c z <=
            D_r (b - A lower)
    */
    for (int i = 0;
         i < model.rows;
         ++i) {

        model.b[i] =
            model.row_scale[i] *
            original.b[i];

        for (int j = 0;
             j < model.variables;
             ++j) {

            model.A[i][j] =
                model.row_scale[i] *
                original.A[i][j] *
                model.column_scale[j];
        }
    }


    for (int j = 0;
         j < model.variables;
         ++j) {

        model.c[j] =
            original.c[j] *
            model.column_scale[j];
    }

    return model;
}


void multiply_A(
    const PreconditionedLP& model,
    const std::vector<double>& x,
    std::vector<double>& result
)
{
    result.assign(
        model.rows,
        0.0
    );

    for (int i = 0;
         i < model.rows;
         ++i) {

        result[i] =
            dot(
                model.A[i],
                x
            );
    }
}


void multiply_AT(
    const PreconditionedLP& model,
    const std::vector<double>& y,
    std::vector<double>& result
)
{
    result.assign(
        model.variables,
        0.0
    );

    for (int i = 0;
         i < model.rows;
         ++i) {

        if (
            std::abs(y[i]) <= EPS
        ) {
            continue;
        }

        for (int j = 0;
             j < model.variables;
             ++j) {

            result[j] +=
                model.A[i][j] *
                y[i];
        }
    }
}


double primal_violation(
    const PreconditionedLP& model,
    const std::vector<double>& Ax
)
{
    double maximum = 0.0;

    for (int i = 0;
         i < model.rows;
         ++i) {

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
    const PreconditionedLP& model,
    const std::vector<double>& ATy
)
{
    double maximum = 0.0;

    for (int j = 0;
         j < model.variables;
         ++j) {

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


double primal_objective(
    const PreconditionedLP& model,
    const std::vector<double>& x
)
{
    return dot(
        model.c,
        x
    );
}


double dual_objective(
    const PreconditionedLP& model,
    const std::vector<double>& y
)
{
    return -dot(
        model.b,
        y
    );
}


double duality_gap(
    double primal,
    double dual
)
{
    return std::max(
        0.0,
        primal - dual
    );
}


std::vector<double> recover_original_values(
    const PreconditionedLP& model,
    const std::vector<double>& z
)
{
    std::vector<double> values(
        model.variables,
        0.0
    );

    /*
        Correct coordinate recovery:

            x_original =
                lower_bound +
                D_column * z
    */
    for (int j = 0;
         j < model.variables;
         ++j) {

        values[j] =
            model.lower_bounds[j] +
            model.column_scale[j] *
            z[j];
    }

    return values;
}

} // namespace


PDLPSolver::PDLPSolver(
    double tolerance,
    int max_iterations
)
    : tolerance_(tolerance),
      max_iterations_(max_iterations)
{
}


void PDLPSolver::set_warm_start(
    const WarmStart& warm_start
)
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
                "PDLP requires at least one variable.";

            return result;
        }


        /*
            PDLP is an LP solver.

            Integer and binary variables are handled by
            MILP, not by PDLP.
        */
        for (
            const auto& variable :
            problem.variables()
        ) {
            if (
                variable.type !=
                VariableType::Continuous
            ) {
                result.status =
                    SolveStatus::Unsupported;

                result.message =
                    "PDLP supports continuous LPs only.";

                return result;
            }
        }


        /*
            Reject quadratic objectives.
        */
        for (
            const auto& row :
            problem.quadratic_matrix()
        ) {
            for (double value : row) {
                if (
                    std::abs(value) >
                    tolerance_
                ) {
                    result.status =
                        SolveStatus::Unsupported;

                    result.message =
                        "PDLP supports linear programs only.";

                    return result;
                }
            }
        }


        CanonicalLP original =
            canonicalize(
                problem,
                tolerance_
            );


        /*
            Unconstrained LP.
        */
        if (
            original.rows == 0
        ) {
            bool unbounded =
                false;

            for (double coefficient :
                 original.c) {

                if (
                    coefficient <
                    -tolerance_
                ) {
                    unbounded = true;
                    break;
                }
            }

            if (unbounded) {
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
                original.lower_bounds;

            result.objective_value =
                dot(
                    problem.objective(),
                    result.variable_values
                );

            result.message =
                "PDLP solved an unconstrained LP.";

            return result;
        }


        PreconditionedLP model =
            precondition(
                original
            );


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
                "PDLP received a numerically zero "
                "constraint matrix.";

            return result;
        }


        /*
            Initial balanced step sizes.
        */
        double tau =
            SAFETY /
            operator_norm;

        double sigma =
            SAFETY /
            operator_norm;


        std::vector<double> x(
            model.variables,
            0.0
        );

        std::vector<double> y(
            model.rows,
            0.0
        );


        /*
            Warm-start values are stored in ORIGINAL
            coordinates.

            Convert:

                original x =
                    lower +
                    D_column z

            therefore:

                z =
                    (x - lower) /
                    D_column
        */
        if (
            warm_start_.available &&
            warm_start_.variable_values.size() ==
                static_cast<std::size_t>(
                    model.variables
                )
        ) {

            for (int j = 0;
                 j < model.variables;
                 ++j) {

                x[j] =
                    (
                        warm_start_
                            .variable_values[j]
                        -
                        model.lower_bounds[j]
                    ) /
                    model.column_scale[j];
            }

            project_nonnegative(x);

            result.warm_start_used =
                true;
        }


        std::vector<double> x_bar =
            x;

        std::vector<double> Ax;
        std::vector<double> ATy;

        double previous_best_residual =
            std::numeric_limits<double>::infinity();

        int stagnant_iterations = 0;
        int stable_iterations = 0;

        constexpr int REQUIRED_STABLE =
            5;

        constexpr int STAGNATION_WINDOW =
            250;

        constexpr double theta =
            1.0;


        for (
            int iteration = 1;
            iteration <= max_iterations_;
            ++iteration
        ) {

            const std::vector<double> old_x =
                x;


            /*
                Dual ascent.
            */
            multiply_A(
                model,
                x_bar,
                Ax
            );

            for (int i = 0;
                 i < model.rows;
                 ++i) {

                y[i] +=
                    sigma *
                    (
                        Ax[i] -
                        model.b[i]
                    );
            }

            project_nonnegative(y);


            /*
                Primal descent.
            */
            multiply_AT(
                model,
                y,
                ATy
            );

            for (int j = 0;
                 j < model.variables;
                 ++j) {

                x[j] -=
                    tau *
                    (
                        model.c[j] +
                        ATy[j]
                    );
            }

            project_nonnegative(x);


            /*
                Extrapolation.
            */
            for (int j = 0;
                 j < model.variables;
                 ++j) {

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


            double step_squared = 0.0;

            for (int j = 0;
                 j < model.variables;
                 ++j) {

                const double difference =
                    x[j] -
                    old_x[j];

                step_squared +=
                    difference *
                    difference;
            }

            const double step =
                std::sqrt(
                    step_squared
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

            const double residual =
                std::max(
                    primal_residual,
                    dual_residual
                );


            /*
                Adaptive primal/dual balancing.
            */
            if (
                iteration > 10 &&
                iteration % 25 == 0
            ) {

                if (
                    primal_residual >
                    2.0 *
                    std::max(
                        dual_residual,
                        EPS
                    )
                ) {

                    tau =
                        std::min(
                            tau * 1.05,
                            2.0 /
                            operator_norm
                        );

                    sigma =
                        std::max(
                            sigma / 1.05,
                            0.05 /
                            operator_norm
                        );
                }
                else if (
                    dual_residual >
                    2.0 *
                    std::max(
                        primal_residual,
                        EPS
                    )
                ) {

                    sigma =
                        std::min(
                            sigma * 1.05,
                            2.0 /
                            operator_norm
                        );

                    tau =
                        std::max(
                            tau / 1.05,
                            0.05 /
                            operator_norm
                        );
                }
            }


            /*
                Stagnation detection.
            */
            if (
                residual >=
                previous_best_residual *
                (1.0 - 1e-5)
            ) {
                ++stagnant_iterations;
            }
            else {
                stagnant_iterations =
                    0;
            }

            previous_best_residual =
                std::min(
                    previous_best_residual,
                    residual
                );


            if (
                stagnant_iterations >=
                STAGNATION_WINDOW
            ) {

                x_bar =
                    x;

                tau *=
                    0.75;

                sigma *=
                    0.75;

                stagnant_iterations =
                    0;
            }


            /*
                Convergence requires primal feasibility,
                dual feasibility, and either a sufficiently
                small duality gap or a sufficiently small
                primal step.
            */
            const bool converged =
                primal_residual <= tolerance_ &&
                dual_residual <= tolerance_ &&
                (
                    scaled_gap <= tolerance_ ||
                    step <= tolerance_
                );


            if (converged) {
                ++stable_iterations;
            }
            else {
                stable_iterations =
                    0;
            }


            if (
                stable_iterations >=
                REQUIRED_STABLE
            ) {

                result.status =
                    SolveStatus::Optimal;

                result.iterations =
                    iteration;

                result.variable_values =
                    recover_original_values(
                        model,
                        x
                    );

                /*
                    Always calculate the reported objective
                    from the ORIGINAL model coordinates.
                */
                result.objective_value =
                    dot(
                        problem.objective(),
                        result.variable_values
                    );

                result.warm_start_iterations =
                    iteration;

                result.message =
                    "PDLP converged with preconditioning, "
                    "primal residual " +
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
                    Store warm start in ORIGINAL coordinates.
                */
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


        /*
            Iteration limit.
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
            recover_original_values(
                model,
                x
            );

        result.objective_value =
            dot(
                problem.objective(),
                result.variable_values
            );

        result.warm_start_iterations =
            max_iterations_;

        result.message =
            "PDLP reached the iteration limit. "
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