#include "dent/model/problem.hpp"
#include "dent/solver/simplex.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    using namespace dent;

    std::cout << "DENT Optimization Engine v0.5\n";
    std::cout << "Interactive Linear Programming Solver\n\n";

    int variable_count;
    int constraint_count;

    std::cout << "Enter number of variables: ";
    std::cin >> variable_count;

    std::cout << "Enter number of constraints: ";
    std::cin >> constraint_count;

    if (variable_count <= 0 || constraint_count <= 0)
    {
        std::cerr
            << "\nInvalid number of variables or constraints.\n";

        return 1;
    }

    // --------------------------------------------------
    // Create problem
    // --------------------------------------------------

    std::string objective_input;

    std::cout
        << "Objective sense (max/min): ";

    std::cin >> objective_input;

    ObjectiveSense objective_sense;

    if (objective_input == "min" ||
        objective_input == "MIN" ||
        objective_input == "Min")
    {
        objective_sense = ObjectiveSense::Minimize;
    }
    else
    {
        objective_sense = ObjectiveSense::Maximize;
    }

    Problem problem(objective_sense);

    // --------------------------------------------------
    // Variables
    // --------------------------------------------------

    std::vector<int> variables;

    for (int i = 0;
         i < variable_count;
         ++i)
    {
        variables.push_back(
            problem.add_variable(
                "x" + std::to_string(i + 1)
            )
        );
    }

    // --------------------------------------------------
    // Objective
    // --------------------------------------------------

    std::cout
        << "\nEnter objective coefficients:\n";

    for (int i = 0;
         i < variable_count;
         ++i)
    {
        double coefficient;

        std::cin >> coefficient;

        problem.set_objective_coefficient(
            variables[i],
            coefficient
        );
    }

    // --------------------------------------------------
    // Constraints
    // --------------------------------------------------

    for (int i = 0;
         i < constraint_count;
         ++i)
    {
        std::cout
            << "\nConstraint "
            << i + 1
            << '\n';

        std::string sense_input;

        std::cout
            << "Enter constraint sense (<=, =, >=): ";

        std::cin >> sense_input;

        ConstraintSense constraint_sense;

        if (sense_input == "=")
        {
            constraint_sense =
                ConstraintSense::Equal;
        }
        else if (sense_input == ">=")
        {
            constraint_sense =
                ConstraintSense::GreaterEqual;
        }
        else
        {
            constraint_sense =
                ConstraintSense::LessEqual;
        }

        std::cout
            << "Enter coefficients:\n";

        std::vector<double> coefficients(
            variable_count
        );

        for (int j = 0;
             j < variable_count;
             ++j)
        {
            std::cin >> coefficients[j];
        }

        double rhs;

        std::cout
            << "Enter RHS: ";

        std::cin >> rhs;

        int constraint =
            problem.add_constraint(
                "constraint_" +
                std::to_string(i + 1),
                constraint_sense,
                rhs
            );

        for (int j = 0;
             j < variable_count;
             ++j)
        {
            problem.set_constraint_coefficient(
                constraint,
                variables[j],
                coefficients[j]
            );
        }
    }

    // --------------------------------------------------
    // Display problem
    // --------------------------------------------------

    std::cout
        << "\n----------------------------------------\n";

    std::cout
        << "Problem created successfully.\n";

    std::cout
        << "----------------------------------------\n";

    std::cout
        << "Variables   : "
        << problem.variables().size()
        << '\n';

    std::cout
        << "Constraints : "
        << problem.constraints().size()
        << '\n';

    std::cout
        << "Type        : LP\n";

    std::cout
        << "Objective   : ";

    if (problem.objective_sense() ==
        ObjectiveSense::Maximize)
    {
        std::cout << "Maximize\n";
    }
    else
    {
        std::cout << "Minimize\n";
    }

    std::cout
        << "Constraints :\n";

    for (std::size_t i = 0;
         i < problem.constraints().size();
         ++i)
    {
        const auto& constraint =
            problem.constraints()[i];

        std::cout
            << "  "
            << constraint.name
            << ' ';

        if (constraint.sense ==
            ConstraintSense::LessEqual)
        {
            std::cout << "<=";
        }
        else if (constraint.sense ==
                 ConstraintSense::Equal)
        {
            std::cout << "=";
        }
        else
        {
            std::cout << ">=";
        }

        std::cout
            << ' '
            << constraint.rhs
            << '\n';
    }

    // --------------------------------------------------
    // Sparse representation information
    // --------------------------------------------------

    const SparseMatrix& sparse =
        problem.sparse_matrix();

    std::cout
        << "\nSparse Matrix:\n";

    std::cout
        << "  Rows      : "
        << sparse.rows()
        << '\n';

    std::cout
        << "  Columns   : "
        << sparse.columns()
        << '\n';

    std::cout
        << "  Nonzeros  : "
        << sparse.nonzeros()
        << '\n';

    // --------------------------------------------------
    // Solve
    // --------------------------------------------------

    std::cout
        << "\nSolving...\n\n";

    SimplexSolver solver;

    SolveResult result =
        solver.solve(problem);

    std::cout
        << "Solver:\n";

    std::cout
        << "  Algorithm   : "
        << "Two-Phase Revised Simplex\n";

    std::cout
        << "  Iterations  : "
        << result.iterations
        << "\n\n";

    // --------------------------------------------------
    // Result
    // --------------------------------------------------

    if (result.status ==
        SolveStatus::Optimal)
    {
        std::cout
            << "Status:\n";

        std::cout
            << "  OPTIMAL\n\n";

        std::cout
            << std::fixed
            << std::setprecision(6);

        std::cout
            << "Objective     : "
            << result.objective_value
            << "\n\n";

        std::cout
            << "Variables:\n";

        for (std::size_t i = 0;
             i < result.variable_values.size();
             ++i)
        {
            std::cout
                << "  "
                << problem.variables()[i].name
                << "            : "
                << result.variable_values[i]
                << '\n';
        }

        return 0;
    }

    std::cout
        << "Status:\n";

    switch (result.status)
    {
        case SolveStatus::Infeasible:
            std::cout
                << "  INFEASIBLE\n";
            break;

        case SolveStatus::Unbounded:
            std::cout
                << "  UNBOUNDED\n";
            break;

        case SolveStatus::IterationLimit:
            std::cout
                << "  ITERATION LIMIT\n";
            break;

        default:
            std::cout
                << "  FAILED\n";
            break;
    }

    std::cout
        << "\nMessage       : "
        << result.message
        << '\n';

    return 1;
}