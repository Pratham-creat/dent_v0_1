#include "dent/solver/simplex.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace dent {

namespace {

constexpr double EPS = 1e-9;
constexpr double INF =
    std::numeric_limits<double>::infinity();

struct SimplexTableau {

    std::vector<std::vector<double>> a;

    std::vector<int> basis;

    int rows = 0;
    int columns = 0;
};


/*
    ============================================================
    Basic tableau operations
    ============================================================
*/

void pivot(
    SimplexTableau& tab,
    int row,
    int column
)
{
    const double p =
        tab.a[row][column];

    if (std::abs(p) <= EPS) {

        throw std::runtime_error(
            "Simplex encountered a zero pivot."
        );
    }

    for (
        int j = 0;
        j <= tab.columns;
        ++j
    ) {

        tab.a[row][j] /= p;
    }

    for (
        int i = 0;
        i <= tab.rows;
        ++i
    ) {

        if (i == row) {
            continue;
        }

        const double factor =
            tab.a[i][column];

        if (std::abs(factor) <= EPS) {
            continue;
        }

        for (
            int j = 0;
            j <= tab.columns;
            ++j
        ) {

            if (j == column) {

                tab.a[i][j] =
                    0.0;

            } else {

                tab.a[i][j] -=
                    factor *
                    tab.a[row][j];
            }
        }
    }

    tab.basis[row] =
        column;
}


/*
    ============================================================
    Build initial tableau
    ============================================================
*/

SimplexTableau build_tableau(
    const Problem& problem
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

    for (
        const auto& variable :
        problem.variables()
    ) {

        if (
            std::abs(
                variable.lower_bound
            ) > EPS
        ) {

            throw std::runtime_error(
                "Simplex currently requires "
                "lower bounds of 0."
            );
        }

        if (
            std::isfinite(
                variable.upper_bound
            ) &&
            std::abs(
                variable.upper_bound
            ) > EPS
        ) {

            throw std::runtime_error(
                "Simplex requires finite upper "
                "bounds to be represented as constraints."
            );
        }
    }

    int extra =
        0;

    for (
        const auto& constraint :
        problem.constraints()
    ) {

        if (
            constraint.sense ==
            ConstraintSense::LessEqual
        ) {

            extra += 1;

        } else if (
            constraint.sense ==
            ConstraintSense::GreaterEqual
        ) {

            extra += 2;

        } else {

            extra += 1;
        }
    }

    SimplexTableau tab;

    tab.rows =
        m;

    tab.columns =
        n + extra;

    tab.a.assign(
        m + 1,
        std::vector<double>(
            tab.columns + 1,
            0.0
        )
    );

    tab.basis.assign(
        m,
        -1
    );

    int next_column =
        n;

    for (
        int i = 0;
        i < m;
        ++i
    ) {

        const auto& constraint =
            problem.constraints()[i];

        ConstraintSense sense =
            constraint.sense;

        double rhs =
            constraint.rhs;

        bool flipped =
            false;

        if (rhs < -EPS) {

            rhs =
                -rhs;

            flipped =
                true;

            if (
                sense ==
                ConstraintSense::LessEqual
            ) {

                sense =
                    ConstraintSense::GreaterEqual;

            } else if (
                sense ==
                ConstraintSense::GreaterEqual
            ) {

                sense =
                    ConstraintSense::LessEqual;
            }
        }

        for (
            int j = 0;
            j < n;
            ++j
        ) {

            double value =
                problem.matrix()[i][j];

            if (flipped) {
                value = -value;
            }

            tab.a[i][j] =
                value;
        }

        tab.a[i][tab.columns] =
            rhs;

        if (
            sense ==
            ConstraintSense::LessEqual
        ) {

            const int slack =
                next_column++;

            tab.a[i][slack] =
                1.0;

            tab.basis[i] =
                slack;

        } else if (
            sense ==
            ConstraintSense::GreaterEqual
        ) {

            const int surplus =
                next_column++;

            const int artificial =
                next_column++;

            tab.a[i][surplus] =
                -1.0;

            tab.a[i][artificial] =
                1.0;

            tab.basis[i] =
                artificial;

        } else {

            const int artificial =
                next_column++;

            tab.a[i][artificial] =
                1.0;

            tab.basis[i] =
                artificial;
        }
    }

    return tab;
}


/*
    ============================================================
    Objective
    ============================================================
*/

void set_objective(
    SimplexTableau& tab,
    const std::vector<double>& c
)
{
    for (
        int j = 0;
        j < tab.columns;
        ++j
    ) {

        tab.a[tab.rows][j] =
            0.0;
    }

    tab.a[tab.rows][tab.columns] =
        0.0;

    for (
        int j = 0;
        j < static_cast<int>(c.size());
        ++j
    ) {

        tab.a[tab.rows][j] =
            -c[j];
    }

    for (
        int i = 0;
        i < tab.rows;
        ++i
    ) {

        const int basic =
            tab.basis[i];

        if (
            basic < 0 ||
            basic >= tab.columns
        ) {
            continue;
        }

        const double cb =
            c[basic];

        if (std::abs(cb) <= EPS) {
            continue;
        }

        for (
            int j = 0;
            j <= tab.columns;
            ++j
        ) {

            tab.a[tab.rows][j] +=
                cb *
                tab.a[i][j];
        }
    }
}


/*
    ============================================================
    Primal Simplex iterations
    ============================================================
*/

SolveStatus simplex_iterations(
    SimplexTableau& tab,
    int max_iterations,
    int& iterations
)
{
    iterations =
        0;

    while (
        iterations <
        max_iterations
    ) {

        int entering =
            -1;

        for (
            int j = 0;
            j < tab.columns;
            ++j
        ) {

            if (
                tab.a[tab.rows][j]
                < -EPS
            ) {

                entering =
                    j;

                break;
            }
        }

        if (entering == -1) {

            return SolveStatus::Optimal;
        }

        int leaving =
            -1;

        double best_ratio =
            INF;

        for (
            int i = 0;
            i < tab.rows;
            ++i
        ) {

            const double coefficient =
                tab.a[i][entering];

            if (
                coefficient <= EPS
            ) {
                continue;
            }

            const double rhs =
                tab.a[i][tab.columns];

            const double ratio =
                rhs /
                coefficient;

            if (ratio < -EPS) {
                continue;
            }

            if (
                ratio <
                best_ratio - EPS
            ) {

                best_ratio =
                    ratio;

                leaving =
                    i;
            }
        }

        if (leaving == -1) {

            return SolveStatus::Unbounded;
        }

        pivot(
            tab,
            leaving,
            entering
        );

        ++iterations;
    }

    return SolveStatus::IterationLimit;
}


/*
    ============================================================
    Extract solution
    ============================================================
*/

std::vector<double> extract_values(
    const SimplexTableau& tab,
    int original_variables
)
{
    std::vector<double> values(
        original_variables,
        0.0
    );

    for (
        int i = 0;
        i < tab.rows;
        ++i
    ) {

        const int basic =
            tab.basis[i];

        if (
            basic >= 0 &&
            basic < original_variables
        ) {

            double value =
                tab.a[i][tab.columns];

            if (
                std::abs(value) <= EPS
            ) {
                value = 0.0;
            }

            values[basic] =
                value;
        }
    }

    return values;
}


/*
    ============================================================
    Artificial variables
    ============================================================
*/

std::vector<bool> find_artificial_columns(
    const Problem& problem
)
{
    const int n =
        static_cast<int>(
            problem.variables().size()
        );

    int extra =
        0;

    for (
        const auto& constraint :
        problem.constraints()
    ) {

        if (
            constraint.sense ==
            ConstraintSense::LessEqual
        ) {

            extra += 1;

        } else if (
            constraint.sense ==
            ConstraintSense::GreaterEqual
        ) {

            extra += 2;

        } else {

            extra += 1;
        }
    }

    std::vector<bool> artificial(
        n + extra,
        false
    );

    int next =
        n;

    for (
        const auto& constraint :
        problem.constraints()
    ) {

        if (
            constraint.sense ==
            ConstraintSense::LessEqual
        ) {

            next += 1;

        } else if (
            constraint.sense ==
            ConstraintSense::GreaterEqual
        ) {

            next += 1;

            artificial[next] =
                true;

            next += 1;

        } else {

            artificial[next] =
                true;

            next += 1;
        }
    }

    return artificial;
}


/*
    ============================================================
    Remove artificial variables
    ============================================================
*/

bool remove_artificial_columns(
    SimplexTableau& tab,
    const std::vector<bool>& artificial,
    std::string& error
)
{
    const int old_columns =
        tab.columns;

    for (
        int i = 0;
        i < tab.rows;
        ++i
    ) {

        const int basic =
            tab.basis[i];

        if (
            basic < 0 ||
            basic >= old_columns ||
            !artificial[basic]
        ) {
            continue;
        }

        if (
            std::abs(
                tab.a[i][old_columns]
            ) > EPS
        ) {

            error =
                "Artificial variable remains positive "
                "after Phase I.";

            return false;
        }

        int entering =
            -1;

        for (
            int j = 0;
            j < old_columns;
            ++j
        ) {

            if (artificial[j]) {
                continue;
            }

            if (
                std::abs(
                    tab.a[i][j]
                ) > EPS
            ) {

                entering =
                    j;

                break;
            }
        }

        if (entering != -1) {

            pivot(
                tab,
                i,
                entering
            );
        }
    }

    std::vector<int> map(
        old_columns,
        -1
    );

    int new_columns =
        0;

    for (
        int j = 0;
        j < old_columns;
        ++j
    ) {

        if (!artificial[j]) {

            map[j] =
                new_columns++;
        }
    }

    std::vector<int> kept_rows;

    for (
        int i = 0;
        i < tab.rows;
        ++i
    ) {

        const int basic =
            tab.basis[i];

        if (
            basic >= 0 &&
            basic < old_columns &&
            artificial[basic]
        ) {

            bool nonzero =
                false;

            for (
                int j = 0;
                j < old_columns;
                ++j
            ) {

                if (artificial[j]) {
                    continue;
                }

                if (
                    std::abs(
                        tab.a[i][j]
                    ) > EPS
                ) {

                    nonzero =
                        true;

                    break;
                }
            }

            if (!nonzero) {

                if (
                    std::abs(
                        tab.a[i][old_columns]
                    ) > EPS
                ) {

                    error =
                        "Infeasible constraint after Phase I.";

                    return false;
                }

                continue;
            }

            error =
                "Could not remove artificial variable.";

            return false;
        }

        kept_rows.push_back(i);
    }

    std::vector<std::vector<double>> new_data(
        kept_rows.size() + 1,
        std::vector<double>(
            new_columns + 1,
            0.0
        )
    );

    std::vector<int> new_basis;

    for (
        std::size_t new_row = 0;
        new_row < kept_rows.size();
        ++new_row
    ) {

        const int old_row =
            kept_rows[new_row];

        for (
            int old_column = 0;
            old_column < old_columns;
            ++old_column
        ) {

            const int mapped =
                map[old_column];

            if (mapped >= 0) {

                new_data[new_row][mapped] =
                    tab.a[old_row][old_column];
            }
        }

        new_data[new_row][new_columns] =
            tab.a[old_row][old_columns];

        const int old_basic =
            tab.basis[old_row];

        if (
            old_basic < 0 ||
            old_basic >= old_columns ||
            artificial[old_basic]
        ) {

            error =
                "Invalid basis after artificial cleanup.";

            return false;
        }

        new_basis.push_back(
            map[old_basic]
        );
    }

    tab.a =
        std::move(new_data);

    tab.basis =
        std::move(new_basis);

    tab.rows =
        static_cast<int>(
            tab.basis.size()
        );

    tab.columns =
        new_columns;

    return true;
}


/*
    ============================================================
    Warm-start helpers
    ============================================================
*/

bool basis_is_valid(
    const SimplexTableau& tab,
    const std::vector<int>& basis
)
{
    if (
        static_cast<int>(
            basis.size()
        ) != tab.rows
    ) {
        return false;
    }

    std::vector<bool> used(
        tab.columns,
        false
    );

    for (const int variable :
         basis)
    {
        if (
            variable < 0 ||
            variable >= tab.columns
        ) {
            return false;
        }

        if (used[variable]) {
            return false;
        }

        used[variable] =
            true;
    }

    return true;
}


/*
    Restore a previously saved basis.

    The current tableau starts with its default
    slack/artificial basis. We pivot until the
    requested basis is reproduced.
*/

bool restore_basis(
    SimplexTableau& tab,
    const std::vector<int>& desired_basis
)
{
    if (
        !basis_is_valid(
            tab,
            desired_basis
        )
    ) {
        return false;
    }

    for (
        int row = 0;
        row < tab.rows;
        ++row
    ) {

        if (
            tab.basis[row] ==
            desired_basis[row]
        ) {
            continue;
        }

        int target_column =
            desired_basis[row];

        int pivot_row =
            -1;

        for (
            int candidate = row;
            candidate < tab.rows;
            ++candidate
        ) {

            if (
                std::abs(
                    tab.a[candidate][target_column]
                ) > EPS
            ) {

                bool already_basic =
                    false;

                for (
                    int r = 0;
                    r < row;
                    ++r
                ) {

                    if (
                        tab.basis[r] ==
                        target_column
                    ) {

                        already_basic =
                            true;

                        break;
                    }
                }

                if (!already_basic) {

                    pivot_row =
                        candidate;

                    break;
                }
            }
        }

        if (pivot_row == -1) {
            return false;
        }

        if (pivot_row != row) {

            std::swap(
                tab.a[pivot_row],
                tab.a[row]
            );

            std::swap(
                tab.basis[pivot_row],
                tab.basis[row]
            );
        }

        pivot(
            tab,
            row,
            target_column
        );
    }

    for (
        int i = 0;
        i < tab.rows;
        ++i
    ) {

        if (
            tab.basis[i] !=
            desired_basis[i]
        ) {
            return false;
        }
    }

    return true;
}


bool is_primal_feasible(
    const SimplexTableau& tab
)
{
    for (
        int i = 0;
        i < tab.rows;
        ++i
    ) {

        if (
            tab.a[i][tab.columns]
            < -EPS
        ) {

            return false;
        }
    }

    return true;
}


void capture_warm_start(
    const Problem& problem,
    const SimplexTableau& tab,
    const std::vector<double>& values,
    WarmStart& warm_start
)
{
    warm_start.available =
        true;

    warm_start.variable_values =
        values;

    warm_start.basis =
        tab.basis;

    warm_start.rows =
        tab.rows;

    warm_start.columns =
        tab.columns;

    warm_start.structural_signature =
        structural_signature_for_problem(
            problem
        );

    warm_start.source =
        "simplex";
}

} // namespace


