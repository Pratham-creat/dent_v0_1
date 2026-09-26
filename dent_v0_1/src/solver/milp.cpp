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

constexpr int MAX_DIVE_DEPTH = 8;

constexpr int MAX_STRONG_BRANCH_CANDIDATES = 4;

constexpr int STRONG_BRANCH_MAX_ITERATIONS = 80;


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
                nearest <
                    -tolerance_ ||
                nearest >
                    1.0 +
                    tolerance_
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
                values[j] <
                    -tolerance_ ||
                values[j] >
                    1.0 +
                    tolerance_
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


Problem MILPSolver::build_lp_relaxation(
    const Problem& original
) const
{
    Problem relaxation(
        original.objective_sense()
    );

    /*
        The current simplex implementation uses
        x >= 0 as its canonical variable domain.

        MILP variables therefore need zero lower bounds
        at this stage.
    */
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
        [&](
            const Constraint& source,
            int source_row
        )
        {
            /*
                Equality:

                    a*x = b

                becomes

                    a*x <= b
                    -a*x <= -b
            */
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
                            [source_row][j];

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

            bool negate =
                false;

            if (
                sense ==
                ConstraintSense::GreaterEqual
            ) {
                negate = true;
                sense =
                    ConstraintSense::LessEqual;
                rhs = -rhs;
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
                    relaxation
                        .set_constraint_coefficient(
                            row,
                            static_cast<int>(j),
                            coefficient
                        );
                }
            }
        };


    /*
        Original constraints first.
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
        Branch constraints are appended last.

        This keeps the node model construction
        deterministic.
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
    MILPSolution& result
) const
{
    LPNodeResult node;

    node.relaxation =
        build_lp_relaxation(
            problem
        );


    /*
        IMPORTANT:

        Do not pass a parent simplex basis here.

        A MILP child changes the constraint matrix by
        adding a branch row. The current Simplex
        WarmStart representation is tied to the exact
        structural signature of a problem.

        Reusing that basis here was the source of the
        Windows heap corruption.

        A dedicated row-addition reoptimization
        interface will be implemented later.
    */
    SimplexSolver simplex(
        tolerance_,
        2000
    );


    node.solve_result =
        simplex.solve(
            node.relaxation
        );


    ++result.lp_solves;


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
    }


    return
        check_feasibility(
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
    Problem diving_problem =
        problem;

    std::vector<double> values =
        start_values;


    for (
        int depth = 0;
        depth < MAX_DIVE_DEPTH;
        ++depth
    ) {
        ++result.heuristic_attempts;


        int branch_variable =
            -1;

        double best_fractionality =
            0.0;


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
                f >
                best_fractionality +
                    tolerance_
            ) {
                best_fractionality =
                    f;

                branch_variable =
                    static_cast<int>(i);
            }
        }


        if (
            branch_variable < 0
        ) {
            if (
                check_feasibility(
                    problem,
                    values
                )
            ) {
                integer_values =
                    values;

                return true;
            }

            return false;
        }


        const double value =
            values[
                static_cast<std::size_t>(
                    branch_variable
                )
            ];


        const double nearest =
            std::round(value);


        const int row =
            diving_problem.add_constraint(
                "__dent_dive_" +
                    std::to_string(depth),
                ConstraintSense::Equal,
                nearest
            );


        diving_problem
            .set_constraint_coefficient(
                row,
                branch_variable,
                1.0
            );


        LPNodeResult lp =
            solve_node_lp(
                diving_problem,
                result
            );


        if (
            lp.solve_result.status !=
            SolveStatus::Optimal
        ) {
            return false;
        }


        values =
            lp.solve_result.variable_values;


        if (
            is_integral_solution(
                diving_problem,
                values
            ) &&
            check_feasibility(
                problem,
                values
            )
        ) {
            integer_values =
                values;

            return true;
        }
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


    /*
        Only inspect constraints that existed when
        this separation round started.

        This prevents references from becoming invalid
        when new constraints are appended.
    */
    const std::size_t original_rows =
        problem.constraints().size();


    for (
        std::size_t i = 0;
        i < original_rows;
        ++i
    ) {
        /*
            Copy the constraint.

            add_constraint() may reallocate the
            underlying vector.
        */
        const Constraint constraint =
            problem.constraints()[i];


        if (
            constraint.sense !=
            ConstraintSense::LessEqual
        ) {
            continue;
        }


        struct Item
        {
            int index;
            double coefficient;
            double lp_value;
        };


        std::vector<Item> items;


        for (
            std::size_t j = 0;
            j < problem.variables().size();
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
            [](
                const Item& a,
                const Item& b
            )
            {
                return
                    a.coefficient >
                    b.coefficient;
            }
        );


        std::vector<Item> cover;

        double weight =
            0.0;


        for (
            const auto& item :
            items
        ) {
            cover.push_back(
                item
            );

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


        /*
            Remove redundant final items while
            the remaining cover is still a cover.
        */
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


        double lhs =
            0.0;


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
            f <=
            tolerance_
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
        [](
            const BranchCandidate& a,
            const BranchCandidate& b
        )
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


MILPSolver::BranchCandidate
MILPSolver::choose_strong_branch(
    const Problem& problem,
    const std::vector<BranchCandidate>& candidates,
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


        MILPSolution down_stats;

        LPNodeResult down_lp =
            solve_node_lp(
                down,
                down_stats
            );


        ++result.strong_branching_solves;


        double down_gain =
            0.0;


        if (
            down_lp.solve_result.status ==
            SolveStatus::Infeasible
        ) {
            down_gain =
                std::numeric_limits<double>::infinity();
        }
        else if (
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


        MILPSolution up_stats;

        LPNodeResult up_lp =
            solve_node_lp(
                up,
                up_stats
            );


        ++result.strong_branching_solves;


        double up_gain =
            0.0;


        if (
            up_lp.solve_result.status ==
            SolveStatus::Infeasible
        ) {
            up_gain =
                std::numeric_limits<double>::infinity();
        }
        else if (
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


    return best;
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
    if (
        !has_incumbent
    ) {
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
        return
            std::max(
                0.0,
                (bound - incumbent) /
                    denominator
            );
    }


    return
        std::max(
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


    /*
        Continuous problem.
    */
    if (
        !is_integer_problem(
            problem
        )
    ) {
        SimplexSolver simplex(
            tolerance_,
            2000
        );


        SolveResult lp =
            simplex.solve(
                problem
            );


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


    using Queue =
        std::priority_queue<
            Node,
            std::vector<Node>,
            NodeCompare
        >;


    Queue open_nodes(
        NodeCompare{
            problem.objective_sense()
        }
    );


    std::size_t sequence =
        0;


    /*
        Root has an infinite bound before its LP
        relaxation is solved.
    */
    const double initial_bound =
        problem.objective_sense() ==
            ObjectiveSense::Maximize
        ? std::numeric_limits<double>::infinity()
        : -std::numeric_limits<double>::infinity();


    open_nodes.push(
        Node{
            problem,
            initial_bound,
            0,
            sequence++
        }
    );


    bool has_incumbent =
        false;


    double incumbent_objective =
        0.0;


    std::vector<double>
        incumbent_values;


    double global_best_bound =
        initial_bound;


    /*
        Actual best-bound Branch-and-Cut loop.
    */
    while (
        !open_nodes.empty() &&
        result.nodes_explored < max_nodes_
    ) {
        Node node =
            open_nodes.top();

        open_nodes.pop();


        ++result.nodes_explored;


        LPNodeResult lp =
            solve_node_lp(
                node.problem,
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


        Problem strengthened =
            node.problem;


        SolveResult current_lp =
            lp.solve_result;


        double node_bound =
            current_lp.objective_value;


        /*
            Cover-cut separation.
        */
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


            const int cuts_before =
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
                    cuts_before
            ) {
                break;
            }


            LPNodeResult cut_lp =
                solve_node_lp(
                    strengthened,
                    result
                );


            if (
                cut_lp.solve_result.status ==
                SolveStatus::Infeasible
            ) {
                ++result.nodes_pruned;
                current_lp.status =
                    SolveStatus::Infeasible;
                break;
            }


            if (
                cut_lp.solve_result.status !=
                SolveStatus::Optimal
            ) {
                break;
            }


            current_lp =
                cut_lp.solve_result;


            node_bound =
                current_lp.objective_value;
        }


        if (
            current_lp.status ==
            SolveStatus::Infeasible
        ) {
            continue;
        }


        /*
            Rounding heuristic.
        */
        ++result.heuristic_attempts;


        std::vector<double>
            rounded_values;


        if (
            try_rounding_heuristic(
                strengthened,
                current_lp.variable_values,
                rounded_values
            )
        ) {
            double objective =
                0.0;


            for (
                std::size_t i = 0;
                i < rounded_values.size();
                ++i
            ) {
                objective +=
                    strengthened.objective()[i] *
                    rounded_values[i];
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
                    rounded_values;

                ++result.heuristic_incumbents;
            }
        }


        /*
            Diving heuristic.
        */
        if (
            !is_integral_solution(
                strengthened,
                current_lp.variable_values
            )
        ) {
            std::vector<double>
                diving_values;


            if (
                try_diving_heuristic(
                    strengthened,
                    current_lp.variable_values,
                    diving_values,
                    result
                )
            ) {
                double objective =
                    0.0;


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


        /*
            Incumbent pruning.
        */
        if (
            !bound_can_improve(
                strengthened.objective_sense(),
                node_bound,
                has_incumbent,
                incumbent_objective
            )
        ) {
            ++result.nodes_pruned;
            continue;
        }


        /*
            Integer LP solution.
        */
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


            continue;
        }


        /*
            Select branching candidates.
        */
        const auto candidates =
            build_branch_candidates(
                strengthened,
                current_lp.variable_values
            );


        if (
            candidates.empty()
        ) {
            ++result.nodes_pruned;
            continue;
        }


        /*
            Strong branching.
        */
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


        /*
            Create the two children.
        */
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
            We already solved both children during
            strong branching, so use those estimates
            as ordering bounds when available.

            For safety, fall back to the parent's
            relaxation bound.
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
            Do not enqueue a child that cannot beat
            the incumbent.
        */
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
                    sequence++
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
                    sequence++
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
                    strengthened.objective_sense(),
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


    /*
        No integer solution found.
    */
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


    /*
        If the queue is empty, the incumbent is proven
        optimal.

        If the queue still contains nodes and the node
        limit was reached, return IterationLimit.
    */
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
            "heuristics, cover-cut separation, and "
            "strong branching.";
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