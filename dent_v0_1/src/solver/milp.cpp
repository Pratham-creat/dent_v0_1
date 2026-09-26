#include "dent/solver/milp.hpp"

#include "dent/solver/simplex.hpp"

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
constexpr int MAX_NODE_DIVE_DEPTH = 8;
constexpr int MAX_STRONG_BRANCH_CANDIDATES = 4;
constexpr int STRONG_BRANCH_ITERATIONS = 80;

bool is_branch_constraint(
    const Constraint& constraint
)
{
    return
        constraint.name.rfind(
            "__dent_branch_",
            0
        ) == 0;
}

bool is_finite_upper_bound(
    const Variable& variable
)
{
    return
        variable.upper_bound != 0.0 &&
        std::isfinite(
            variable.upper_bound
        );
}

double fractionality(
    double value
)
{
    const double lower =
        std::floor(value);

    const double upper =
        std::ceil(value);

    return std::min(
        value - lower,
        upper - value
    );
}

bool basis_shape_is_valid(
    const WarmStart& warm_start,
    int rows,
    int columns
)
{
    if (!warm_start.available) {
        return false;
    }

    if (
        warm_start.rows <= 0 ||
        warm_start.columns <= 0
    ) {
        return false;
    }

    if (
        rows <= warm_start.rows ||
        columns <= warm_start.columns
    ) {
        return false;
    }

    if (
        rows -
            warm_start.rows !=
        columns -
            warm_start.columns
    ) {
        return false;
    }

    if (
        static_cast<int>(
            warm_start.basis.size()
        ) != warm_start.rows
    ) {
        return false;
    }

    return true;
}

bool basis_indices_are_valid(
    const std::vector<int>& basis,
    int rows,
    int columns
)
{
    if (
        static_cast<int>(
            basis.size()
        ) != rows
    ) {
        return false;
    }

    std::vector<bool> seen(
        static_cast<std::size_t>(
            columns
        ),
        false
    );

    for (
        int column :
        basis
    ) {
        if (
            column < 0 ||
            column >= columns
        ) {
            return false;
        }

        if (
            seen[
                static_cast<std::size_t>(
                    column
                )
            ]
        ) {
            return false;
        }

        seen[
            static_cast<std::size_t>(
                column
            )
        ] = true;
    }

    return true;
}

/*
    Check whether a variable vector satisfies the
    LP relaxation represented by Problem.

    This is deliberately independent of the MILP
    integer restrictions because an LP node solution
    is allowed to be fractional.
*/
bool relaxation_point_is_feasible(
    const Problem& problem,
    const std::vector<double>& values,
    double tolerance
)
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
        if (
            values[j] <
            problem.variables()[j].lower_bound -
                tolerance
        ) {
            return false;
        }

        if (
            is_finite_upper_bound(
                problem.variables()[j]
            ) &&
            values[j] >
            problem.variables()[j].upper_bound +
                tolerance
        ) {
            return false;
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
                    tolerance
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
                    tolerance
            ) {
                return false;
            }
        }
        else {
            if (
                std::abs(
                    lhs -
                    constraint.rhs
                ) >
                tolerance
            ) {
                return false;
            }
        }
    }

    return true;
}

} // namespace


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


MILPSolver::MILPSolver(
    double tolerance,
    int max_nodes
)
    : tolerance_(tolerance),
      max_nodes_(max_nodes)
{
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
            is_finite_upper_bound(variable) &&
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
            is_finite_upper_bound(variable) &&
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
                values[j] <
                    -tolerance_ ||
                values[j] >
                    1.0 + tolerance_
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
                ) >
                tolerance_
            ) {
                return false;
            }
        }
    }

    return true;
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
        return
            candidate >
            incumbent +
                tolerance_;
    }

    return
        candidate <
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
        return
            bound >
            incumbent +
                tolerance_;
    }

    return
        bound <
        incumbent -
            tolerance_;
}


