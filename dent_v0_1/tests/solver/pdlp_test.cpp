#include "dent/model/problem.hpp"
#include "dent/solver/pdlp.hpp"

#include <cmath>
#include <iostream>
#include <string>

using namespace dent;

namespace {

bool approximately_equal(
    double a,
    double b,
    double tolerance = 1e-3
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
            problem.add_variable("x");

        const int y =
            problem.add_variable("y");

        problem.set_objective_coefficient(
            x, 3.0
        );

        problem.set_objective_coefficient(
            y, 2.0
        );

        int row =
            problem.add_constraint(
                "capacity",
                ConstraintSense::LessEqual,
                4.0
            );

        problem.set_constraint_coefficient(
            row, x, 1.0
        );

        problem.set_constraint_coefficient(
            row, y, 1.0
        );

        row =
            problem.add_constraint(
                "x_limit",
                ConstraintSense::LessEqual,
                2.0
            );

        problem.set_constraint_coefficient(
            row, x, 1.0
        );

        row =
            problem.add_constraint(
                "y_limit",
                ConstraintSense::LessEqual,
                3.0
            );

        problem.set_constraint_coefficient(
            row, y, 1.0
        );

        PDLPSolver solver(
            1e-6,
            50000
        );

        SolveResult result =
            solver.solve(problem);

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            result.variable_values.size() == 2 &&
            approximately_equal(
                result.variable_values[0],
                2.0,
                5e-3
            ) &&
            approximately_equal(
                result.variable_values[1],
                2.0,
                5e-3
            ) &&
            approximately_equal(
                result.objective_value,
                10.0,
                5e-3
            ) &&
            contains(
                result.message,
                "PDLP converged"
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] PDLP basic LP\n";
        }
        else {
            ++failed;

            std::cout
                << "[FAIL] PDLP basic LP\n"
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

            minimize x + y

            x + y >= 5
            x <= 5
            y <= 5

            x,y >= 0

        Optimum objective = 5.
    */
    {
        Problem problem(
            ObjectiveSense::Minimize
        );

        const int x =
            problem.add_variable("x");

        const int y =
            problem.add_variable("y");

        problem.set_objective_coefficient(
            x, 1.0
        );

        problem.set_objective_coefficient(
            y, 1.0
        );

        int row =
            problem.add_constraint(
                "minimum_total",
                ConstraintSense::GreaterEqual,
                5.0
            );

        problem.set_constraint_coefficient(
            row, x, 1.0
        );

        problem.set_constraint_coefficient(
            row, y, 1.0
        );

        row =
            problem.add_constraint(
                "x_limit",
                ConstraintSense::LessEqual,
                5.0
            );

        problem.set_constraint_coefficient(
            row, x, 1.0
        );

        row =
            problem.add_constraint(
                "y_limit",
                ConstraintSense::LessEqual,
                5.0
            );

        problem.set_constraint_coefficient(
            row, y, 1.0
        );

        PDLPSolver solver(
            1e-6,
            50000
        );

        SolveResult result =
            solver.solve(problem);

        const bool feasible =
            result.variable_values.size() == 2 &&
            result.variable_values[0] >= -1e-3 &&
            result.variable_values[1] >= -1e-3 &&
            result.variable_values[0] <= 5.01 &&
            result.variable_values[1] <= 5.01 &&
            result.variable_values[0] +
                result.variable_values[1] >=
                4.99;

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            feasible &&
            approximately_equal(
                result.objective_value,
                5.0,
                5e-3
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] PDLP minimization\n";
        }
        else {
            ++failed;

            std::cout
                << "[FAIL] PDLP minimization\n"
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
        Test 3

            maximize x + y

            x + y = 5
            x <= 4
            y <= 4

        Optimum objective = 5.
    */
    {
        Problem problem(
            ObjectiveSense::Maximize
        );

        const int x =
            problem.add_variable("x");

        const int y =
            problem.add_variable("y");

        problem.set_objective_coefficient(
            x, 1.0
        );

        problem.set_objective_coefficient(
            y, 1.0
        );

        int row =
            problem.add_constraint(
                "balance",
                ConstraintSense::Equal,
                5.0
            );

        problem.set_constraint_coefficient(
            row, x, 1.0
        );

        problem.set_constraint_coefficient(
            row, y, 1.0
        );

        row =
            problem.add_constraint(
                "x_limit",
                ConstraintSense::LessEqual,
                4.0
            );

        problem.set_constraint_coefficient(
            row, x, 1.0
        );

        row =
            problem.add_constraint(
                "y_limit",
                ConstraintSense::LessEqual,
                4.0
            );

        problem.set_constraint_coefficient(
            row, y, 1.0
        );

        PDLPSolver solver(
            1e-6,
            60000
        );

        SolveResult result =
            solver.solve(problem);

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            result.variable_values.size() == 2 &&
            approximately_equal(
                result.variable_values[0] +
                    result.variable_values[1],
                5.0,
                7e-3
            ) &&
            approximately_equal(
                result.objective_value,
                5.0,
                7e-3
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] PDLP equality handling\n";
        }
        else {
            ++failed;

            std::cout
                << "[FAIL] PDLP equality handling\n"
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

        Finite variable lower/upper bounds.

            maximize x

            2 <= x <= 5

        Optimum x = 5.
    */
    {
        Problem problem(
            ObjectiveSense::Maximize
        );

        const int x =
            problem.add_variable(
                "x",
                2.0,
                5.0,
                VariableType::Continuous
            );

        problem.set_objective_coefficient(
            x,
            1.0
        );

        PDLPSolver solver(
            1e-6,
            50000
        );

        SolveResult result =
            solver.solve(problem);

        const bool ok =
            result.status ==
                SolveStatus::Optimal &&
            result.variable_values.size() == 1 &&
            approximately_equal(
                result.variable_values[0],
                5.0,
                7e-3
            ) &&
            approximately_equal(
                result.objective_value,
                5.0,
                7e-3
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] PDLP variable bounds\n";
        }
        else {
            ++failed;

            std::cout
                << "[FAIL] PDLP variable bounds\n"
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
        Test 5

        Warm start.

        The second solve starts from the first PDLP solution.
    */
    {
        Problem problem(
            ObjectiveSense::Maximize
        );

        const int x =
            problem.add_variable("x");

        const int y =
            problem.add_variable("y");

        problem.set_objective_coefficient(
            x, 3.0
        );

        problem.set_objective_coefficient(
            y, 2.0
        );

        int row =
            problem.add_constraint(
                "capacity",
                ConstraintSense::LessEqual,
                4.0
            );

        problem.set_constraint_coefficient(
            row, x, 1.0
        );

        problem.set_constraint_coefficient(
            row, y, 1.0
        );

        row =
            problem.add_constraint(
                "x_limit",
                ConstraintSense::LessEqual,
                2.0
            );

        problem.set_constraint_coefficient(
            row, x, 1.0
        );

        PDLPSolver first(
            1e-6,
            50000
        );

        SolveResult first_result =
            first.solve(problem);

        WarmStart warm_start;

        warm_start.available =
            true;

        warm_start.variable_values =
            first_result.variable_values;

        warm_start.columns =
            static_cast<int>(
                first_result.variable_values.size()
            );

        warm_start.source =
            "pdlp-test";

        PDLPSolver second(
            1e-6,
            50000
        );

        second.set_warm_start(
            warm_start
        );

        SolveResult second_result =
            second.solve(problem);

        const bool ok =
            second_result.status ==
                SolveStatus::Optimal &&
            second_result.warm_start_used &&
            approximately_equal(
                second_result.objective_value,
                10.0,
                7e-3
            );

        if (ok) {
            ++passed;
            std::cout
                << "[PASS] PDLP warm start\n";
        }
        else {
            ++failed;

            std::cout
                << "[FAIL] PDLP warm start\n"
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
        << "\nPDLP regression\n"
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