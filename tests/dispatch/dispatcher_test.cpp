#include "dent/dispatch/dispatcher.hpp"
#include "dent/dispatch/fingerprint.hpp"
#include "dent/model/problem.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int passed = 0;
int failed = 0;

void check(
    bool condition,
    const std::string& name
) {
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

dent::Problem make_small_lp() {
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

    const int c =
        problem.add_constraint(
            "capacity",
            dent::ConstraintSense::LessEqual,
            10.0
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
        c,
        x1,
        1.0
    );

    problem.set_constraint_coefficient(
        c,
        x2,
        1.0
    );

    return problem;
}

dent::Problem make_large_sparse_lp() {
    dent::Problem problem(
        dent::ObjectiveSense::Maximize
    );

    constexpr int variables = 1000;
    constexpr int constraints = 1000;

    for (int i = 0;
         i < variables;
         ++i) {

        problem.add_variable(
            "x" + std::to_string(i),
            0.0,
            0.0,
            dent::VariableType::Continuous
        );
    }

    for (int i = 0;
         i < constraints;
         ++i) {

        problem.add_constraint(
            "c" + std::to_string(i),
            dent::ConstraintSense::LessEqual,
            100.0
        );
    }

    /*
     * Only one coefficient per row.
     *
     * Density is approximately 0.1%.
     */
    for (int i = 0;
         i < constraints;
         ++i) {

        problem.set_constraint_coefficient(
            i,
            i,
            1.0
        );

        problem.set_objective_coefficient(
            i,
            1.0
        );
    }

    return problem;
}

dent::Problem make_qp() {
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

    const int c =
        problem.add_constraint(
            "capacity",
            dent::ConstraintSense::LessEqual,
            10.0
        );

    problem.set_objective_coefficient(
        x1,
        1.0
    );

    problem.set_objective_coefficient(
        x2,
        1.0
    );

    problem.set_quadratic_coefficient(
        x1,
        x1,
        1.0
    );

    problem.set_quadratic_coefficient(
        x2,
        x2,
        1.0
    );

    problem.set_constraint_coefficient(
        c,
        x1,
        1.0
    );

    problem.set_constraint_coefficient(
        c,
        x2,
        1.0
    );

    return problem;
}

dent::Problem make_milp() {
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

    const int c =
        problem.add_constraint(
            "capacity",
            dent::ConstraintSense::LessEqual,
            10.0
        );

    problem.set_objective_coefficient(
        x,
        5.0
    );

    problem.set_constraint_coefficient(
        c,
        x,
        1.0
    );

    return problem;
}

} // namespace

int main() {
    std::cout
        << "DENT ADAPTIVE DISPATCHER REGRESSION SUITE\n\n";

    dent::AdaptiveDispatcher dispatcher;

    /*
     * Small LP -> Simplex.
     */
    {
        dent::Problem problem =
            make_small_lp();

        const dent::ProblemFingerprint fingerprint =
            dent::fingerprint_problem(problem);

        const dent::DispatchDecision decision =
            dispatcher.dispatch(
                problem,
                fingerprint
            );

        check(
            decision.method ==
                dent::SolverMethod::PrimalSimplex,
            "Small LP -> Primal Simplex"
        );

        check(
            decision.use_cpu &&
            !decision.use_gpu,
            "Small LP -> CPU execution"
        );
    }

    /*
     * Large sparse LP -> IPM.
     */
    {
        dent::Problem problem =
            make_large_sparse_lp();

        const dent::ProblemFingerprint fingerprint =
            dent::fingerprint_problem(problem);

        const dent::DispatchDecision decision =
            dispatcher.dispatch(
                problem,
                fingerprint
            );

        check(
            decision.method ==
                dent::SolverMethod::InteriorPoint,
            "Large sparse LP -> Interior Point"
        );

        check(
            decision.solver_name ==
                "Interior Point",
            "IPM solver name reported"
        );

        check(
            decision.use_cpu &&
            !decision.use_gpu,
            "IPM -> CPU execution"
        );
    }

    /*
     * QP -> QP solver.
     */
    {
        dent::Problem problem =
            make_qp();

        const dent::ProblemFingerprint fingerprint =
            dent::fingerprint_problem(problem);

        const dent::DispatchDecision decision =
            dispatcher.dispatch(
                problem,
                fingerprint
            );

        check(
            decision.method ==
                dent::SolverMethod::QP,
            "Continuous QP -> QP solver"
        );
    }

    /*
     * MILP -> Branch-and-Bound.
     */
    {
        dent::Problem problem =
            make_milp();

        const dent::ProblemFingerprint fingerprint =
            dent::fingerprint_problem(problem);

        const dent::DispatchDecision decision =
            dispatcher.dispatch(
                problem,
                fingerprint
            );

        check(
            decision.method ==
                dent::SolverMethod::MILP,
            "MILP -> Branch-and-Bound"
        );
    }

    /*
     * MIQP -> MIQP branch-and-bound.
     */
    {
        dent::Problem problem =
            make_milp();

        const int x2 =
            problem.add_variable(
                "x2",
                0.0,
                0.0,
                dent::VariableType::Integer
            );

        problem.set_quadratic_coefficient(
            0,
            0,
            1.0
        );

        problem.set_quadratic_coefficient(
            x2,
            x2,
            1.0
        );

        const dent::ProblemFingerprint fingerprint =
            dent::fingerprint_problem(problem);

        const dent::DispatchDecision decision =
            dispatcher.dispatch(
                problem,
                fingerprint
            );

        check(
            decision.method ==
                dent::SolverMethod::MIQP,
            "MIQP -> MIQP Branch-and-Bound"
        );
    }

    /*
     * Verify all solver method names are stable.
     */
    {
        check(
            std::string(
                dent::solver_method_name(
                    dent::SolverMethod::PrimalSimplex
                )
            ) == "Primal Simplex",
            "Primal Simplex method name"
        );

        check(
            std::string(
                dent::solver_method_name(
                    dent::SolverMethod::InteriorPoint
                )
            ) == "Interior Point",
            "Interior Point method name"
        );

        check(
            std::string(
                dent::solver_method_name(
                    dent::SolverMethod::MILP
                )
            ) == "MILP Branch-and-Bound",
            "MILP method name"
        );
    }

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