#include "dent/model/problem.hpp"
#include "dent/solver/solver.hpp"
#include "dent/solver/dual_simplex.hpp"
#include "dent/solver/simplex.hpp"
#include "dent/warmstart/warm_start.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{

int passed = 0;
int failed = 0;

constexpr double EPS = 1e-7;

bool close_enough(
    double a,
    double b
)
{
    return std::abs(a - b) <= EPS;
}

void check(
    bool condition,
    const std::string& name
)
{
    if (condition)
    {
        ++passed;

        std::cout
            << "[PASS] "
            << name
            << '\n';
    }
    else
    {
        ++failed;

        std::cout
            << "[FAIL] "
            << name
            << '\n';
    }
}

/*
    ============================================================
    Base LP
    ============================================================

    Maximize:

        3 x1 + 2 x2

    subject to:

        x1 + x2 <= 4
        x1       <= 2

        x1,x2 >= 0

    Optimal:

        x1 = 2
        x2 = 2

    Objective = 10

    Standard-form columns:

        x1      -> 0
        x2      -> 1
        slack1  -> 2
        slack2  -> 3

    Optimal basis:

        [x1, slack2]

        basis = [0, 3]
*/
dent::Problem make_base_problem()
{
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
            "capacity",
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


/*
    ============================================================
    RHS modified LP
    ============================================================

    Same matrix:

        x1 + x2 <= 4
        x1       <= second_rhs

    For second_rhs = 0.25:

        x1 = 0.25
        x2 = 3.75

    Objective = 8.25
*/
dent::Problem make_rhs_modified_problem(
    double second_rhs
)
{
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
            "capacity",
            dent::ConstraintSense::LessEqual,
            4.0
        );

    const int c2 =
        problem.add_constraint(
            "x1_limit",
            dent::ConstraintSense::LessEqual,
            second_rhs
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


/*
    ============================================================
    Test 1
    Cold Dual Simplex path
    ============================================================
*/
void test_cold_dual_path()
{
    dent::Problem problem =
        make_base_problem();

    dent::DualSimplexSolver solver;

    dent::SolveResult result =
        solver.solve(
            problem
        );

    check(
        result.status ==
            dent::SolveStatus::Optimal,
        "Cold Dual Simplex solve"
    );

    check(
        close_enough(
            result.objective_value,
            10.0
        ),
        "Cold Dual Simplex objective"
    );

    check(
        result.variable_values.size() == 2 &&
        close_enough(
            result.variable_values[0],
            2.0
        ) &&
        close_enough(
            result.variable_values[1],
            2.0
        ),
        "Cold Dual Simplex solution"
    );
}


/*
    ============================================================
    Test 2
    Actual Dual Simplex reoptimization
    ============================================================
*/
void test_actual_dual_reoptimization()
{
    dent::Problem original =
        make_base_problem();

    dent::SimplexSolver primal_solver;

    dent::SolveResult first =
        primal_solver.solve(
            original
        );

    check(
        first.status ==
            dent::SolveStatus::Optimal,
        "Primal basis acquisition solve"
    );

    check(
        close_enough(
            first.objective_value,
            10.0
        ),
        "Primal basis acquisition objective"
    );

    dent::WarmStart warm_start =
        dent::make_basis_warm_start(
            original,
            {0, 3},
            2,
            4,
            first.variable_values,
            "primal-simplex"
        );

    check(
        warm_start.available,
        "Persisted basis marked available"
    );

    check(
        warm_start.matches_problem(
            original
        ),
        "Persisted basis matches original model"
    );

    dent::Problem modified =
        make_rhs_modified_problem(
            0.25
        );

    check(
        warm_start.matches_problem(
            modified
        ),
        "Warm-start survives RHS-only model change"
    );

    dent::DualSimplexSolver solver;

    solver.set_warm_start(
        warm_start
    );

    dent::SolveResult result =
        solver.solve(
            modified
        );

    check(
        result.status ==
            dent::SolveStatus::Optimal,
        "Actual Dual Simplex reoptimization"
    );

    check(
        result.warm_start_used,
        "Dual Simplex reports warm-start usage"
    );

    check(
        result.iterations >= 1,
        "Dual Simplex performed repair pivots"
    );

    check(
        close_enough(
            result.objective_value,
            8.25
        ),
        "Dual Simplex reoptimization objective"
    );

    check(
        result.variable_values.size() == 2 &&
        close_enough(
            result.variable_values[0],
            0.25
        ) &&
        close_enough(
            result.variable_values[1],
            3.75
        ),
        "Dual Simplex reoptimization solution"
    );
}


/*
    ============================================================
    Test 3
    Structural rejection
    ============================================================
*/
void test_structural_rejection()
{
    dent::Problem problem =
        make_base_problem();

    dent::WarmStart warm_start =
        dent::make_basis_warm_start(
            problem,
            {0, 3},
            2,
            4,
            {},
            "test"
        );

    dent::Problem changed =
        make_base_problem();

    /*
        Change coefficient matrix.

        RHS changes are allowed.
        Matrix changes must invalidate the basis.
    */
    changed.set_constraint_coefficient(
        0,
        1,
        2.0
    );

    check(
        !warm_start.matches_problem(
            changed
        ),
        "Warm-start rejected after matrix change"
    );

    dent::DualSimplexSolver solver;

    solver.set_warm_start(
        warm_start
    );

    dent::SolveResult result =
        solver.solve(
            changed
        );

    check(
        result.status ==
            dent::SolveStatus::Optimal ||
        result.status ==
            dent::SolveStatus::Unsupported,
        "Incompatible basis safely rejected"
    );

    check(
        !result.warm_start_used,
        "Rejected basis not reported as used"
    );
}


/*
    ============================================================
    Test 4
    Warm-start lifecycle
    ============================================================
*/
void test_clear_warm_start()
{
    dent::Problem problem =
        make_base_problem();

    dent::DualSimplexSolver solver;

    dent::WarmStart warm_start =
        dent::make_basis_warm_start(
            problem,
            {0, 3},
            2,
            4,
            {},
            "test"
        );

    solver.set_warm_start(
        warm_start
    );

    check(
        solver.has_warm_start(),
        "Dual warm-start lifecycle set"
    );

    solver.clear_warm_start();

    check(
        !solver.has_warm_start(),
        "Dual warm-start lifecycle clear"
    );
}


/*
    ============================================================
    Test 5
    Repeated RHS reoptimization
    ============================================================

    Simulates repeated operational reoptimization.

    The matrix remains fixed while the RHS changes.
*/
void test_repeated_rhs_reoptimization()
{
    dent::Problem original =
        make_base_problem();

    dent::WarmStart warm_start =
        dent::make_basis_warm_start(
            original,
            {0, 3},
            2,
            4,
            {2.0, 2.0},
            "repeated-production"
        );

    dent::DualSimplexSolver solver;

    const std::vector<double> rhs_values = {
        0.25,
        0.50,
        0.75,
        1.00
    };

    const std::vector<double> expected_objectives = {
        8.25,
        8.50,
        8.75,
        9.00
    };

    bool all_good = true;

    for (std::size_t i = 0;
         i < rhs_values.size();
         ++i)
    {
        dent::Problem modified =
            make_rhs_modified_problem(
                rhs_values[i]
            );

        solver.set_warm_start(
            warm_start
        );

        dent::SolveResult result =
            solver.solve(
                modified
            );

        if (
            result.status !=
                dent::SolveStatus::Optimal ||
            !result.warm_start_used ||
            !close_enough(
                result.objective_value,
                expected_objectives[i]
            )
        )
        {
            all_good = false;
            break;
        }

        warm_start =
            dent::make_basis_warm_start(
                modified,
                {0, 3},
                2,
                4,
                result.variable_values,
                "dual-simplex"
            );
    }

    check(
        all_good,
        "Repeated RHS reoptimization sequence"
    );
}

} // namespace


int main()
{
    std::cout
        << "============================================================\n"
        << " DENT DUAL SIMPLEX REGRESSION SUITE\n"
        << "============================================================\n\n";

    std::cout
        << "Testing cold solve, basis reuse, RHS reoptimization,\n"
        << "structural rejection and repeated operational solves.\n\n";

    test_cold_dual_path();

    test_actual_dual_reoptimization();

    test_structural_rejection();

    test_clear_warm_start();

    test_repeated_rhs_reoptimization();

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
        << "\n\n";

    if (failed == 0)
    {
        std::cout
            << "DENT Dual Simplex regression suite PASSED.\n";

        return 0;
    }

    std::cout
        << "DENT Dual Simplex regression suite FAILED.\n";

    return 1;
}