/*
    Build canonical LP relaxation.
*/
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

    auto copy_constraint =
        [&](const Constraint& source,
            int source_index)
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
                        [source_index][j];

                if (
                    std::abs(coefficient) >
                    tolerance_
                ) {
                    relaxation
                        .set_constraint_coefficient(
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
                        [source_index][j];

                if (
                    std::abs(coefficient) >
                    tolerance_
                ) {
                    relaxation
                        .set_constraint_coefficient(
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

        const bool negate =
            sense ==
            ConstraintSense::GreaterEqual;

        if (negate) {
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
                    [source_index][j];

            if (negate) {
                coefficient =
                    -coefficient;
            }

            if (
                std::abs(coefficient) >
                tolerance_
            ) {
                relaxation
                    .set_constraint_coefficient(
                        row,
                        static_cast<int>(j),
                        coefficient
                    );
            }
        }
    };

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

        copy_constraint(
            constraint,
            static_cast<int>(i)
        );
    }

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
            is_finite_upper_bound(variable)
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

        copy_constraint(
            constraint,
            static_cast<int>(i)
        );
    }

    return relaxation;
}


/*
    Solve a node LP.

    Safe warm-start policy:

        A parent basis may only be reused when the
        parent's primal solution is feasible for the
        child relaxation.

    A branch normally makes the parent fractional
    point infeasible. In that case the child is solved
    cold.

    This prevents an apparently valid basis from being
    used to produce a stale parent objective.
*/
MILPSolver::LPNodeResult
MILPSolver::solve_node_lp(
    const Problem& problem,
    const WarmStart& parent_warm_start,
    MILPSolution& result
) const
{
    LPNodeResult node_result;

    node_result.relaxation =
        build_lp_relaxation(problem);

    const int rows =
        static_cast<int>(
            node_result.relaxation
                .constraints()
                .size()
        );

    const int variables =
        static_cast<int>(
            node_result.relaxation
                .variables()
                .size()
        );

    const int columns =
        variables + rows;

    bool used_warm_start = false;

    /*
        We can only safely inherit the basis if the
        parent primal point remains feasible.

        WarmStart::variable_values is captured by the
        Simplex solver after a successful solve.
    */
    const bool parent_point_feasible =
        parent_warm_start.available &&
        !parent_warm_start.variable_values.empty() &&
        relaxation_point_is_feasible(
            node_result.relaxation,
            parent_warm_start.variable_values,
            tolerance_
        );

    if (
        parent_point_feasible &&
        basis_shape_is_valid(
            parent_warm_start,
            rows,
            columns
        )
    ) {
        std::vector<int> basis =
            parent_warm_start.basis;

        const int added_rows =
            rows -
            parent_warm_start.rows;

        for (
            int k = 0;
            k < added_rows;
            ++k
        ) {
            basis.push_back(
                parent_warm_start.columns +
                k
            );
        }

        if (
            basis_indices_are_valid(
                basis,
                rows,
                columns
            )
        ) {
            WarmStart child_start =
                make_basis_warm_start(
                    node_result.relaxation,
                    basis,
                    rows,
                    columns,
                    parent_warm_start.variable_values,
                    "milp-parent-basis"
                );

            SimplexSolver simplex(
                tolerance_,
                2000
            );

            simplex.set_warm_start(
                child_start
            );

            SolveResult warm_result =
                simplex.solve(
                    node_result.relaxation
                );

            if (
                warm_result.status ==
                SolveStatus::Optimal &&
                relaxation_point_is_feasible(
                    node_result.relaxation,
                    warm_result.variable_values,
                    tolerance_
                )
            ) {
                node_result.solve_result =
                    std::move(warm_result);

                node_result.warm_started =
                    true;

                ++result.warm_start_lp_solves;

                node_result.next_warm_start =
                    simplex.last_warm_start();

                used_warm_start =
                    true;
            }
        }
    }

    /*
        Cold solve.

        This is the authoritative fallback whenever
        the inherited basis cannot safely be reused.
    */
    if (!used_warm_start) {
        SimplexSolver simplex(
            tolerance_,
            2000
        );

        node_result.solve_result =
            simplex.solve(
                node_result.relaxation
            );

        node_result.warm_started =
            false;

        if (
            node_result.solve_result.status ==
            SolveStatus::Optimal
        ) {
            node_result.next_warm_start =
                simplex.last_warm_start();
        }
    }

    ++result.lp_solves;

    return node_result;
}


/*
    Simple rounding incumbent heuristic.
*/
bool MILPSolver::try_rounding_heuristic(
    const Problem& problem,
    const std::vector<double>& lp_values,
    std::vector<double>& integer_values
) const
{
    integer_values =
        lp_values;

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

        double value =
            std::round(
                integer_values[i]
            );

        if (
            variable.type ==
            VariableType::Binary
        ) {
            value =
                std::clamp(
                    value,
                    0.0,
                    1.0
                );
        }

        value =
            std::max(
                value,
                variable.lower_bound
            );

        if (
            is_finite_upper_bound(variable)
        ) {
            value =
                std::min(
                    value,
                    variable.upper_bound
                );
        }

        integer_values[i] =
            value;
    }

    return
        is_integral_solution(
            problem,
            integer_values
        ) &&
        check_feasibility(
            problem,
            integer_values
        );
}


