#include "dent/model/problem.hpp"
#include "dent/solver/pdhg.hpp"

#include <cmath>
#include <iostream>
#include <string>

using namespace dent;

namespace {

bool approximately_equal(
    double a,
    double b,
    double tolerance = 1e-4
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
        Continuous maximization LP

            maximize 3x + 2y

            x + y <= 4
            x <= 2
            y <= 3

            x,y >= 0

        Optimum:

            x = 2
            y = 2

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
                VariableType::Continuous
            );

        const int y =
            problem.add_variable(
                "y",
                0.0,
                0.0,
                VariableType::Continuous
            );

        problem.set_objective_coefficient(
            x,
            3.0
        );

        problem.set_objective_coefficient(
            y,
            2.0
        );

        int row =
            problem.add_constraint(
                "capacity",
                ConstraintSense::LessEqual,
                4.0
            );

        problem.set_constraint_coefficient(
            row,
            x,
            1.0
        );

        problem.set_constraint_coefficient(
            row,
            y,
            1.0
        );

        row =
            problem.add_constraint(
                "x_limit",
                ConstraintSense::LessEqual,
                2.0
            );

        problem.set_constraint_coefficient(
            row,
            x,
            1.0
        );

        row =
            problem.add_constraint(
                "y_limit",
                ConstraintSense::LessEqual,
                3.0
            );

        problem.set_constraint_coefficient(
            row,
            y,
            1.0
        );

        PDHGSolver solver(
            1e-6,
            30000
        );

