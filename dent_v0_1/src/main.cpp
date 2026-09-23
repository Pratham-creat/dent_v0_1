#include "dent/model/problem.hpp"
#include "dent/io/interactive_builder.hpp"

#include "dent/solver/simplex.hpp"
#include "dent/solver/milp.hpp"
#include "dent/solver/qp.hpp"

#include <cmath>
#include <iostream>

namespace dent {

namespace {

bool has_quadratic_terms(
    const Problem& problem
)
{
    const auto& Q =
        problem.quadratic_matrix();

    for (const auto& row : Q) {

        for (double value : row) {

            if (std::abs(value) > 1e-12) {
                return true;
            }
        }
    }

    return false;
}

bool has_integer_variables(
    const Problem& problem
)
{
    for (const auto& variable :
         problem.variables()) {

        if (
            variable.type ==
                VariableType::Integer ||
            variable.type ==
                VariableType::Binary
        ) {
            return true;
        }
    }

    return false;
}

void print_status(
    SolveStatus status
)
{
    switch (status) {

    case SolveStatus::Optimal:
        std::cout << "OPTIMAL";
        break;

    case SolveStatus::Infeasible:
        std::cout << "INFEASIBLE";
        break;

    case SolveStatus::Unbounded:
        std::cout << "UNBOUNDED";
        break;

    case SolveStatus::IterationLimit:
        std::cout << "ITERATION LIMIT";
        break;

    default:
        std::cout << "UNSUPPORTED";
        break;
    }
}

void solve_lp(
    const Problem& problem
)
{
    std::cout
        << "\nModel detected: LP\n"
        << "\nStarting DENT Simplex solver...\n";

    SimplexSolver solver;

    SolveResult result =
        solver.solve(problem);

    std::cout
        << "\n========================================\n"
        << "             SOLUTION\n"
        << "========================================\n";

    std::cout << "\nStatus: ";

    print_status(result.status);

    std::cout << "\n";

    if (result.status == SolveStatus::Optimal) {

        std::cout
            << "\nObjective value: "
            << result.objective_value
            << "\n";

        std::cout
            << "\nDecision values:\n";

        const auto& variables =
            problem.variables();

        for (
            std::size_t i = 0;
            i < variables.size() &&
            i < result.variable_values.size();
            ++i
        ) {

            std::cout
                << "  "
                << variables[i].name
                << " = "
                << result.variable_values[i]
                << "\n";
        }
    }

    if (!result.message.empty()) {

        std::cout
            << "\n"
            << result.message
            << "\n";
    }

    std::cout
        << "\n========================================\n";
}

void solve_milp(
    const Problem& problem
)
{
    std::cout
        << "\nModel detected: MILP\n"
        << "\nStarting DENT Branch and Bound solver...\n";

    MILPSolver solver;

    MILPSolution result =
        solver.solve(problem);

    std::cout
        << "\n========================================\n"
        << "             SOLUTION\n"
        << "========================================\n";

    std::cout << "\nStatus: ";

    print_status(result.status);

    std::cout << "\n";

    if (result.status == SolveStatus::Optimal) {

        std::cout
            << "\nObjective value: "
            << result.objective_value
            << "\n";

        std::cout
            << "\nDecision values:\n";

        const auto& variables =
            problem.variables();

        for (
            std::size_t i = 0;
            i < variables.size() &&
            i < result.variable_values.size();
            ++i
        ) {

            std::cout
                << "  "
                << variables[i].name
                << " = "
                << result.variable_values[i]
                << "\n";
        }
    }

    std::cout
        << "\nNodes explored: "
        << result.nodes_explored
        << "\n";

    std::cout
        << "Nodes pruned: "
        << result.nodes_pruned
        << "\n";

    std::cout
        << "LP relaxations: "
        << result.lp_solves
        << "\n";

    if (!result.message.empty()) {

        std::cout
            << "\n"
            << result.message
            << "\n";
    }

    std::cout
        << "\n========================================\n";
}

void solve_qp(
    const Problem& problem
)
{
    std::cout
        << "\nModel detected: QP\n"
        << "\nStarting DENT QP solver...\n";

    QPSolver solver;

    QPSolution result =
        solver.solve(problem);

    std::cout
        << "\n========================================\n"
        << "             SOLUTION\n"
        << "========================================\n";

    std::cout << "\nStatus: ";

    print_status(result.status);

    std::cout << "\n";

    if (result.status == SolveStatus::Optimal) {

        std::cout
            << "\nObjective value: "
            << result.objective_value
            << "\n";

        std::cout
            << "\nDecision values:\n";

        const auto& variables =
            problem.variables();

        for (
            std::size_t i = 0;
            i < variables.size() &&
            i < result.variable_values.size();
            ++i
        ) {

            std::cout
                << "  "
                << variables[i].name
                << " = "
                << result.variable_values[i]
                << "\n";
        }
    }

    if (!result.message.empty()) {

        std::cout
            << "\n"
            << result.message
            << "\n";
    }

    std::cout
        << "\n========================================\n";
}

void solve_problem(
    const Problem& problem
)
{
    const bool quadratic =
        has_quadratic_terms(problem);

    const bool integer =
        has_integer_variables(problem);

    if (quadratic && integer) {

        std::cout
            << "\nModel detected: MIQP\n"
            << "\nDENT currently supports QP and MILP "
            << "separately.\n"
            << "MIQP is not implemented yet.\n";

        return;
    }

    if (quadratic) {
        solve_qp(problem);
        return;
    }

    if (integer) {
        solve_milp(problem);
        return;
    }

    solve_lp(problem);
}

} // anonymous namespace

} // namespace dent


int main()
{
    using namespace dent;

    try {

        std::cout
            << "\n========================================\n"
            << "       DENT OPTIMIZATION ENGINE\n"
            << "========================================\n";

        std::cout
            << "\nBuild your optimization problem "
            << "using the guided modeling interface.\n";

        Problem problem =
            InteractiveBuilder::build();

        solve_problem(problem);

        return 0;
    }
    catch (const std::exception& error) {

        std::cerr
            << "\nDENT ERROR:\n"
            << error.what()
            << "\n";

        return 1;
    }
}