/*
    Fractional diving heuristic.
*/
bool MILPSolver::try_diving_heuristic(
    const Problem& problem,
    const std::vector<double>& start_values,
    std::vector<double>& integer_values,
    MILPSolution& result
) const
{
    Problem diving_problem =
        problem;

    std::vector<double> current =
        start_values;

    for (
        int depth = 0;
        depth < MAX_NODE_DIVE_DEPTH;
        ++depth
    ) {
        ++result.heuristic_attempts;

        if (
            is_integral_solution(
                diving_problem,
                current
            ) &&
            check_feasibility(
                diving_problem,
                current
            )
        ) {
            integer_values =
                current;

            return true;
        }

        int selected =
            -1;

        double best =
            std::numeric_limits<double>::infinity();

        for (
            std::size_t i = 0;
            i < current.size();
            ++i
        ) {
            if (
                diving_problem
                    .variables()[i]
                    .type ==
                VariableType::Continuous
            ) {
                continue;
            }

            const double f =
                fractionality(
                    current[i]
                );

            if (
                f <= tolerance_
            ) {
                continue;
            }

            if (
                f < best
            ) {
                best = f;
                selected =
                    static_cast<int>(i);
            }
        }

        if (
            selected < 0
        ) {
            break;
        }

        const double value =
            current[selected];

        const double down =
            std::floor(value);

        const double up =
            std::ceil(value);

        Problem candidate =
            diving_problem;

        const double chosen =
            (
                value - down <=
                up - value
            )
                ? down
                : up;

        const int row =
            candidate.add_constraint(
                "__dent_dive_fix_" +
                    std::to_string(depth),
                ConstraintSense::Equal,
                chosen
            );

        candidate.set_constraint_coefficient(
            row,
            selected,
            1.0
        );

        SimplexSolver solver(
            tolerance_,
            500
        );

        SolveResult lp =
            solver.solve(
                build_lp_relaxation(
                    candidate
                )
            );

        ++result.lp_solves;

        if (
            lp.status !=
            SolveStatus::Optimal
        ) {
            return false;
        }

        current =
            lp.variable_values;

        diving_problem =
            std::move(candidate);
    }

    return false;
}


/*
    Cover separation.
*/
bool MILPSolver::add_cover_cuts(
    Problem& problem,
    const std::vector<double>& lp_values,
    MILPSolution& result
) const
{
    bool added =
        false;

    const std::size_t n =
        problem.variables().size();

    const std::size_t original_row_count =
        problem.constraints().size();

    for (
        std::size_t i = 0;
        i < original_row_count;
        ++i
    ) {
        const Constraint constraint =
            problem.constraints()[i];

        if (
            constraint.sense !=
            ConstraintSense::LessEqual
        ) {
            continue;
        }

        if (
            constraint.rhs <
            -tolerance_
        ) {
            continue;
        }

        struct Item {
            int index;
            double coefficient;
            double lp_value;
        };

        std::vector<Item> items;

        for (
            std::size_t j = 0;
            j < n;
            ++j
        ) {
            const auto& variable =
                problem.variables()[j];

            const double coefficient =
                problem.matrix()[i][j];

            if (
                variable.type ==
                    VariableType::Binary &&
                coefficient >
                    tolerance_
            ) {
                items.push_back(
                    {
                        static_cast<int>(j),
                        coefficient,
                        lp_values[j]
                    }
                );
            }
        }

        if (
            items.size() < 2
        ) {
            continue;
        }

        std::sort(
            items.begin(),
            items.end(),
            [](const Item& a,
               const Item& b)
            {
                return
                    a.coefficient >
                    b.coefficient;
            }
        );

        std::vector<Item> cover;

        double weight = 0.0;

        for (
            const auto& item :
            items
        ) {
            cover.push_back(item);

            weight +=
                item.coefficient;

            if (
                weight >
                constraint.rhs +
                    tolerance_
            ) {
                break;
            }
        }

        if (
            weight <=
            constraint.rhs +
                tolerance_
        ) {
            continue;
        }

        while (
            cover.size() > 1
        ) {
            const Item last =
                cover.back();

            if (
                weight -
                    last.coefficient >
                constraint.rhs +
                    tolerance_
            ) {
                weight -=
                    last.coefficient;

                cover.pop_back();
            }
            else {
                break;
            }
        }

        double lhs = 0.0;

        for (
            const auto& item :
            cover
        ) {
            lhs +=
                item.lp_value;
        }

        const double rhs =
            static_cast<double>(
                cover.size() - 1
            );

        ++result.cuts_generated;

        if (
            lhs <=
            rhs +
                tolerance_
        ) {
            continue;
        }

        const int cut =
            problem.add_constraint(
                "__dent_cover_" +
                    std::to_string(
                        result.cuts_added
                    ),
                ConstraintSense::LessEqual,
                rhs
            );

        for (
            const auto& item :
            cover
        ) {
            problem.set_constraint_coefficient(
                cut,
                item.index,
                1.0
            );
        }

        ++result.cuts_added;

        added = true;
    }

    return added;
}


