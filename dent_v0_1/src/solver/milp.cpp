#include "dent/solver/milp.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace dent {

namespace {

Problem build_lp_relaxation(
    const Problem& original
)
{
    Problem relaxation(
        original.objective_sense()
    );

    const auto& variables =
        original.variables();

    /*
        Copy variables.

        Integer/Binary variables become
        continuous variables for the LP relaxation.

        Simplex requires lower bound = 0.
    */

    for (
        const auto& variable :
        variables
    ) {

        double lower =
            variable.lower_bound;

        if (
            std::abs(lower) > 1e-9
        ) {

            throw std::runtime_error(
                "MILP relaxation currently requires "
                "variable lower bounds of 0."
            );
        }

        int index =
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

    /*
        Copy original constraints.
    */

    for (
        std::size_t i = 0;
        i < original.constraints().size();
        ++i
    ) {

        const auto& source =
            original.constraints()[i];

        int constraint =
            relaxation.add_constraint(
                source.name,
                source.sense,
                source.rhs
            );

        for (
            std::size_t j = 0;
            j < variables.size();
            ++j
        ) {

            double coefficient =
                original.matrix()[i][j];

            if (
                std::abs(coefficient) >
                1e-12
            ) {

                relaxation.set_constraint_coefficient(
                    constraint,
                    static_cast<int>(j),
                    coefficient
                );
            }
        }
    }

    /*
        Add finite upper bounds.

        IMPORTANT:

        In DENT's Problem class:
            upper_bound == 0
        means:
            +infinity / no upper bound.

        Therefore we MUST NOT treat 0 as
        a finite upper bound.

        Binary variables are the exception:
            Binary -> x <= 1
    */

    for (
        std::size_t i = 0;
        i < variables.size();
        ++i
    ) {

        const auto& variable =
            variables[i];

        double upper =
            variable.upper_bound;

        bool binary =
            variable.type ==
            VariableType::Binary;

        /*
            0 means "no upper bound".
        */
        bool has_finite_upper =
            upper != 0.0 &&
            std::isfinite(upper);

        /*
            Binary variables always have
            an upper bound of 1.
        */
        if (binary) {

            upper = 1.0;

            has_finite_upper = true;
        }

        if (
            has_finite_upper
        ) {

            int constraint =
                relaxation.add_constraint(
                    variable.name +
                        "_upper_bound",
                    ConstraintSense::LessEqual,
                    upper
                );

            relaxation.set_constraint_coefficient(
                constraint,
                static_cast<int>(i),
                1.0
            );
        }
    }

    return relaxation;
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
    const auto& variables =
        problem.variables();

    if (
        values.size() !=
        variables.size()
    ) {

        return false;
    }

    for (
        std::size_t i = 0;
        i < variables.size();
        ++i
    ) {

        const auto& variable =
            variables[i];

        if (
            variable.type ==
            VariableType::Continuous
        ) {

            continue;
        }

        double value =
            values[i];

        double nearest =
            std::round(value);

        if (
            std::abs(
                value - nearest
            ) > tolerance_
        ) {

            return false;
        }

        /*
            Binary variables must be 0 or 1.
        */

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

        /*
            Respect lower bound.
        */

        if (
            nearest <
            variable.lower_bound -
                tolerance_
        ) {

            return false;
        }

        /*
            In DENT:
            upper_bound == 0 means
            no upper bound.
        */

        if (
            variable.upper_bound != 0.0 &&
            std::isfinite(
                variable.upper_bound
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


int MILPSolver::choose_branch_variable(
    const Problem& problem,
    const std::vector<double>& values
) const
{
    const auto& variables =
        problem.variables();

    int selected =
        -1;

    double best_fractionality =
        tolerance_;

    for (
        std::size_t i = 0;
        i < variables.size();
        ++i
    ) {

        if (
            variables[i].type ==
            VariableType::Continuous
        ) {

            continue;
        }

        double value =
            values[i];

        double lower =
            std::floor(value);

        double upper =
            std::ceil(value);

        double down =
            value - lower;

        double up =
            upper - value;

        double fractionality =
            std::min(
                down,
                up
            );

        if (
            fractionality >
            best_fractionality
        ) {

            best_fractionality =
                fractionality;

            selected =
                static_cast<int>(i);
        }
    }

    return selected;
}


void MILPSolver::branch_and_bound(
    const Problem& problem,
    MILPSolution& result,
    bool& has_incumbent,
    double& incumbent_objective,
    std::vector<double>& incumbent_values
) const
{
    /*
        Node limit.
    */

    if (
        result.nodes_explored >=
        max_nodes_
    ) {

        return;
    }

    ++result.nodes_explored;

    /*
        Build LP relaxation.
    */

    Problem relaxation_problem;

    try {

        relaxation_problem =
            build_lp_relaxation(
                problem
            );
    }
    catch (
        const std::exception&
        error
    ) {

        ++result.nodes_pruned;

        if (
            result.message.empty()
        ) {

            result.message =
                error.what();
        }

        return;
    }

    /*
        Solve LP relaxation.
    */

    SimplexSolver lp_solver(
        tolerance_,
        2000
    );

    SolveResult relaxation =
        lp_solver.solve(
            relaxation_problem
        );

    ++result.lp_solves;

    /*
        LP infeasible.
    */

    if (
        relaxation.status ==
        SolveStatus::Infeasible
    ) {

        ++result.nodes_pruned;

        return;
    }

    /*
        LP unbounded.
    */

    if (
        relaxation.status ==
        SolveStatus::Unbounded
    ) {

        ++result.nodes_pruned;

        return;
    }

    /*
        Only an optimal LP solution
        can safely provide a bound.
    */

    if (
        relaxation.status !=
        SolveStatus::Optimal
    ) {

        ++result.nodes_pruned;

        return;
    }

    /*
        Bound pruning.
    */

    if (
        has_incumbent
    ) {

        if (
            problem.objective_sense() ==
            ObjectiveSense::Maximize
        ) {

            /*
                LP gives an upper bound.
            */

            if (
                relaxation.objective_value <=
                incumbent_objective +
                    tolerance_
            ) {

                ++result.nodes_pruned;

                return;
            }
        }
        else {

            /*
                LP gives a lower bound.
            */

            if (
                relaxation.objective_value >=
                incumbent_objective -
                    tolerance_
            ) {

                ++result.nodes_pruned;

                return;
            }
        }
    }

    /*
        Check whether the LP solution
        already satisfies all integer
        restrictions.
    */

    if (
        is_integral_solution(
            problem,
            relaxation.variable_values
        )
    ) {

        bool better =
            false;

        if (
            !has_incumbent
        ) {

            better = true;
        }
        else if (
            problem.objective_sense() ==
            ObjectiveSense::Maximize
        ) {

            better =
                relaxation.objective_value >
                incumbent_objective +
                    tolerance_;
        }
        else {

            better =
                relaxation.objective_value <
                incumbent_objective -
                    tolerance_;
        }

        if (
            better
        ) {

            has_incumbent =
                true;

            incumbent_objective =
                relaxation.objective_value;

            incumbent_values =
                relaxation.variable_values;
        }

        return;
    }

    /*
        Choose variable to branch on.
    */

    int branch_variable =
        choose_branch_variable(
            problem,
            relaxation.variable_values
        );

    if (
        branch_variable < 0
    ) {

        ++result.nodes_pruned;

        return;
    }

    double value =
        relaxation.variable_values[
            branch_variable
        ];

    double lower_branch =
        std::floor(value);

    double upper_branch =
        std::ceil(value);

    /*
        Safety check.
    */

    if (
        std::abs(
            upper_branch -
            lower_branch
        ) < 0.5
    ) {

        ++result.nodes_pruned;

        return;
    }

    /*
        LEFT CHILD

            x <= floor(value)
    */

    Problem left =
        problem;

    int left_constraint =
        left.add_constraint(
            "branch_left",
            ConstraintSense::LessEqual,
            lower_branch
        );

    left.set_constraint_coefficient(
        left_constraint,
        branch_variable,
        1.0
    );

    branch_and_bound(
        left,
        result,
        has_incumbent,
        incumbent_objective,
        incumbent_values
    );

    /*
        Stop if node limit reached.
    */

    if (
        result.nodes_explored >=
        max_nodes_
    ) {

        return;
    }

    /*
        RIGHT CHILD

            x >= ceil(value)
    */

    Problem right =
        problem;

    int right_constraint =
        right.add_constraint(
            "branch_right",
            ConstraintSense::GreaterEqual,
            upper_branch
        );

    right.set_constraint_coefficient(
        right_constraint,
        branch_variable,
        1.0
    );

    branch_and_bound(
        right,
        result,
        has_incumbent,
        incumbent_objective,
        incumbent_values
    );
}


MILPSolution MILPSolver::solve(
    const Problem& problem
) const
{
    MILPSolution result;

    /*
        Continuous model.

        If MILPSolver is called on an LP,
        simply solve the LP.
    */

    if (
        !is_integer_problem(
            problem
        )
    ) {

        try {

            Problem relaxation =
                build_lp_relaxation(
                    problem
                );

            SimplexSolver lp_solver(
                tolerance_,
                2000
            );

            SolveResult lp =
                lp_solver.solve(
                    relaxation
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
        catch (
            const std::exception&
            error
        ) {

            result.status =
                SolveStatus::Unsupported;

            result.message =
                error.what();

            return result;
        }
    }

    /*
        Branch and Bound.
    */

    bool has_incumbent =
        false;

    double incumbent_objective =
        0.0;

    std::vector<double>
        incumbent_values;

    branch_and_bound(
        problem,
        result,
        has_incumbent,
        incumbent_objective,
        incumbent_values
    );

    /*
        Return best integer solution.
    */

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
                "Node limit reached. "
                "Best integer solution found "
                "is returned.";
        }
        else {

            result.message =
                "MILP solved using Branch-and-Bound.";
        }

        return result;
    }

    /*
        No integer solution.
    */

    if (
        result.nodes_explored >=
        max_nodes_
    ) {

        result.status =
            SolveStatus::IterationLimit;

        result.message =
            "MILP node limit reached before "
            "finding an integer solution.";

        return result;
    }

    result.status =
        SolveStatus::Infeasible;

    result.message =
        "No feasible integer solution exists.";

    return result;
}

} // namespace dent