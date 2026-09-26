#include "dent/model/problem.hpp"
#include "dent/solver/interior_point.hpp"
#include "dent/solver/solver.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int passed = 0;
int failed = 0;

constexpr double TOL = 1e-5;

void check(
    bool condition,
    const std::string& name
) {
    if (condition) {
        std::cout << "[PASS] " << name << '\n';
        ++passed;
    }
    else {
        std::cout << "[FAIL] " << name << '\n';
        ++failed;
    }
}

bool near(
    double actual,
    double expected,
    double tolerance = TOL
) {
    return std::abs(actual - expected) <= tolerance;
}

dent::Problem make_basic_max_lp() {
    dent::Problem problem(
        dent::ObjectiveSense::Maximize
    );

    const int x1 =
        problem.add_variable(
            "x1",
            0.0,
            0.0,
            dent::VariableType::Continuous
        );

    const int x2 =
        problem.add_variable(
            "x2",
            0.0,
            0.0,
            dent::VariableType::Continuous
        );

    const int c1 =
        problem.add_constraint(
            "resource",
            dent::ConstraintSense::LessEqual,
            4.0
        );

    const int c2 =
        problem.add_constraint(
            "x1_limit",
            dent::ConstraintSense::LessEqual,
            2.0
        );

    problem.set_objective_coefficient(
        x1,
        3.0
    );

    problem.set_objective_coefficient(
        x2,
        2.0
    );

    problem.set_constraint_coefficient(
        c1,
        x1,
        1.0
    );

    problem.set_constraint_coefficient(
        c1,
        x2,
        1.0
    );

    problem.set_constraint_coefficient(
        c2,
        x1,
        1.0
    );

    return problem;
}

dent::Problem make_minimization_lp() {
    dent::Problem problem(
        dent::ObjectiveSense::Minimize
    );

    const int x1 =
        problem.add_variable(
            "x1",
            0.0,
            0.0,
            dent::VariableType::Continuous
        );

    const int x2 =
        problem.add_variable(
            "x2",
            0.0,
            0.0,
            dent::VariableType::Continuous
        );

    const int demand =
        problem.add_constraint(
            "demand",
            dent::ConstraintSense::GreaterEqual,
            4.0
        );

    problem.set_objective_coefficient(
        x1,
        2.0
    );

    problem.set_objective_coefficient(
        x2,
        3.0
    );

    problem.set_constraint_coefficient(
        demand,
        x1,
        1.0
    );

    problem.set_constraint_coefficient(
        demand,
        x2,
        1.0
    );

    return problem;
}

dent::Problem make_mixed_sense_lp() {
    dent::Problem problem(
        dent::ObjectiveSense::Maximize
    );

    const int x1 =
        problem.add_variable(
            "x1",
            0.0,
            0.0,
            dent::VariableType::Continuous
        );

    const int x2 =
        problem.add_variable(
            "x2",
            0.0,
            0.0,
            dent::VariableType::Continuous
        );

    const int c1 =
        problem.add_constraint(
            "upper",
            dent::ConstraintSense::LessEqual,
            6.0
        );

    const int c2 =
        problem.add_constraint(
            "lower",
            dent::ConstraintSense::GreaterEqual,
            4.0
        );

    const int c3 =
        problem.add_constraint(
            "balance",
            dent::ConstraintSense::Equal,
            3.0
        );

    problem.set_objective_coefficient(
        x1,
        4.0
    );

    problem.set_objective_coefficient(
        x2,
        2.0
    );

    problem.set_constraint_coefficient(
        c1,
        x1,
        1.0
    );

    problem.set_constraint_coefficient(
        c1,
        x2,
        1.0
    );

    problem.set_constraint_coefficient(
        c2,
        x1,
        1.0
    );

    problem.set_constraint_coefficient(
        c2,
        x2,
        1.0
    );

    problem.set_constraint_coefficient(
        c3,
        x1,
        1.0
    );

    return problem;
}

dent::Problem make_bounded_lp() {
    dent::Problem problem(
        dent::ObjectiveSense::Maximize
    );

    const int x =
        problem.add_variable(
            "x",
            0.0,
            5.0,
            dent::VariableType::Continuous
        );

    const int constraint =
        problem.add_constraint(
            "capacity",
            dent::ConstraintSense::LessEqual,
            10.0
        );

    problem.set_objective_coefficient(
        x,
        3.0
    );

    problem.set_constraint_coefficient(
        constraint,
        x,
        1.0
    );

    return problem;
}

} // namespace