std::vector<MILPSolver::BranchCandidate>
MILPSolver::build_branch_candidates(
    const Problem& problem,
    const std::vector<double>& values
) const
{
    std::vector<BranchCandidate>
        candidates;

    for (
        std::size_t i = 0;
        i < values.size();
        ++i
    ) {
        if (
            problem.variables()[i].type ==
            VariableType::Continuous
        ) {
            continue;
        }

        const double f =
            fractionality(
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
            return
                a.fractionality >
                b.fractionality;
        }
    );

    if (
        candidates.size() >
        MAX_STRONG_BRANCH_CANDIDATES
    ) {
        candidates.resize(
            MAX_STRONG_BRANCH_CANDIDATES
        );
    }

    return candidates;
}


Problem MILPSolver::make_branch_down(
    const Problem& problem,
    int variable,
    double value
) const
{
    Problem child =
        problem;

    const double floor_value =
        std::floor(value);

    const int row =
        child.add_constraint(
            "__dent_branch_down_" +
                std::to_string(variable) +
                "_" +
                std::to_string(
                    child.constraints().size()
                ),
            ConstraintSense::LessEqual,
            floor_value
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

    const double ceil_value =
        std::ceil(value);

    const int row =
        child.add_constraint(
            "__dent_branch_up_" +
                std::to_string(variable) +
                "_" +
                std::to_string(
                    child.constraints().size()
                ),
            ConstraintSense::GreaterEqual,
            ceil_value
        );

    child.set_constraint_coefficient(
        row,
        variable,
        1.0
    );

    return child;
}


/*
    Strong branching.
*/
MILPSolver::BranchCandidate
MILPSolver::choose_strong_branch(
    const Problem& problem,
    const std::vector<double>& values,
    const std::vector<BranchCandidate>& candidates,
    const WarmStart& parent_warm_start,
    double parent_bound,
    MILPSolution& result
) const
{
    if (
        candidates.empty()
    ) {
        return {};
    }

    BranchCandidate best =
        candidates.front();

    best.score =
        -std::numeric_limits<double>::infinity();

    for (
        auto candidate :
        candidates
    ) {
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

        MILPSolution probe_down_result;

        LPNodeResult down_lp =
            solve_node_lp(
                down,
                parent_warm_start,
                probe_down_result
            );

        ++result.strong_branching_solves;

        double down_gain = 0.0;

        if (
            down_lp.solve_result.status ==
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
                            down_lp.solve_result
                                .objective_value
                    );
            }
            else {
                down_gain =
                    std::max(
                        0.0,
                        down_lp.solve_result
                            .objective_value -
                            parent_bound
                    );
            }
        }
        else if (
            down_lp.solve_result.status ==
            SolveStatus::Infeasible
        ) {
            down_gain =
                std::numeric_limits<double>::infinity();
        }

        MILPSolution probe_up_result;

        LPNodeResult up_lp =
            solve_node_lp(
                up,
                parent_warm_start,
                probe_up_result
            );

        ++result.strong_branching_solves;

        double up_gain = 0.0;

        if (
            up_lp.solve_result.status ==
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
                            up_lp.solve_result
                                .objective_value
                    );
            }
            else {
                up_gain =
                    std::max(
                        0.0,
                        up_lp.solve_result
                            .objective_value -
                            parent_bound
                    );
            }
        }
        else if (
            up_lp.solve_result.status ==
            SolveStatus::Infeasible
        ) {
            up_gain =
                std::numeric_limits<double>::infinity();
        }

        candidate.down_gain =
            down_gain;

        candidate.up_gain =
            up_gain;

        if (
            std::isinf(down_gain) ||
            std::isinf(up_gain)
        ) {
            candidate.score =
                std::numeric_limits<double>::infinity();
        }
        else {
            candidate.score =
                std::min(
                    down_gain,
                    up_gain
                );
        }

        if (
            candidate.score >
            best.score
        ) {
            best =
                candidate;
        }
    }

    if (
        !std::isfinite(best.score)
    ) {
        for (
            const auto& candidate :
            candidates
        ) {
            if (
                candidate.fractionality >
                best.fractionality
            ) {
                best =
                    candidate;
            }
        }
    }

    return best;
}