/*
    ============================================================
    Constructor
    ============================================================
*/

SimplexSolver::SimplexSolver(
    double tolerance,
    int max_iterations
)
    : tolerance_(tolerance),
      max_iterations_(max_iterations)
{
}


/*
    ============================================================
    Warm-start API
    ============================================================
*/

void SimplexSolver::set_warm_start(
    const WarmStart& warm_start
)
{
    warm_start_ =
        warm_start;
}


void SimplexSolver::clear_warm_start()
{
    warm_start_.clear();
}


bool SimplexSolver::has_warm_start() const
{
    return warm_start_.available;
}


/*
    ============================================================
    Solve
    ============================================================
*/

SolveResult SimplexSolver::solve(
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

        SimplexTableau tab =
            build_tableau(
                problem
            );

        const int original_variables =
            static_cast<int>(
                problem.variables().size()
            );

        int total_iterations =
            0;

        std::vector<bool> artificial =
            find_artificial_columns(
                problem
            );

        bool has_artificial =
            false;

        for (bool value :
             artificial)
        {
            if (value) {
                has_artificial = true;
                break;
            }
        }


        /*
            ----------------------------------------------------
            Warm-start attempt
            ----------------------------------------------------
        */

        bool warm_started =
            false;

        if (
            warm_start_.available &&
            warm_start_.matches_problem(
                problem
            ) &&
            warm_start_.rows ==
                tab.rows
        ) {

            /*
                Warm-start basis indices correspond
                directly only when the generated
                tableau has the same column layout.
            */

            if (
                warm_start_.columns ==
                tab.columns &&
                basis_is_valid(
                    tab,
                    warm_start_.basis
                )
            ) {

                if (
                    restore_basis(
                        tab,
                        warm_start_.basis
                    )
                ) {

                    /*
                        If the restored basis is
                        primal feasible, we can perform
                        normal primal reoptimization.
                    */

                    if (
                        is_primal_feasible(
                            tab
                        )
                    ) {

                        warm_started =
                            true;
                    }
                }
            }
        }


        /*
            ----------------------------------------------------
            Cold Phase I
            ----------------------------------------------------
        */

        if (!warm_started &&
            has_artificial)
        {

            std::vector<double> phase1_c(
                tab.columns,
                0.0
            );

            for (
                int j = 0;
                j < tab.columns;
                ++j
            ) {

                if (artificial[j]) {

                    phase1_c[j] =
                        -1.0;
                }
            }

            set_objective(
                tab,
                phase1_c
            );

            int phase1_iterations =
                0;

            SolveStatus phase1_status =
                simplex_iterations(
                    tab,
                    max_iterations_,
                    phase1_iterations
                );

            total_iterations +=
                phase1_iterations;

            if (
                phase1_status ==
                SolveStatus::IterationLimit
            ) {

                result.status =
                    SolveStatus::IterationLimit;

                result.iterations =
                    total_iterations;

                result.message =
                    "Phase I iteration limit reached.";

                return result;
            }

            if (
                phase1_status ==
                SolveStatus::Unbounded
            ) {

                result.status =
                    SolveStatus::Infeasible;

                result.iterations =
                    total_iterations;

                result.message =
                    "Phase I failed.";

                return result;
            }

            const double phase1_objective =
                tab.a[
                    tab.rows
                ][
                    tab.columns
                ];

            if (
                phase1_objective <
                -EPS
            ) {

                result.status =
                    SolveStatus::Infeasible;

                result.iterations =
                    total_iterations;

                result.message =
                    "Problem is infeasible.";

                return result;
            }

            std::string cleanup_error;

            if (
                !remove_artificial_columns(
                    tab,
                    artificial,
                    cleanup_error
                )
            ) {

                result.status =
                    SolveStatus::Infeasible;

                result.iterations =
                    total_iterations;

                result.message =
                    cleanup_error;

                return result;
            }
        }


        /*
            ----------------------------------------------------
            Phase II objective
            ----------------------------------------------------
        */

        std::vector<double> objective(
            tab.columns,
            0.0
        );

        for (
            int j = 0;
            j < original_variables;
            ++j
        ) {

            double coefficient =
                problem.objective()[j];

            if (
                problem.objective_sense() ==
                ObjectiveSense::Minimize
            ) {

                coefficient =
                    -coefficient;
            }

            objective[j] =
                coefficient;
        }

        set_objective(
            tab,
            objective
        );


        /*
            ----------------------------------------------------
            Phase II
            ----------------------------------------------------
        */

        const int remaining_iterations =
            std::max(
                1,
                max_iterations_ -
                    total_iterations
            );

        int phase2_iterations =
            0;

        SolveStatus phase2_status =
            simplex_iterations(
                tab,
                remaining_iterations,
                phase2_iterations
            );

        total_iterations +=
            phase2_iterations;

        result.status =
            phase2_status;

        result.iterations =
            total_iterations;

        result.variable_values =
            extract_values(
                tab,
                original_variables
            );

        double internal_objective =
            tab.a[
                tab.rows
            ][
                tab.columns
            ];

        if (
            problem.objective_sense() ==
            ObjectiveSense::Minimize
        ) {

            result.objective_value =
                -internal_objective;

        } else {

            result.objective_value =
                internal_objective;
        }


        /*
            ----------------------------------------------------
            Warm-start result
            ----------------------------------------------------
        */

        result.warm_start_used =
            warm_started;

        result.warm_start_iterations =
            warm_started
                ? phase2_iterations
                : 0;


        /*
            Save the basis after every optimal solve.
        */

        if (
            phase2_status ==
            SolveStatus::Optimal
        ) {

            capture_warm_start(
                problem,
                tab,
                result.variable_values,
                warm_start_
            );

            if (warm_started) {

                result.message =
                    "Optimal solution found using "
                    "Simplex warm-start reoptimization.";

            } else {

                result.message =
                    "Optimal solution found.";
            }

        } else if (
            phase2_status ==
            SolveStatus::Unbounded
        ) {

            result.message =
                "LP is unbounded.";

        } else if (
            phase2_status ==
            SolveStatus::IterationLimit
        ) {

            result.message =
                "Simplex iteration limit reached.";
        }

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