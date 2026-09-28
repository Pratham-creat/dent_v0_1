#include "dent/solver/interior_point.hpp"

#include "dent/model/problem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace dent {

namespace {

constexpr double EPS = 1e-12;
constexpr double MIN_POSITIVE = 1e-14;
constexpr double REGULARIZATION = 1e-10;
constexpr double FRACTION_TO_BOUNDARY = 0.995;

struct StandardForm {
    int original_variables = 0;
    int standard_variables = 0;
    int rows = 0;

    std::vector<std::vector<double>> A;
    std::vector<double> b;
    std::vector<double> c;

    std::vector<double> lower_bounds;
    std::vector<double> upper_bounds;

    ObjectiveSense original_sense = ObjectiveSense::Minimize;
};

double dot_product(
    const std::vector<double>& a,
    const std::vector<double>& b
) {
    const std::size_t n = std::min(a.size(), b.size());

    double result = 0.0;

    for (std::size_t i = 0; i < n; ++i) {
        result += a[i] * b[i];
    }

    return result;
}

double vector_norm_inf(
    const std::vector<double>& values
) {
    double result = 0.0;

    for (double value : values) {
        result = std::max(result, std::abs(value));
    }

    return result;
}

bool is_finite_vector(
    const std::vector<double>& values
) {
    for (double value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }

    return true;
}

bool solve_dense_system(
    std::vector<std::vector<double>> A,
    std::vector<double> b,
    std::vector<double>& x
) {
    const int n =
        static_cast<int>(A.size());

    if (n == 0 ||
        static_cast<int>(b.size()) != n) {
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

        double pivot_value =
            std::abs(A[column][column]);

        for (int row = column + 1;
             row < n;
             ++row) {

            const double candidate =
                std::abs(A[row][column]);

            if (candidate > pivot_value) {
                pivot_value = candidate;
                pivot = row;
            }
        }

        if (pivot_value <= EPS) {
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

        const double diagonal =
            A[column][column];

        for (int row = column + 1;
             row < n;
             ++row) {

            const double factor =
                A[row][column] /
                diagonal;

            if (std::abs(factor) <= EPS) {
                continue;
            }

            A[row][column] = 0.0;

            for (int col = column + 1;
                 col < n;
                 ++col) {

                A[row][col] -=
                    factor *
                    A[column][col];
            }

            b[row] -=
                factor *
                b[column];
        }
    }

    x.assign(n, 0.0);

    for (int row = n - 1;
         row >= 0;
         --row) {

        double rhs = b[row];

        for (int col = row + 1;
             col < n;
             ++col) {

            rhs -=
                A[row][col] *
                x[col];
        }

        if (std::abs(A[row][row]) <= EPS) {
            return false;
        }

        x[row] =
            rhs /
            A[row][row];
    }

    return is_finite_vector(x);
}

/*
 * Convert the original LP to equality form:
 *
 *     A z = b
 *     z >= 0
 *
 * Every inequality receives its own slack variable.
 *
 * <= :
 *
 *     A x + s = b
 *
 * >= :
 *
 *    -A x + s = -b
 *
 * = :
 *
 *     A x = b
 */
bool convert_to_standard_form(
    const Problem& problem,
    StandardForm& standard,
    std::string& error
) {
    const int original_variables =
        static_cast<int>(
            problem.variables().size()
        );

    const int original_constraints =
        static_cast<int>(
            problem.constraints().size()
        );

    if (original_variables == 0) {
        error =
            "Interior-point solver received an empty model.";
        return false;
    }

    if (original_constraints == 0) {
        error =
            "Interior-point solver requires at least one constraint.";
        return false;
    }

    /*
     * IPM currently supports continuous variables only.
     */
    for (const auto& variable :
         problem.variables()) {

        if (variable.type !=
            VariableType::Continuous) {

            error =
                "Interior-point solver currently supports "
                "continuous variables only.";

            return false;
        }
    }

    standard.original_variables =
        original_variables;

    standard.original_sense =
        problem.objective_sense();

    /*
     * Count inequality constraints first.
     *
     * Each one needs a UNIQUE slack variable.
     */
    int slack_count = 0;

    for (const auto& constraint :
         problem.constraints()) {

        if (constraint.sense !=
            ConstraintSense::Equal) {

            ++slack_count;
        }
    }

    /*
     * Finite upper bounds also require one
     * additional slack variable each.
     */
    int upper_bound_count = 0;

    for (const auto& variable :
         problem.variables()) {

        if (variable.upper_bound != 0.0) {
            ++upper_bound_count;
        }
    }

    const int slack_variables =
        slack_count +
        upper_bound_count;

    const int standard_variables =
        original_variables +
        slack_variables;

    standard.standard_variables =
        standard_variables;

    standard.rows =
        original_constraints +
        upper_bound_count;

    standard.A.assign(
        standard.rows,
        std::vector<double>(
            standard_variables,
            0.0
        )
    );

    standard.b.assign(
        standard.rows,
        0.0
    );

    standard.c.assign(
        standard_variables,
        0.0
    );

    standard.lower_bounds.assign(
        original_variables,
        0.0
    );

    standard.upper_bounds.assign(
        original_variables,
        std::numeric_limits<double>::infinity()
    );

    /*
     * Convert the objective to minimization.
     *
     * Max c^T x
     *
     * becomes
     *
     * Min -c^T x
     */
    const double objective_sign =
        problem.objective_sense() ==
                ObjectiveSense::Maximize
            ? -1.0
            : 1.0;

    /*
     * Store variable bounds and objective.
     */
    for (int j = 0;
         j < original_variables;
         ++j) {

        const auto& variable =
            problem.variables()[j];

        if (!std::isfinite(
                variable.lower_bound)) {

            error =
                "Interior-point solver requires "
                "finite lower bounds.";

            return false;
        }

        if (variable.upper_bound != 0.0 &&
            variable.upper_bound <
                variable.lower_bound) {

            error =
                "Interior-point solver found "
                "an invalid variable bound.";

            return false;
        }

        standard.lower_bounds[j] =
            variable.lower_bound;

        if (variable.upper_bound != 0.0) {
            standard.upper_bounds[j] =
                variable.upper_bound;
        }

        standard.c[j] =
            objective_sign *
            problem.objective()[j];
    }

    /*
     * Shift variables:
     *
     * x = lower_bound + z
     *
     * Therefore z >= 0.
     */
    std::vector<double> shifted_rhs(
        original_constraints,
        0.0
    );

    for (int i = 0;
         i < original_constraints;
         ++i) {

        shifted_rhs[i] =
            problem.constraints()[i].rhs;

        for (int j = 0;
             j < original_variables;
             ++j) {

            shifted_rhs[i] -=
                problem.matrix()[i][j] *
                standard.lower_bounds[j];
        }
    }

    /*
     * Each inequality gets a different slack column.
     */
    int next_slack_column =
        original_variables;

    for (int i = 0;
         i < original_constraints;
         ++i) {

        const auto& constraint =
            problem.constraints()[i];

        for (int j = 0;
             j < original_variables;
             ++j) {

            double coefficient =
                problem.matrix()[i][j];

            if (constraint.sense ==
                ConstraintSense::GreaterEqual) {

                coefficient =
                    -coefficient;
            }

            standard.A[i][j] =
                coefficient;
        }

        double rhs =
            shifted_rhs[i];

        if (constraint.sense ==
            ConstraintSense::LessEqual) {

            /*
             * A z + s = b
             */
            standard.A[i][next_slack_column] =
                1.0;

            standard.b[i] =
                rhs;

            ++next_slack_column;
        }
        else if (
            constraint.sense ==
            ConstraintSense::GreaterEqual) {

            /*
             * -A z + s = -b
             */
            standard.A[i][next_slack_column] =
                1.0;

            standard.b[i] =
                -rhs;

            ++next_slack_column;
        }
        else {
            /*
             * Equality:
             *
             * A z = b
             */
            standard.b[i] =
                rhs;
        }
    }

    /*
     * Finite upper bounds.
     *
     * z_j <= upper-lower
     *
     * becomes:
     *
     * z_j + s = upper-lower
     */
    for (int j = 0;
         j < original_variables;
         ++j) {

        if (!std::isfinite(
                standard.upper_bounds[j])) {
            continue;
        }

        const int row =
            original_constraints +
            std::count_if(
                problem.variables().begin(),
                problem.variables().begin() + j,
                [](const Variable& variable) {
                    return variable.upper_bound != 0.0;
                }
            );

        standard.A[row][j] =
            1.0;

        standard.A[row][next_slack_column] =
            1.0;

        standard.b[row] =
            standard.upper_bounds[j] -
            standard.lower_bounds[j];

        ++next_slack_column;
    }

    /*
     * Objective is unaffected by slack variables.
     */
    for (int j = original_variables;
         j < standard_variables;
         ++j) {

        standard.c[j] =
            0.0;
    }

    return true;
}

std::vector<double> matrix_vector_product(
    const std::vector<std::vector<double>>& A,
    const std::vector<double>& x
) {
    std::vector<double> result(
        A.size(),
        0.0
    );

    for (std::size_t i = 0;
         i < A.size();
         ++i) {

        result[i] =
            dot_product(
                A[i],
                x
            );
    }

    return result;
}

std::vector<double> transpose_matrix_vector_product(
    const std::vector<std::vector<double>>& A,
    const std::vector<double>& y,
    int columns
) {
    std::vector<double> result(
        columns,
        0.0
    );

    for (std::size_t i = 0;
         i < A.size();
         ++i) {

        for (int j = 0;
             j < columns;
             ++j) {

            result[j] +=
                A[i][j] *
                y[i];
        }
    }

    return result;
}

double calculate_step_length(
    const std::vector<double>& values,
    const std::vector<double>& direction
) {
    double alpha = 1.0;

    for (std::size_t i = 0;
         i < values.size();
         ++i) {

        if (direction[i] < 0.0) {

            alpha =
                std::min(
                    alpha,
                    -values[i] /
                    direction[i]
                );
        }
    }

    alpha =
        std::min(
            1.0,
            FRACTION_TO_BOUNDARY *
                alpha
        );

    return std::max(
        0.0,
        alpha
    );
}

double complementarity(
    const std::vector<double>& z,
    const std::vector<double>& w
) {
    if (z.empty()) {
        return 0.0;
    }

    return
        dot_product(z, w) /
        static_cast<double>(z.size());
}

bool build_newton_direction(
    const StandardForm& standard,
    const std::vector<double>& z,
    const std::vector<double>& w,
    const std::vector<double>& y,
    const std::vector<double>& rp,
    const std::vector<double>& rd,
    const std::vector<double>& rc,
    std::vector<double>& dz,
    std::vector<double>& dy,
    std::vector<double>& dw
) {
    (void)y;

    const int m =
        standard.rows;

    const int n =
        standard.standard_variables;

    if (static_cast<int>(z.size()) != n ||
        static_cast<int>(w.size()) != n ||
        static_cast<int>(rp.size()) != m ||
        static_cast<int>(rd.size()) != n ||
        static_cast<int>(rc.size()) != n) {

        return false;
    }

    /*
     * Eliminate dz and dw from the KKT equations.
     *
     * D^-1 = Z W^-1
     */
    std::vector<double> inverse_D(
        n,
        0.0
    );

    std::vector<double> correction(
        n,
        0.0
    );

    for (int j = 0;
         j < n;
         ++j) {

        const double safe_z =
            std::max(
                z[j],
                MIN_POSITIVE
            );

        const double safe_w =
            std::max(
                w[j],
                MIN_POSITIVE
            );

        inverse_D[j] =
            safe_z /
            safe_w;

        correction[j] =
            rd[j] -
            rc[j] /
            safe_z;
    }

    /*
     * Normal equations:
     *
     * A D^-1 A^T dy =
     * -rp -
     * A D^-1 correction
     */
    std::vector<std::vector<double>> normal(
        m,
        std::vector<double>(
            m,
            0.0
        )
    );

    for (int row = 0;
         row < m;
         ++row) {

        for (int column = row;
             column < m;
             ++column) {

            double value = 0.0;

            for (int j = 0;
                 j < n;
                 ++j) {

                value +=
                    standard.A[row][j] *
                    inverse_D[j] *
                    standard.A[column][j];
            }

            normal[row][column] =
                value;

            normal[column][row] =
                value;
        }
    }

    /*
     * Numerical regularization.
     */
    for (int i = 0;
         i < m;
         ++i) {

        normal[i][i] +=
            REGULARIZATION;
    }

    std::vector<double> normal_rhs(
        m,
        0.0
    );

    for (int i = 0;
         i < m;
         ++i) {

        double value =
            -rp[i];

        for (int j = 0;
             j < n;
             ++j) {

            value -=
                standard.A[i][j] *
                inverse_D[j] *
                correction[j];
        }

        normal_rhs[i] =
            value;
    }

    if (!solve_dense_system(
            normal,
            normal_rhs,
            dy)) {

        return false;
    }

    /*
     * dz =
     *
     * D^-1(
     *      A^T dy
     *      + correction
     * )
     */
    const std::vector<double> Atdy =
        transpose_matrix_vector_product(
            standard.A,
            dy,
            n
        );

    dz.assign(
        n,
        0.0
    );

    for (int j = 0;
         j < n;
         ++j) {

        dz[j] =
            inverse_D[j] *
            (
                Atdy[j] +
                correction[j]
            );
    }

    /*
     * dw =
     *
     * -(rc + W dz) / z
     */
    dw.assign(
        n,
        0.0
    );

    for (int j = 0;
         j < n;
         ++j) {

        const double safe_z =
            std::max(
                z[j],
                MIN_POSITIVE
            );

        dw[j] =
            -(
                rc[j] +
                w[j] * dz[j]
            ) /
            safe_z;
    }

    return
        is_finite_vector(dz) &&
        is_finite_vector(dy) &&
        is_finite_vector(dw);
}

bool recover_solution(
    const Problem& problem,
    const StandardForm& standard,
    const std::vector<double>& z,
    std::vector<double>& solution
) {
    if (static_cast<int>(z.size()) <
        standard.original_variables) {

        return false;
    }

    solution.assign(
        standard.original_variables,
        0.0
    );

    for (int j = 0;
         j < standard.original_variables;
         ++j) {

        solution[j] =
            standard.lower_bounds[j] +
            z[j];

        if (std::abs(solution[j]) <
            1e-10) {

            solution[j] =
                0.0;
        }
    }

    (void)problem;

    return is_finite_vector(solution);
}

double calculate_objective(
    const Problem& problem,
    const std::vector<double>& solution
) {
    double objective =
        0.0;

    const int n =
        static_cast<int>(
            problem.objective().size()
        );

    for (int j = 0;
         j < n &&
         j < static_cast<int>(
                 solution.size());
         ++j) {

        objective +=
            problem.objective()[j] *
            solution[j];
    }

    return objective;
}

} // namespace

InteriorPointSolver::InteriorPointSolver(
    double tolerance,
    int max_iterations
)
    : tolerance_(tolerance),
      max_iterations_(max_iterations) {
}

void InteriorPointSolver::set_warm_start(
    const WarmStart& warm_start
) {
    warm_start_ =
        warm_start;
}

void InteriorPointSolver::clear_warm_start() {
    warm_start_.clear();
}

bool InteriorPointSolver::has_warm_start() const {
    return warm_start_.available;
}

SolveResult InteriorPointSolver::solve(
    const Problem& problem
) const {
    SolveResult result;

    StandardForm standard;

    std::string conversion_error;

    if (!convert_to_standard_form(
            problem,
            standard,
            conversion_error)) {

        result.status =
            SolveStatus::Unsupported;

        result.message =
            conversion_error;

        return result;
    }

    const int m =
        standard.rows;

    const int n =
        standard.standard_variables;

    /*
     * Strictly positive primal variables.
     */
    std::vector<double> z(
        n,
        1.0
    );

    /*
     * Strictly positive dual/slack variables.
     */
    std::vector<double> w(
        n,
        1.0
    );

    /*
     * Equality multipliers.
     */
    std::vector<double> y(
        m,
        0.0
    );

    double previous_gap =
        std::numeric_limits<double>::infinity();

    for (int iteration = 0;
         iteration < max_iterations_;
         ++iteration) {

        /*
         * A z
         */
        const std::vector<double> Az =
            matrix_vector_product(
                standard.A,
                z
            );

        /*
         * A^T y
         */
        const std::vector<double> Aty =
            transpose_matrix_vector_product(
                standard.A,
                y,
                n
            );

        /*
         * Primal residual:
         *
         * r_p = A z - b
         */
        std::vector<double> rp(
            m,
            0.0
        );

        for (int i = 0;
             i < m;
             ++i) {

            rp[i] =
                Az[i] -
                standard.b[i];
        }

        /*
         * Dual residual:
         *
         * r_d =
         * A^T y + w - c
         */
        std::vector<double> rd(
            n,
            0.0
        );

        for (int j = 0;
             j < n;
             ++j) {

            rd[j] =
                Aty[j] +
                w[j] -
                standard.c[j];
        }

        const double primal_error =
            vector_norm_inf(rp);

        const double dual_error =
            vector_norm_inf(rd);

        const double mu =
            complementarity(
                z,
                w
            );

        /*
         * Optimality test.
         */
        if (primal_error <= tolerance_ &&
            dual_error <= tolerance_ &&
            mu <= tolerance_) {

            std::vector<double> solution;

            if (!recover_solution(
                    problem,
                    standard,
                    z,
                    solution)) {

                result.status =
                    SolveStatus::IterationLimit;

                result.iterations =
                    iteration;

                result.message =
                    "Interior-point solver converged "
                    "but failed to recover the solution.";

                return result;
            }

            result.status =
                SolveStatus::Optimal;

            result.variable_values =
                solution;

            result.objective_value =
                calculate_objective(
                    problem,
                    solution
                );

            result.iterations =
                iteration;

            result.warm_start_used =
                false;

            result.warm_start_iterations =
                0;

            result.message =
                "LP solved using primal-dual "
                "interior-point method.";

            return result;
        }

        /*
         * ---------------------------------------------------------
         * Mehrotra predictor step
         * ---------------------------------------------------------
         *
         * Affine complementarity:
         *
         *     ZW e
         *
         * corresponds to sigma = 0.
         */
        std::vector<double> rc_aff(
            n,
            0.0
        );

        for (int j = 0;
             j < n;
             ++j) {

            rc_aff[j] =
                z[j] *
                w[j];
        }

        std::vector<double> dz_aff;
        std::vector<double> dy_aff;
        std::vector<double> dw_aff;

        if (!build_newton_direction(
                standard,
                z,
                w,
                y,
                rp,
                rd,
                rc_aff,
                dz_aff,
                dy_aff,
                dw_aff)) {

            result.status =
                SolveStatus::IterationLimit;

            result.iterations =
                iteration;

            result.message =
                "Interior-point solver failed "
                "during predictor step.";

            return result;
        }

        const double alpha_aff_primal =
            calculate_step_length(
                z,
                dz_aff
            );

        const double alpha_aff_dual =
            calculate_step_length(
                w,
                dw_aff
            );

        const double alpha_aff =
            std::min(
                alpha_aff_primal,
                alpha_aff_dual
            );

        double mu_aff =
            0.0;

        for (int j = 0;
             j < n;
             ++j) {

            const double z_aff =
                z[j] +
                alpha_aff *
                dz_aff[j];

            const double w_aff =
                w[j] +
                alpha_aff *
                dw_aff[j];

            mu_aff +=
                z_aff *
                w_aff;
        }

        mu_aff /=
            static_cast<double>(n);

        /*
         * Centering parameter:
         *
         * sigma =
         *     (mu_aff / mu)^3
         */
        double sigma =
            0.0;

        if (mu > EPS) {

            const double ratio =
                std::max(
                    0.0,
                    std::min(
                        1.0,
                        mu_aff / mu
                    )
                );

            sigma =
                std::pow(
                    ratio,
                    3.0
                );
        }

        /*
         * ---------------------------------------------------------
         * Corrector step
         * ---------------------------------------------------------
         */
        std::vector<double> rc(
            n,
            0.0
        );

        for (int j = 0;
             j < n;
             ++j) {

            rc[j] =
                z[j] *
                w[j]
                +
                dz_aff[j] *
                dw_aff[j]
                -
                sigma *
                mu;
        }

        std::vector<double> dz;
        std::vector<double> dy;
        std::vector<double> dw;

        if (!build_newton_direction(
                standard,
                z,
                w,
                y,
                rp,
                rd,
                rc,
                dz,
                dy,
                dw)) {

            result.status =
                SolveStatus::IterationLimit;

            result.iterations =
                iteration;

            result.message =
                "Interior-point solver failed "
                "during corrector step.";

            return result;
        }

        /*
         * Step length.
         */
        const double alpha_primal =
            calculate_step_length(
                z,
                dz
            );

        const double alpha_dual =
            calculate_step_length(
                w,
                dw
            );

        const double alpha =
            std::min(
                alpha_primal,
                alpha_dual
            );

        if (alpha <= 0.0 ||
            !std::isfinite(alpha)) {

            result.status =
                SolveStatus::IterationLimit;

            result.iterations =
                iteration;

            result.message =
                "Interior-point solver produced "
                "an invalid step length.";

            return result;
        }

        /*
         * Update primal variables.
         */
        for (int j = 0;
             j < n;
             ++j) {

            z[j] +=
                alpha *
                dz[j];

            w[j] +=
                alpha *
                dw[j];

            z[j] =
                std::max(
                    z[j],
                    MIN_POSITIVE
                );

            w[j] =
                std::max(
                    w[j],
                    MIN_POSITIVE
                );
        }

        /*
         * Update equality multipliers.
         */
        for (int i = 0;
             i < m;
             ++i) {

            y[i] +=
                alpha *
                dy[i];
        }

        if (!is_finite_vector(z) ||
            !is_finite_vector(w) ||
            !is_finite_vector(y)) {

            result.status =
                SolveStatus::IterationLimit;

            result.iterations =
                iteration + 1;

            result.message =
                "Interior-point solver encountered "
                "non-finite numerical values.";

            return result;
        }

        /*
         * Numerical divergence protection.
         */
        const double current_gap =
            complementarity(
                z,
                w
            );

        if (std::isfinite(previous_gap) &&
            current_gap >
                previous_gap * 1.0e6) {

            result.status =
                SolveStatus::IterationLimit;

            result.iterations =
                iteration + 1;

            result.message =
                "Interior-point solver detected "
                "numerical divergence.";

            return result;
        }

        previous_gap =
            current_gap;
    }

    result.status =
        SolveStatus::IterationLimit;

    result.iterations =
        max_iterations_;

    result.message =
        "Interior-point solver reached "
        "the iteration limit.";

    return result;
}

} // namespace dent