void MILPSolver::branch_and_cut(
    const Problem& problem,
    int depth,
    const WarmStart& parent_warm_start,
    MILPSolution& result,
    bool& has_incumbent,
    double& incumbent_objective,
    std::vector<double>& incumbent_values
) const
{
    if (
        result.nodes_explored >=
        max_nodes_
    ) {
        return;
    }

    ++result.nodes_explored;

    LPNodeResult node =
        solve_node_lp(
            problem,
            parent_warm_start,
            result
        );

    if (
        node.solve_result.status ==
        SolveStatus::Infeasible
    ) {
        ++result.nodes_pruned;
        return;
    }

    if (
        node.solve_result.status !=
        SolveStatus::Optimal
    ) {
        ++result.nodes_pruned;
        return;
    }

    double node_bound =
        node.solve_result.objective_value;

    result.best_bound =
        node_bound;

    Problem strengthened =
        problem;

    WarmStart cut_warm_start =
        node.next_warm_start;

    SolveResult current_lp =
        node.solve_result;

    for (
        int round = 0;
        round < MAX_CUT_ROUNDS;
        ++round
    ) {
        if (
            is_integral_solution(
                strengthened,
                current_lp.variable_values
            )
        ) {
            break;
        }

        const int old_cuts =
            result.cuts_added;

        const bool generated =
            add_cover_cuts(
                strengthened,
                current_lp.variable_values,
                result
            );

        if (
            !generated ||
            result.cuts_added ==
                old_cuts
        ) {
            break;
        }

        LPNodeResult cut_node =
            solve_node_lp(
                strengthened,
                cut_warm_start,
                result
            );

        if (
            cut_node.solve_result.status ==
            SolveStatus::Infeasible
        ) {
            ++result.nodes_pruned;
            return;
        }

        if (
            cut_node.solve_result.status !=
            SolveStatus::Optimal
        ) {
            break;
        }

        current_lp =
            cut_node.solve_result;

        cut_warm_start =
            cut_node.next_warm_start;

        node_bound =
            current_lp.objective_value;

        result.best_bound =
            node_bound;
    }

    std::vector<double> heuristic_values;

    if (
        try_rounding_heuristic(
            strengthened,
            current_lp.variable_values,
            heuristic_values
        )
    ) {
        double objective = 0.0;

        for (
            std::size_t i = 0;
            i < heuristic_values.size();
            ++i
        ) {
            objective +=
                strengthened.objective()[i] *
                heuristic_values[i];
        }

        if (
            !has_incumbent ||
            better_objective(
                strengthened.objective_sense(),
                objective,
                incumbent_objective
            )
        ) {
            has_incumbent =
                true;

            incumbent_objective =
                objective;

            incumbent_values =
                heuristic_values;

            ++result.heuristic_incumbents;
        }
    }

    if (
        !is_integral_solution(
            strengthened,
            current_lp.variable_values
        )
    ) {
        std::vector<double> diving_values;

        if (
            try_diving_heuristic(
                strengthened,
                current_lp.variable_values,
                diving_values,
                result
            )
        ) {
            double objective = 0.0;

            for (
                std::size_t i = 0;
                i < diving_values.size();
                ++i
            ) {
                objective +=
                    strengthened.objective()[i] *
                    diving_values[i];
            }

            if (
                !has_incumbent ||
                better_objective(
                    strengthened.objective_sense(),
                    objective,
                    incumbent_objective
                )
            ) {
                has_incumbent =
                    true;

                incumbent_objective =
                    objective;

                incumbent_values =
                    diving_values;

                ++result.heuristic_incumbents;
            }
        }
    }

    if (
        !bound_can_improve(
            strengthened.objective_sense(),
            node_bound,
            has_incumbent,
            incumbent_objective
        )
    ) {
        ++result.nodes_pruned;
        return;
    }

    if (
        is_integral_solution(
            strengthened,
            current_lp.variable_values
        )
    ) {
        const double objective =
            current_lp.objective_value;

        if (
            !has_incumbent ||
            better_objective(
                strengthened.objective_sense(),
                objective,
                incumbent_objective
            )
        ) {
            has_incumbent =
                true;

            incumbent_objective =
                objective;

            incumbent_values =
                current_lp.variable_values;
        }

        return;
    }

    const auto candidates =
        build_branch_candidates(
            strengthened,
            current_lp.variable_values
        );

    if (
        candidates.empty()
    ) {
        ++result.nodes_pruned;
        return;
    }

    BranchCandidate selected =
        choose_strong_branch(
            strengthened,
            current_lp.variable_values,
            candidates,
            cut_warm_start,
            node_bound,
            result
        );

    if (
        selected.variable < 0
    ) {
        selected =
            candidates.front();
    }

    Problem left =
        make_branch_down(
            strengthened,
            selected.variable,
            selected.value
        );

    Problem right =
        make_branch_up(
            strengthened,
            selected.variable,
            selected.value
        );

    const double down_distance =
        selected.value -
        std::floor(
            selected.value
        );

    const double up_distance =
        std::ceil(
            selected.value
        ) -
        selected.value;

    if (
        down_distance <=
        up_distance
    ) {
        branch_and_cut(
            left,
            depth + 1,
            cut_warm_start,
            result,
            has_incumbent,
            incumbent_objective,
            incumbent_values
        );

        if (
            result.nodes_explored <
            max_nodes_
        ) {
            branch_and_cut(
                right,
                depth + 1,
                cut_warm_start,
                result,
                has_incumbent,
                incumbent_objective,
                incumbent_values
            );
        }
    }
    else {
        branch_and_cut(
            right,
            depth + 1,
            cut_warm_start,
            result,
            has_incumbent,
            incumbent_objective,
            incumbent_values
        );

        if (
            result.nodes_explored <
            max_nodes_
        ) {
            branch_and_cut(
                left,
                depth + 1,
                cut_warm_start,
                result,
                has_incumbent,
                incumbent_objective,
                incumbent_values
            );
        }
    }
}


