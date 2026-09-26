#include "dent/model/problem.hpp"
#include "dent/solver/milp.hpp"

#include <cmath>
#include <iostream>
#include <string>

using namespace dent;

namespace {

bool approximately_equal(
    double a,
    double b,
    double tolerance = 1e-7
)
{
    return std::abs(a - b) <= tolerance;
}

bool contains(
    const std::string& value,
    const std::string& token
)
{
    return value.find(token) !=
           std::string::npos;
}

}

int main()
{
    int passed = 0;
    int failed = 0;

    /*
        ========================================================
        Test 1
        Branching is required.

            maximize 10x + 9y

            2x + 2y <= 3

            x,y integer >= 0

        LP relaxation:
            objective = 15

        MILP:
            x = 1
            y = 0
            objective = 10
    */
    {
        Problem problem(
            ObjectiveSense::Maximize
        );

        const int x =
            problem.add_variable(
                "x",
                0.0,
                0.0,
                VariableType::Integer
            );

        const int y =
            problem.add_variable(
                "y",
                0.0,
                0.0,
                VariableType::Integer
            );

        problem.set_objective_coefficient(
            x,
            10.0
        );

        problem.set_objective_coefficient(
            y,
            9.0
        );

        const int constraint =
            problem.add_constraint(
                "capacity",
                ConstraintSense::LessEqual,
                3.0
            );

        problem.set_constraint_coefficient(
            constraint,
            x,
            2.0
        );

        problem.set_constraint_coefficient(
            constraint,
            y,
            2.0
        );

        MILPSolver solver(
            1e-9,
            500
        );

        MILPSolution result =
            solver.solve(
                problem
            );

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            approximately_equal(
                result.objective_value,
                10.0
            ) &&
            result.variable_values.size() ==
                2 &&
            approximately_equal(
                result.variable_values[0],
                1.0
            ) &&
            approximately_equal(
                result.variable_values[1],
                0.0
            ) &&
            result.nodes_explored > 1 &&
            result.lp_solves > 1 &&
            contains(
                result.message,
                "Branch-and-Cut"
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] Branching MILP\n";
        }
        else {
            ++failed;
            std::cout
                << "[FAIL] Branching MILP\n";
            std::cout
                << "  status="
                << static_cast<int>(
                    result.status
                )
                << "\n";
            std::cout
                << "  objective="
                << result.objective_value
                << "\n";
            std::cout
                << "  nodes="
                << result.nodes_explored
                << "\n";
            std::cout
                << "  LP solves="
                << result.lp_solves
                << "\n";
            std::cout
                << "  warm starts="
                << result.warm_start_lp_solves
                << "\n";
            std::cout
                << "  strong branching="
                << result.strong_branching_solves
                << "\n";
        }
    }

    /*
        ========================================================
        Test 2
        Cover separation.

            maximize 10x1 + 9x2 + 8x3

            8x1 + 7x2 + 6x3 <= 10

            x1,x2,x3 binary

        Cover:
            x1 + x2 <= 1
            x1 + x3 <= 1
            x2 + x3 <= 1

        Integer optimum:
            x1 = 1
            x2 = 0
            x3 = 0

            objective = 10
    */
    {
        Problem problem(
            ObjectiveSense::Maximize
        );

        const int x1 =
            problem.add_variable(
                "x1",
                0.0,
                1.0,
                VariableType::Binary
            );

        const int x2 =
            problem.add_variable(
                "x2",
                0.0,
                1.0,
                VariableType::Binary
            );

        const int x3 =
            problem.add_variable(
                "x3",
                0.0,
                1.0,
                VariableType::Binary
            );

        problem.set_objective_coefficient(
            x1,
            10.0
        );

        problem.set_objective_coefficient(
            x2,
            9.0
        );

        problem.set_objective_coefficient(
            x3,
            8.0
        );

        const int constraint =
            problem.add_constraint(
                "knapsack",
                ConstraintSense::LessEqual,
                10.0
            );

        problem.set_constraint_coefficient(
            constraint,
            x1,
            8.0
        );

        problem.set_constraint_coefficient(
            constraint,
            x2,
            7.0
        );

        problem.set_constraint_coefficient(
            constraint,
            x3,
            6.0
        );

        MILPSolver solver(
            1e-9,
            500
        );

        MILPSolution result =
            solver.solve(
                problem
            );

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            approximately_equal(
                result.objective_value,
                10.0
            ) &&
            result.variable_values.size() ==
                3 &&
            approximately_equal(
                result.variable_values[0],
                1.0
            ) &&
            approximately_equal(
                result.variable_values[1],
                0.0
            ) &&
            approximately_equal(
                result.variable_values[2],
                0.0
            ) &&
            result.cuts_generated > 0 &&
            result.cuts_added > 0;

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] Cover separation\n";
        }
        else {
            ++failed;
            std::cout
                << "[FAIL] Cover separation\n";
            std::cout
                << "  status="
                << static_cast<int>(
                    result.status
                )
                << "\n";
            std::cout
                << "  objective="
                << result.objective_value
                << "\n";
            std::cout
                << "  cuts generated="
                << result.cuts_generated
                << "\n";
            std::cout
                << "  cuts added="
                << result.cuts_added
                << "\n";
        }
    }

    /*
        ========================================================
        Test 3
        Warm-start path.

        This model intentionally creates a fractional
        root relaxation so that child LPs can inherit
        the parent basis.
    */
    {
        Problem problem(
            ObjectiveSense::Maximize
        );

        const int x =
            problem.add_variable(
                "x",
                0.0,
                0.0,
                VariableType::Integer
            );

        const int y =
            problem.add_variable(
                "y",
                0.0,
                0.0,
                VariableType::Integer
            );

        problem.set_objective_coefficient(
            x,
            7.0
        );

        problem.set_objective_coefficient(
            y,
            6.0
        );

        const int constraint =
            problem.add_constraint(
                "capacity",
                ConstraintSense::LessEqual,
                5.0
            );

        problem.set_constraint_coefficient(
            constraint,
            x,
            3.0
        );

        problem.set_constraint_coefficient(
            constraint,
            y,
            2.0
        );

        MILPSolver solver(
            1e-9,
            500
        );

        MILPSolution result =
            solver.solve(
                problem
            );

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            result.lp_solves > 0 &&
            result.warm_start_lp_solves > 0;

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] Node LP warm starts\n";
        }
        else {
            ++failed;
            std::cout
                << "[FAIL] Node LP warm starts\n";
            std::cout
                << "  status="
                << static_cast<int>(
                    result.status
                )
                << "\n";
            std::cout
                << "  LP solves="
                << result.lp_solves
                << "\n";
            std::cout
                << "  warm starts="
                << result.warm_start_lp_solves
                << "\n";
        }
    }

    /*
        ========================================================
        Test 4
        Incumbent heuristic should be active.
    */
    {
        Problem problem(
            ObjectiveSense::Maximize
        );

        const int x =
            problem.add_variable(
                "x",
                0.0,
                1.0,
                VariableType::Binary
            );

        const int y =
            problem.add_variable(
                "y",
                0.0,
                1.0,
                VariableType::Binary
            );

        problem.set_objective_coefficient(
            x,
            10.0
        );

        problem.set_objective_coefficient(
            y,
            8.0
        );

        const int constraint =
            problem.add_constraint(
                "capacity",
                ConstraintSense::LessEqual,
                1.0
            );

        problem.set_constraint_coefficient(
            constraint,
            x,
            1.0
        );

        problem.set_constraint_coefficient(
            constraint,
            y,
            1.0
        );

        MILPSolver solver(
            1e-9,
            500
        );

        MILPSolution result =
            solver.solve(
                problem
            );

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            approximately_equal(
                result.objective_value,
                10.0
            ) &&
            result.heuristic_attempts > 0;

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] Incumbent heuristic\n";
        }
        else {
            ++failed;
            std::cout
                << "[FAIL] Incumbent heuristic\n";
            std::cout
                << "  status="
                << static_cast<int>(
                    result.status
                )
                << "\n";
            std::cout
                << "  objective="
                << result.objective_value
                << "\n";
            std::cout
                << "  heuristic attempts="
                << result.heuristic_attempts
                << "\n";
        }
    }

    std::cout
        << "\nMILP strengthening regression\n"
        << "Passed: "
        << passed
        << "\n"
        << "Failed: "
        << failed
        << "\n";

    return failed == 0
        ? 0
        : 1;
}