        SolveResult result =
            solver.solve(
                problem
            );

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            result.variable_values.size() == 2 &&
            approximately_equal(
                result.variable_values[0],
                2.0,
                2e-3
            ) &&
            approximately_equal(
                result.variable_values[1],
                2.0,
                2e-3
            ) &&
            approximately_equal(
                result.objective_value,
                10.0,
                2e-3
            ) &&
            contains(
                result.message,
                "PDHG converged"
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] PDHG continuous LP\n";
        }
        else {
            ++failed;

            std::cout
                << "[FAIL] PDHG continuous LP\n"
                << "  status="
                << static_cast<int>(
                    result.status
                )
                << "\n"
                << "  objective="
                << result.objective_value
                << "\n"
                << "  iterations="
                << result.iterations
                << "\n";

            if (
                result.variable_values.size() >= 2
            ) {
                std::cout
                    << "  x="
                    << result.variable_values[0]
                    << "\n"
                    << "  y="
                    << result.variable_values[1]
                    << "\n";
            }
        }
    }


    /*
        ========================================================
        Test 2
        Minimization LP

            minimize x + y

            x + y >= 5
            x <= 5
            y <= 5

            x,y >= 0

        Optimum objective = 5.

        Multiple optimal solutions exist.
    */
    {
        Problem problem(
            ObjectiveSense::Minimize
        );

        const int x =
            problem.add_variable(
                "x",
                0.0,
                0.0,
                VariableType::Continuous
            );

        const int y =
            problem.add_variable(
                "y",
                0.0,
                0.0,
                VariableType::Continuous
            );

        problem.set_objective_coefficient(
            x,
            1.0
        );

        problem.set_objective_coefficient(
            y,
            1.0
        );

        int row =
            problem.add_constraint(
                "minimum_total",
                ConstraintSense::GreaterEqual,
                5.0
            );

        problem.set_constraint_coefficient(
            row,
            x,
            1.0
        );

        problem.set_constraint_coefficient(
            row,
            y,
            1.0
        );

        row =
            problem.add_constraint(
                "x_limit",
                ConstraintSense::LessEqual,
                5.0
            );

        problem.set_constraint_coefficient(
            row,
            x,
            1.0
        );

        row =
            problem.add_constraint(
                "y_limit",
                ConstraintSense::LessEqual,
                5.0
            );

        problem.set_constraint_coefficient(
            row,
            y,
            1.0
        );

        PDHGSolver solver(
            1e-6,
            30000
        );

        SolveResult result =
            solver.solve(
                problem
            );

        const bool feasible =
            result.variable_values.size() == 2 &&
            result.variable_values[0] >= -1e-3 &&
            result.variable_values[1] >= -1e-3 &&
            result.variable_values[0] <= 5.001 &&
            result.variable_values[1] <= 5.001 &&
            result.variable_values[0] +
                result.variable_values[1] >=
                4.995;

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            feasible &&
            approximately_equal(
                result.objective_value,
                5.0,
                3e-3
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] PDHG minimization\n";
        }
        else {
            ++failed;

            std::cout
                << "[FAIL] PDHG minimization\n"
                << "  status="
                << static_cast<int>(
                    result.status
                )
                << "\n"
                << "  objective="
                << result.objective_value
                << "\n";

            if (
                result.variable_values.size() >= 2
            ) {
                std::cout
                    << "  x="
                    << result.variable_values[0]
                    << "\n"
                    << "  y="
                    << result.variable_values[1]
                    << "\n";
            }
        }
    }


    /*
        ========================================================
        Test 3
        Equality handling

            maximize x + y

            x + y = 5
            x <= 4
            y <= 4

            x,y >= 0

        Optimum objective = 5.
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
                VariableType::Continuous
            );

        const int y =
            problem.add_variable(
                "y",
                0.0,
                0.0,
                VariableType::Continuous
            );

        problem.set_objective_coefficient(
            x,
            1.0
        );

        problem.set_objective_coefficient(
            y,
            1.0
        );

        int row =
            problem.add_constraint(
                "balance",
                ConstraintSense::Equal,
                5.0
            );

        problem.set_constraint_coefficient(
            row,
            x,
            1.0
        );

        problem.set_constraint_coefficient(
            row,
            y,
            1.0
        );

        row =
            problem.add_constraint(
                "x_limit",
                ConstraintSense::LessEqual,
                4.0
            );

        problem.set_constraint_coefficient(
            row,
            x,
            1.0
        );

        row =
            problem.add_constraint(
                "y_limit",
                ConstraintSense::LessEqual,
                4.0
            );

        problem.set_constraint_coefficient(
            row,
            y,
            1.0
        );

        PDHGSolver solver(
            1e-6,
            40000
        );

        SolveResult result =
            solver.solve(
                problem
            );

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            result.variable_values.size() == 2 &&
            approximately_equal(
                result.variable_values[0] +
                    result.variable_values[1],
                5.0,
                3e-3
            ) &&
            approximately_equal(
                result.objective_value,
                5.0,
                3e-3
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] PDHG equality constraints\n";
        }
        else {
            ++failed;

            std::cout
                << "[FAIL] PDHG equality constraints\n"
                << "  status="
                << static_cast<int>(
                    result.status
                )
                << "\n"
                << "  objective="
                << result.objective_value
                << "\n";
        }
    }


    /*
        ========================================================
        Test 4
        Warm-start regression.

        Solve once, then solve again using the primal
        iterate produced by the first solve.
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
                VariableType::Continuous
            );

        const int y =
            problem.add_variable(
                "y",
                0.0,
                0.0,
                VariableType::Continuous
            );

        problem.set_objective_coefficient(
            x,
            3.0
        );

        problem.set_objective_coefficient(
            y,
            2.0
        );

        int row =
            problem.add_constraint(
                "capacity",
                ConstraintSense::LessEqual,
                4.0
            );

        problem.set_constraint_coefficient(
            row,
            x,
            1.0
        );

        problem.set_constraint_coefficient(
            row,
            y,
            1.0
        );

        row =
            problem.add_constraint(
                "x_limit",
                ConstraintSense::LessEqual,
                2.0
            );

        problem.set_constraint_coefficient(
            row,
            x,
            1.0
        );

        PDHGSolver first(
            1e-6,
            30000
        );

        SolveResult first_result =
            first.solve(
                problem
            );

        WarmStart warm_start;

        warm_start.available =
            true;

        warm_start.variable_values =
            first_result.variable_values;

        warm_start.rows =
            0;

        warm_start.columns =
            static_cast<int>(
                first_result.variable_values.size()
            );

        warm_start.source =
            "pdhg-test";

        PDHGSolver second(
            1e-6,
            30000
        );

        second.set_warm_start(
            warm_start
        );

        SolveResult second_result =
            second.solve(
                problem
            );

        const bool ok =
            second_result.status ==
                SolveStatus::Optimal &&
            second_result.warm_start_used &&
            approximately_equal(
                second_result.objective_value,
                10.0,
                3e-3
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] PDHG warm start\n";
        }
        else {
            ++failed;

            std::cout
                << "[FAIL] PDHG warm start\n"
                << "  status="
                << static_cast<int>(
                    second_result.status
                )
                << "\n"
                << "  objective="
                << second_result.objective_value
                << "\n"
                << "  warm start used="
                << second_result.warm_start_used
                << "\n";
        }
    }


    std::cout
        << "\nPDHG regression\n"
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