int main() {
    std::cout
        << "DENT INTERIOR-POINT REGRESSION SUITE\n\n";

    /*
     * Test 1:
     *
     * max 3x1 + 2x2
     *
     * x1 + x2 <= 4
     * x1 <= 2
     * x1,x2 >= 0
     *
     * optimum:
     * x1 = 2
     * x2 = 2
     * objective = 10
     */
    {
        dent::Problem problem =
            make_basic_max_lp();

        dent::InteriorPointSolver solver(
            1e-8,
            100
        );

        dent::SolveResult result =
            solver.solve(problem);

        check(
            result.status ==
                dent::SolveStatus::Optimal,
            "Basic LP status"
        );

        check(
            near(
                result.objective_value,
                10.0,
                1e-4
            ),
            "Basic LP objective"
        );

        check(
            result.variable_values.size() == 2 &&
            near(
                result.variable_values[0],
                2.0,
                1e-4
            ) &&
            near(
                result.variable_values[1],
                2.0,
                1e-4
            ),
            "Basic LP solution"
        );
    }

    /*
     * Test 2:
     *
     * min 2x1 + 3x2
     *
     * x1 + x2 >= 4
     *
     * optimum:
     * x1 = 4
     * x2 = 0
     * objective = 8
     */
    {
        dent::Problem problem =
            make_minimization_lp();

        dent::InteriorPointSolver solver(
            1e-8,
            100
        );

        dent::SolveResult result =
            solver.solve(problem);

        check(
            result.status ==
                dent::SolveStatus::Optimal,
            "Minimization LP status"
        );

        check(
            near(
                result.objective_value,
                8.0,
                1e-4
            ),
            "Minimization LP objective"
        );

        check(
            result.variable_values.size() == 2 &&
            near(
                result.variable_values[0],
                4.0,
                1e-4
            ) &&
            near(
                result.variable_values[1],
                0.0,
                1e-4
            ),
            "Minimization LP solution"
        );
    }

    /*
     * Test 3:
     *
     * Mixed <=, >= and equality constraints.
     *
     * x1 + x2 <= 6
     * x1 + x2 >= 4
     * x1 = 3
     *
     * maximize 4x1 + 2x2
     *
     * x2 = 3
     * objective = 18
     */
    {
        dent::Problem problem =
            make_mixed_sense_lp();

        dent::InteriorPointSolver solver(
            1e-8,
            150
        );

        dent::SolveResult result =
            solver.solve(problem);

        check(
            result.status ==
                dent::SolveStatus::Optimal,
            "Mixed constraint LP status"
        );

        check(
            near(
                result.objective_value,
                18.0,
                1e-4
            ),
            "Mixed constraint LP objective"
        );

        check(
            result.variable_values.size() == 2 &&
            near(
                result.variable_values[0],
                3.0,
                1e-4
            ) &&
            near(
                result.variable_values[1],
                3.0,
                1e-4
            ),
            "Mixed constraint LP solution"
        );
    }

    /*
     * Test 4:
     *
     * max 3x
     * x <= 10
     * x <= 5 through variable upper bound
     *
     * expected x = 5
     * objective = 15
     */
    {
        dent::Problem problem =
            make_bounded_lp();

        dent::InteriorPointSolver solver(
            1e-8,
            100
        );

        dent::SolveResult result =
            solver.solve(problem);

        check(
            result.status ==
                dent::SolveStatus::Optimal,
            "Finite variable bound LP status"
        );

        check(
            near(
                result.objective_value,
                15.0,
                1e-4
            ),
            "Finite variable bound LP objective"
        );

        check(
            result.variable_values.size() == 1 &&
            near(
                result.variable_values[0],
                5.0,
                1e-4
            ),
            "Finite variable bound LP solution"
        );
    }

    /*
     * Test 5:
     * Integer variables must be rejected.
     */
    {
        dent::Problem problem(
            dent::ObjectiveSense::Maximize
        );

        const int x =
            problem.add_variable(
                "x",
                0.0,
                0.0,
                dent::VariableType::Integer
            );

        const int constraint =
            problem.add_constraint(
                "capacity",
                dent::ConstraintSense::LessEqual,
                5.0
            );

        problem.set_objective_coefficient(
            x,
            1.0
        );

        problem.set_constraint_coefficient(
            constraint,
            x,
            1.0
        );

        dent::InteriorPointSolver solver;

        dent::SolveResult result =
            solver.solve(problem);

        check(
            result.status ==
                dent::SolveStatus::Unsupported,
            "Integer model rejection"
        );
    }

    /*
     * Test 6:
     * Deterministic repeated solve.
     */
    {
        dent::Problem problem =
            make_basic_max_lp();

        dent::InteriorPointSolver solver(
            1e-8,
            100
        );

        dent::SolveResult first =
            solver.solve(problem);

        dent::SolveResult second =
            solver.solve(problem);

        check(
            first.status ==
                dent::SolveStatus::Optimal &&
            second.status ==
                dent::SolveStatus::Optimal,
            "Repeated IPM solve status"
        );

        check(
            near(
                first.objective_value,
                second.objective_value,
                1e-8
            ),
            "Repeated IPM objective consistency"
        );

        check(
            first.variable_values.size() ==
                second.variable_values.size(),
            "Repeated IPM solution size consistency"
        );
    }

    std::cout << "\n";

    std::cout
        << "Passed : "
        << passed
        << '\n';

    std::cout
        << "Failed : "
        << failed
        << '\n';

    std::cout
        << "Total  : "
        << passed + failed
        << '\n';

    return failed == 0 ? 0 : 1;
}