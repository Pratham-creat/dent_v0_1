#include "dent/model/problem.hpp"
#include "dent/presolve/scaling.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{

int passed = 0;
int failed = 0;

constexpr double TOL = 1e-8;

void check(
    bool condition,
    const std::string& name
)
{
    if (condition)
    {
        std::cout
            << "[PASS] "
            << name
            << '\n';

        ++passed;
    }
    else
    {
        std::cout
            << "[FAIL] "
            << name
            << '\n';

        ++failed;
    }
}

bool near(
    double actual,
    double expected,
    double tolerance = TOL
)
{
    return std::abs(
        actual - expected
    ) <= tolerance;
}

} // namespace

int main()
{
    std::cout
        << "============================================================\n"
        << " DENT SCALING REGRESSION SUITE\n"
        << "============================================================\n\n";

    /*
        --------------------------------------------------------
        Test 1
        Continuous LP scaling + postsolve.
        --------------------------------------------------------

        max 10000 x + 0.0001 y

        10000 x + y <= 10
        x + 10000 y <= 10
        --------------------------------------------------------
    */
    {
        dent::Problem problem(
            dent::ObjectiveSense::Maximize
        );

        const int x =
            problem.add_variable(
                "x"
            );

        const int y =
            problem.add_variable(
                "y"
            );

        const int c1 =
            problem.add_constraint(
                "c1",
                dent::ConstraintSense::LessEqual,
                10.0
            );

        const int c2 =
            problem.add_constraint(
                "c2",
                dent::ConstraintSense::LessEqual,
                10.0
            );

        problem.set_objective_coefficient(
            x,
            10000.0
        );

        problem.set_objective_coefficient(
            y,
            0.0001
        );

        problem.set_constraint_coefficient(
            c1,
            x,
            10000.0
        );

        problem.set_constraint_coefficient(
            c1,
            y,
            1.0
        );

        problem.set_constraint_coefficient(
            c2,
            x,
            1.0
        );

        problem.set_constraint_coefficient(
            c2,
            y,
            10000.0
        );

        dent::GeometricScaler scaler(
            1e-9,
            10
        );

        dent::ScalingResult result =
            scaler.scale(
                problem
            );

        check(
            result.applied,
            "Continuous LP scaling applied"
        );

        check(
            result.row_scale.size() == 2 &&
            result.column_scale.size() == 2,
            "Scaling metadata dimensions"
        );

        check(
            result.rows_scaled > 0 ||
            result.columns_scaled > 0,
            "At least one scaling factor changed"
        );

        /*
            x = D z.
            Provide a known scaled point and verify
            reverse transformation.
        */
        const std::vector<double> scaled_values = {
            2.0,
            3.0
        };

        const std::vector<double> original_values =
            result.postsolve_values(
                scaled_values
            );

        check(
            original_values.size() == 2,
            "Scaled solution size"
        );

        check(
            near(
                original_values[0],
                scaled_values[0] *
                    result.column_scale[0]
            ) &&
            near(
                original_values[1],
                scaled_values[1] *
                    result.column_scale[1]
            ),
            "Postsolve unscaling"
        );
    }

    /*
        --------------------------------------------------------
        Test 2
        Integer and binary columns must not be scaled.
        --------------------------------------------------------
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

        const int y =
            problem.add_variable(
                "y",
                0.0,
                1.0,
                dent::VariableType::Binary
            );

        const int constraint =
            problem.add_constraint(
                "capacity",
                dent::ConstraintSense::LessEqual,
                100.0
            );

        problem.set_objective_coefficient(
            x,
            10.0
        );

        problem.set_objective_coefficient(
            y,
            5.0
        );

        problem.set_constraint_coefficient(
            constraint,
            x,
            10000.0
        );

        problem.set_constraint_coefficient(
            constraint,
            y,
            0.0001
        );

        dent::GeometricScaler scaler(
            1e-9,
            10
        );

        dent::ScalingResult result =
            scaler.scale(
                problem
            );

        check(
            result.applied,
            "Mixed-integer scaling completes"
        );

        check(
            near(
                result.column_scale[0],
                1.0
            ),
            "Integer column remains unscaled"
        );

        check(
            near(
                result.column_scale[1],
                1.0
            ),
            "Binary column remains unscaled"
        );
    }

    /*
        --------------------------------------------------------
        Test 3
        Empty model.
        --------------------------------------------------------
    */
    {
        dent::Problem problem;

        dent::GeometricScaler scaler;

        dent::ScalingResult result =
            scaler.scale(
                problem
            );

        check(
            !result.applied,
            "Empty model skips scaling"
        );

        check(
            result.message ==
                "Scaling skipped: empty model.",
            "Empty-model scaling message"
        );
    }

    /*
        --------------------------------------------------------
        Test 4
        QP scaling transformation.
        --------------------------------------------------------
    */
    {
        dent::Problem problem(
            dent::ObjectiveSense::Minimize
        );

        const int x =
            problem.add_variable(
                "x"
            );

        const int y =
            problem.add_variable(
                "y"
            );

        const int constraint =
            problem.add_constraint(
                "capacity",
                dent::ConstraintSense::LessEqual,
                10.0
            );

        problem.set_objective_coefficient(
            x,
            2.0
        );

        problem.set_objective_coefficient(
            y,
            3.0
        );

        problem.set_constraint_coefficient(
            constraint,
            x,
            1000.0
        );

        problem.set_constraint_coefficient(
            constraint,
            y,
            1.0
        );

        problem.set_quadratic_coefficient(
            x,
            x,
            100.0
        );

        problem.set_quadratic_coefficient(
            y,
            y,
            0.01
        );

        dent::GeometricScaler scaler(
            1e-9,
            10
        );

        dent::ScalingResult result =
            scaler.scale(
                problem
            );

        check(
            result.applied,
            "QP scaling applied"
        );

        check(
            result.scaled_problem.quadratic_matrix().size()
                == 2,
            "QP quadratic matrix preserved"
        );

        check(
            std::isfinite(
                result.scaled_problem
                    .quadratic_matrix()[0][0]
            ) &&
            std::isfinite(
                result.scaled_problem
                    .quadratic_matrix()[1][1]
            ),
            "QP scaled quadratic coefficients finite"
        );
    }

    std::cout
        << "\n============================================================\n"
        << " TEST SUMMARY\n"
        << "============================================================\n";

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

    if (failed == 0)
    {
        std::cout
            << "\nDENT scaling regression suite PASSED.\n";

        return 0;
    }

    std::cout
        << "\nDENT scaling regression suite FAILED.\n";

    return 1;
}