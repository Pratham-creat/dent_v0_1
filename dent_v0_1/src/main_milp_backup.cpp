#include "dent/model/problem.hpp"
#include "dent/solver/milp.hpp"

#include <iomanip>
#include <iostream>

int main()
{
    using namespace dent;

    std::cout
        << "DENT Optimization Engine v0.6\n"
        << "MILP Branch-and-Bound Test\n\n";

    /*
     * MILP:
     *
     * Maximize:
     *     10x1 + 10x2
     *
     * Subject to:
     *     2x1 + x2 <= 4
     *     x1 + 2x2 <= 4
     *
     *     x1, x2 >= 0
     *     x1, x2 integer
     *
     * LP relaxation:
     *     x1 = 4/3
     *     x2 = 4/3
     *
     * Therefore Branch-and-Bound must branch.
     */

    Problem problem(
        ObjectiveSense::Maximize
    );

    int x1 =
        problem.add_variable(
            "x1",
            0.0,
            0.0,
            VariableType::Integer
        );

    int x2 =
        problem.add_variable(
            "x2",
            0.0,
            0.0,
            VariableType::Integer
        );

    problem.set_objective_coefficient(
        x1,
        10.0
    );

    problem.set_objective_coefficient(
        x2,
        10.0
    );

    int c1 =
        problem.add_constraint(
            "resource_1",
            ConstraintSense::LessEqual,
            4.0
        );

    problem.set_constraint_coefficient(
        c1,
        x1,
        2.0
    );

    problem.set_constraint_coefficient(
        c1,
        x2,
        1.0
    );

    int c2 =
        problem.add_constraint(
            "resource_2",
            ConstraintSense::LessEqual,
            4.0
        );

    problem.set_constraint_coefficient(
        c2,
        x1,
        1.0
    );

    problem.set_constraint_coefficient(
        c2,
        x2,
        2.0
    );

    std::cout
        << "Problem:\n"
        << "  Maximize 10x1 + 10x2\n"
        << "  2x1 + x2 <= 4\n"
        << "  x1 + 2x2 <= 4\n"
        << "  x1, x2 integer\n\n";

    MILPSolver solver(
        1e-9,
        10000
    );

    MILPSolution result =
        solver.solve(problem);

    std::cout
        << "MILP Solver:\n"
        << "  Algorithm        : Branch-and-Bound\n"
        << "  Nodes Explored   : "
        << result.nodes_explored
        << '\n'
        << "  Nodes Pruned     : "
        << result.nodes_pruned
        << '\n'
        << "  LP Solves        : "
        << result.lp_solves
        << "\n\n";

    std::cout
        << "Status:\n";

    switch (result.status) {

        case SolveStatus::Optimal:
            std::cout << "  OPTIMAL\n";
            break;

        case SolveStatus::Infeasible:
            std::cout << "  INFEASIBLE\n";
            break;

        case SolveStatus::Unbounded:
            std::cout << "  UNBOUNDED\n";
            break;

        case SolveStatus::IterationLimit:
            std::cout << "  NODE LIMIT\n";
            break;

        default:
            std::cout << "  UNSUPPORTED\n";
            break;
    }

    std::cout
        << "\nObjective     : "
        << std::fixed
        << std::setprecision(6)
        << result.objective_value
        << '\n';

    std::cout
        << "Variables:\n";

    for (std::size_t i = 0;
         i < result.variable_values.size();
         ++i) {

        std::cout
            << "  "
            << problem.variables()[i].name
            << " : "
            << result.variable_values[i]
            << '\n';
    }

    std::cout
        << "\nMessage:\n"
        << "  "
        << result.message
        << '\n';

    return 0;
}