#include "dent/model/problem.hpp"
#include "dent/solver/solver.hpp"
#include "dent/validation/solution_validator.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int passed = 0;
int failed = 0;

void check(
    bool condition,
    const std::string& name
)
{
    if (condition) {
        std::cout
            << "[PASS] "
            << name
            << '\n';

        ++passed;
    }
    else {
        std::cout
            << "[FAIL] "
            << name
            << '\n';

        ++failed;
    }
}


dent::Problem make_lp()
{
    dent::Problem problem(
        dent::ObjectiveSense::Maximize
    );

    const int x =
        problem.add_variable(
            "x",
            0.0,
            10.0,
            dent::VariableType::Continuous
        );

    const int y =
        problem.add_variable(
            "y",
            0.0,
            10.0,
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

    problem.set_objective_coefficient(
        y,
        2.0
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

    return problem;
}


void test_valid_solution()
{
    dent::Problem problem =
        make_lp();

    dent::SolutionValidator validator(
        1e-7
    );

    const std::vector<double> values{
        10.0,
        0.0
    };

    const auto result =
        validator.validate_values(
            problem,
            values,
            30.0
        );

    check(
        result.valid,
        "Valid LP solution"
    );

    check(
        result.dimensions_valid,
        "Valid dimensions"
    );

    check(
        result.bounds_valid,
        "Valid variable bounds"
    );

    check(
        result.constraints_valid,
        "Valid constraints"
    );

    check(
        result.objective_valid,
        "Valid objective"
    );

    check(
        std::abs(
            result.computed_objective -
            30.0
        ) < 1e-9,
        "Objective recomputation"
    );
}


void test_constraint_violation()
{
    dent::Problem problem =
        make_lp();

    dent::SolutionValidator validator(
        1e-7
    );

    const std::vector<double> values{
        8.0,
        5.0
    };

    const auto result =
        validator.validate_values(
            problem,
            values,
            34.0
        );

    check(
        !result.valid,
        "Constraint violation detected"
    );

    check(
        !result.constraints_valid,
        "Constraint status rejected"
    );

    check(
        std::abs(
            result.max_constraint_violation -
            3.0
        ) < 1e-9,
        "Constraint residual is correct"
    );
}


void test_bound_violation()
{
    dent::Problem problem =
        make_lp();

    dent::SolutionValidator validator(
        1e-7
    );

    const std::vector<double> values{
        11.0,
        0.0
    };

    const auto result =
        validator.validate_values(
            problem,
            values,
            33.0
        );

    check(
        !result.valid,
        "Bound violation detected"
    );

    check(
        !result.bounds_valid,
        "Bound status rejected"
    );

    check(
        std::abs(
            result.max_bound_violation -
            1.0
        ) < 1e-9,
        "Bound violation magnitude"
    );
}


void test_objective_mismatch()
{
    dent::Problem problem =
        make_lp();

    dent::SolutionValidator validator(
        1e-7
    );

    const std::vector<double> values{
        10.0,
        0.0
    };

    const auto result =
        validator.validate_values(
            problem,
            values,
            25.0
        );

    check(
        !result.valid,
        "Objective mismatch detected"
    );

    check(
        !result.objective_valid,
        "Objective status rejected"
    );

    check(
        std::abs(
            result.objective_error -
            5.0
        ) < 1e-9,
        "Objective error is correct"
    );
}


void test_integer_validation()
{
    dent::Problem problem(
        dent::ObjectiveSense::Maximize
    );

    const int x =
        problem.add_variable(
            "x",
            0.0,
            10.0,
            dent::VariableType::Integer
        );

    problem.set_objective_coefficient(
        x,
        5.0
    );

    dent::SolutionValidator validator(
        1e-7
    );

    {
        const auto result =
            validator.validate_values(
                problem,
                {4.0},
                20.0
            );

        check(
            result.valid,
            "Integer solution accepted"
        );
    }

    {
        const auto result =
            validator.validate_values(
                problem,
                {4.5},
                22.5
            );

        check(
            !result.valid,
            "Fractional integer solution rejected"
        );

        check(
            !result.bounds_valid,
            "Integer integrality violation detected"
        );
    }
}


void test_binary_validation()
{
    dent::Problem problem(
        dent::ObjectiveSense::Maximize
    );

    const int x =
        problem.add_variable(
            "x",
            0.0,
            1.0,
            dent::VariableType::Binary
        );

    problem.set_objective_coefficient(
        x,
        10.0
    );

    dent::SolutionValidator validator(
        1e-7
    );

    {
        const auto result =
            validator.validate_values(
                problem,
                {1.0},
                10.0
            );

        check(
            result.valid,
            "Binary one accepted"
        );
    }

    {
        const auto result =
            validator.validate_values(
                problem,
                {0.0},
                0.0
            );

        check(
            result.valid,
            "Binary zero accepted"
        );
    }

    {
        const auto result =
            validator.validate_values(
                problem,
                {0.5},
                5.0
            );

        check(
            !result.valid,
            "Fractional binary solution rejected"
        );
    }
}


void test_qp_objective()
{
    dent::Problem problem(
        dent::ObjectiveSense::Minimize
    );

    const int x =
        problem.add_variable(
            "x",
            0.0,
            0.0,
            dent::VariableType::Continuous
        );

    problem.set_objective_coefficient(
        x,
        2.0
    );

    problem.set_quadratic_coefficient(
        x,
        x,
        4.0
    );

    /*
     * Objective:
     *
     * 1/2 * 4 * 2^2 + 2 * 2
     *
     * = 8 + 4
     * = 12
     */
    dent::SolutionValidator validator(
        1e-7
    );

    const auto result =
        validator.validate_values(
            problem,
            {2.0},
            12.0
        );

    check(
        result.valid,
        "QP objective verification"
    );

    check(
        std::abs(
            result.computed_objective -
            12.0
        ) < 1e-9,
        "QP objective value"
    );
}


void test_dimension_mismatch()
{
    dent::Problem problem =
        make_lp();

    dent::SolutionValidator validator(
        1e-7
    );

    const auto result =
        validator.validate_values(
            problem,
            {10.0},
            30.0
        );

    check(
        !result.valid,
        "Dimension mismatch rejected"
    );

    check(
        !result.dimensions_valid,
        "Dimension status rejected"
    );
}


void test_solver_result_status()
{
    dent::Problem problem =
        make_lp();

    dent::SolutionValidator validator(
        1e-7
    );

    dent::SolveResult result;

    result.status =
        dent::SolveStatus::IterationLimit;

    result.variable_values = {
        10.0,
        0.0
    };

    result.objective_value =
        30.0;

    const auto validation =
        validator.validate(
            problem,
            result
        );

    check(
        !validation.valid,
        "Non-optimal result is not accepted"
    );
}


} // namespace


int main()
{
    std::cout
        << "DENT SOLUTION VALIDATION REGRESSION SUITE\n\n";

    test_valid_solution();
    test_constraint_violation();
    test_bound_violation();
    test_objective_mismatch();
    test_integer_validation();
    test_binary_validation();
    test_qp_objective();
    test_dimension_mismatch();
    test_solver_result_status();

    std::cout << '\n';

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