MILPSolution MILPSolver::solve(
    const Problem& problem
) const
{
    MILPSolution result;

    if (
        !is_integer_problem(problem)
    ) {
        SimplexSolver simplex(
            tolerance_,
            2000
        );

        SolveResult lp =
            simplex.solve(problem);

        result.status =
            lp.status;

        result.objective_value =
            lp.objective_value;

        result.variable_values =
            lp.variable_values;

        result.nodes_explored =
            1;

        result.lp_solves =
            1;

        result.message =
            "No integer variables. "
            "Problem solved as LP.";

        return result;
    }

    bool has_incumbent =
        false;

    double incumbent_objective =
        0.0;

    std::vector<double> incumbent_values;

    branch_and_cut(
        problem,
        0,
        WarmStart{},
        result,
        has_incumbent,
        incumbent_objective,
        incumbent_values
    );

    if (
        has_incumbent
    ) {
        result.status =
            SolveStatus::Optimal;

        result.objective_value =
            incumbent_objective;

        result.variable_values =
            incumbent_values;

        if (
            result.nodes_explored >=
            max_nodes_
        ) {
            result.status =
                SolveStatus::IterationLimit;

            result.message =
                "MILP node limit reached. "
                "Best incumbent returned.";
        }
        else {
            result.message =
                "MILP solved using strengthened "
                "Branch-and-Cut with best-bound "
                "node management, incumbent heuristics, "
                "cover-cut separation, strong branching, "
                "and LP basis warm starts.";
        }
    }
    else if (
        result.nodes_explored >=
        max_nodes_
    ) {
        result.status =
            SolveStatus::IterationLimit;

        result.message =
            "MILP node limit reached before "
            "finding an integer solution.";
    }
    else {
        result.status =
            SolveStatus::Infeasible;

        result.message =
            "No feasible integer solution exists.";
    }

    return result;
}

} // namespace dent