#include "dent/model/problem.hpp"
#include "dent/solver/simplex.hpp"
#include "dent/solver/solver_session.hpp"
#include "dent/warmstart/warm_start.hpp"

#include <iostream>
#include <memory>

int main()
{
    using namespace dent;

    Problem problem(ObjectiveSense::Maximize);

    const int x = problem.add_variable(
        "x",
        0.0,
        0.0,
        VariableType::Continuous
    );

    const int constraint =
        problem.add_constraint(
            "capacity",
            ConstraintSense::LessEqual,
            10.0
        );

    problem.set_objective_coefficient(
        x,
        5.0
    );

    problem.set_constraint_coefficient(
        constraint,
        x,
        1.0
    );

    auto simplex =
        std::make_shared<SimplexSolver>();

    SolverSession session(simplex);

    WarmStart warm_start;

    warm_start.available = true;

    warm_start.variable_values = {
        5.0
    };

    warm_start.source =
        "warm-start architecture test";

    session.set_warm_start(
        warm_start
    );

    if (!session.has_warm_start())
    {
        std::cerr
            << "FAIL: warm start was not stored.\n";

        return 1;
    }

    SolveResult result =
        session.solve(problem);

    if (result.status != SolveStatus::Optimal)
    {
        std::cerr
            << "FAIL: solver did not return OPTIMAL.\n";

        std::cerr
            << "Message: "
            << result.message
            << '\n';

        return 1;
    }

    std::cout
        << "Warm-start session test passed.\n";

    std::cout
        << "Objective: "
        << result.objective_value
        << '\n';

    std::cout
        << "x: "
        << result.variable_values[0]
        << '\n';

    std::cout
        << "Warm start stored: "
        << (session.has_warm_start() ? "yes" : "no")
        << '\n';

    return 0;
}