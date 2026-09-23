#include "dent/io/interactive_builder.hpp"

#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <cmath>

namespace dent {

namespace {

void clear_input()
{
    std::cin.clear();

    std::cin.ignore(
        std::numeric_limits<std::streamsize>::max(),
        '\n'
    );
}

int read_choice(
    const std::string& prompt,
    int minimum,
    int maximum
)
{
    int value;

    while (true) {

        std::cout << prompt;

        if (std::cin >> value &&
            value >= minimum &&
            value <= maximum) {

            clear_input();
            return value;
        }

        std::cout
            << "Please enter a number from "
            << minimum
            << " to "
            << maximum
            << ".\n";

        clear_input();
    }
}

int read_non_negative_int(
    const std::string& prompt
)
{
    int value;

    while (true) {

        std::cout << prompt;

        if (std::cin >> value &&
            value >= 0) {

            clear_input();
            return value;
        }

        std::cout
            << "Please enter a non-negative "
            << "whole number.\n";

        clear_input();
    }
}

double read_double(
    const std::string& prompt
)
{
    double value;

    while (true) {

        std::cout << prompt;

        if (std::cin >> value) {

            clear_input();
            return value;
        }

        std::cout
            << "Please enter a valid number.\n";

        clear_input();
    }
}

std::string read_text(
    const std::string& prompt
)
{
    std::string value;

    while (true) {

        std::cout << prompt;

        std::getline(
            std::cin,
            value
        );

        if (!value.empty()) {
            return value;
        }

        std::cout
            << "This field cannot be empty.\n";
    }
}

bool read_yes_no(
    const std::string& prompt
)
{
    int choice =
        read_choice(
            prompt,
            1,
            2
        );

    return choice == 1;
}

VariableType read_decision_type()
{
    std::cout
        << "\nHow can this quantity be decided?\n"
        << "  1. Any amount\n"
        << "     Example: 12.5 units\n"
        << "  2. Whole units only\n"
        << "     Example: 12 units\n"
        << "  3. Yes / No decision\n";

    int choice =
        read_choice(
            "Enter choice: ",
            1,
            3
        );

    if (choice == 1) {
        return VariableType::Continuous;
    }

    if (choice == 2) {
        return VariableType::Integer;
    }

    return VariableType::Binary;
}

ConstraintSense read_constraint_sense()
{
    std::cout
        << "\nWhat does this requirement mean?\n"
        << "  1. Cannot exceed the available amount\n"
        << "  2. Must be exactly this amount\n"
        << "  3. Must be at least this amount\n";

    int choice =
        read_choice(
            "Enter choice: ",
            1,
            3
        );

    if (choice == 1) {
        return ConstraintSense::LessEqual;
    }

    if (choice == 2) {
        return ConstraintSense::Equal;
    }

    return ConstraintSense::GreaterEqual;
}

} // anonymous namespace


Problem InteractiveBuilder::build()
{
    std::cout
        << "\n========================================\n"
        << "       DENT OPTIMIZATION ENGINE\n"
        << "========================================\n";

    std::cout
        << "\nDENT converts real-world planning problems\n"
        << "into optimization models automatically.\n"
        << "\n"
        << "You do NOT need to write equations.\n"
        << "Simply provide your business information.\n";


    // =====================================================
    // Problem name
    // =====================================================

    std::string problem_name =
        read_text(
            "\nWhat should we call this problem? "
        );


    // =====================================================
    // Objective
    // =====================================================

    std::cout
        << "\n========================================\n"
        << "             YOUR OBJECTIVE\n"
        << "========================================\n";

    std::cout
        << "\nWhat are you trying to achieve?\n"
        << "  1. Maximize profit / output\n"
        << "  2. Minimize cost / resource usage\n";

    int objective_choice =
        read_choice(
            "Enter choice: ",
            1,
            2
        );

    ObjectiveSense objective_sense;

    if (objective_choice == 1) {
        objective_sense =
            ObjectiveSense::Maximize;
    }
    else {
        objective_sense =
            ObjectiveSense::Minimize;
    }

    Problem problem(objective_sense);


    // =====================================================
    // Decisions / Products
    // =====================================================

    std::cout
        << "\n========================================\n"
        << "          WHAT ARE YOU DECIDING?\n"
        << "========================================\n";

    std::cout
        << "\nExamples:\n"
        << "  Petrol production\n"
        << "  Diesel production\n"
        << "  Number of trucks\n"
        << "  Number of workers\n"
        << "  Amount shipped\n"
        << "  Raw material purchased\n";

    int variable_count =
        read_non_negative_int(
            "\nNumber of decisions/products: "
        );

    while (variable_count == 0) {

        std::cout
            << "At least one decision is required.\n";

        variable_count =
            read_non_negative_int(
                "Number of decisions/products: "
            );
    }

    std::vector<int> variable_indices;

    for (int i = 0;
         i < variable_count;
         ++i) {

        std::cout
            << "\nDecision "
            << i + 1
            << "\n";

        std::string name =
            read_text(
                "What are you deciding? "
            );

        int index =
            problem.add_variable(
                name,
                0.0,
                0.0,
                VariableType::Continuous
            );

        variable_indices.push_back(index);
    }


    // =====================================================
    // Profit / Cost
    // =====================================================

    std::cout
        << "\n========================================\n"
        << "          PROFIT / COST PER UNIT\n"
        << "========================================\n";

    if (objective_choice == 1) {

        std::cout
            << "\nEnter the profit earned from one unit\n"
            << "of each decision/product.\n";
    }
    else {

        std::cout
            << "\nEnter the cost associated with one unit\n"
            << "of each decision/product.\n";
    }

    for (int i = 0;
         i < variable_count;
         ++i) {

        double coefficient =
            read_double(
                "\n" +
                problem.variables()[i].name +
                ": "
            );

        problem.set_objective_coefficient(
            variable_indices[i],
            coefficient
        );
    }


    // =====================================================
    // Nonlinear / changing contribution
    // =====================================================

    std::cout
        << "\n========================================\n"
        << "       CHANGING COST / PROFIT\n"
        << "========================================\n";

    std::cout
        << "\nDoes the cost or profit change as\n"
        << "the quantity increases?\n"
        << "\n"
        << "Examples:\n"
        << "  More production becomes increasingly expensive.\n"
        << "  More production gives a changing return.\n"
        << "\n"
        << "  1. Yes\n"
        << "  2. No\n";

    bool has_quadratic =
        read_yes_no(
            "Enter choice: "
        );

    if (has_quadratic) {

        std::cout
            << "\n----------------------------------------\n"
            << "       CHANGING EFFECT BY PRODUCT\n"
            << "----------------------------------------\n";

        std::cout
            << "\nEnter the additional effect for each\n"
            << "decision as its quantity increases.\n"
            << "\n"
            << "Enter 0 if there is no additional effect.\n";

        for (int i = 0;
             i < variable_count;
             ++i) {

            double coefficient =
                read_double(
                    "\nAdditional effect for " +
                    problem.variables()[i].name +
                    ": "
                );

            problem.set_quadratic_coefficient(
                variable_indices[i],
                variable_indices[i],
                2.0 * coefficient
            );
        }

        if (variable_count > 1) {

            std::cout
                << "\n----------------------------------------\n"
                << "          PRODUCT INTERACTIONS\n"
                << "----------------------------------------\n";

            std::cout
                << "\nDoes the interaction between two\n"
                << "decisions affect the objective?\n"
                << "\n"
                << "  1. Yes\n"
                << "  2. No\n";

            bool has_interaction =
                read_yes_no(
                    "Enter choice: "
                );

            if (has_interaction) {

                for (int i = 0;
                     i < variable_count;
                     ++i) {

                    for (int j = i + 1;
                         j < variable_count;
                         ++j) {

                        double coefficient =
                            read_double(
                                "\nInteraction between " +
                                problem.variables()[i].name +
                                " and " +
                                problem.variables()[j].name +
                                ": "
                            );

                        problem.set_quadratic_coefficient(
                            variable_indices[i],
                            variable_indices[j],
                            coefficient
                        );

                        problem.set_quadratic_coefficient(
                            variable_indices[j],
                            variable_indices[i],
                            coefficient
                        );
                    }
                }
            }
        }
    }


    // =====================================================
    // Resources / Requirements
    // =====================================================

    std::cout
        << "\n========================================\n"
        << "       RESOURCES & REQUIREMENTS\n"
        << "========================================\n";

    std::cout
        << "\nTell DENT about anything that limits\n"
        << "or requires your decisions.\n"
        << "\n"
        << "Examples:\n"
        << "  Labour hours\n"
        << "  Machine capacity\n"
        << "  Budget\n"
        << "  Minimum demand\n"
        << "  Storage capacity\n"
        << "  Delivery requirement\n";

    int constraint_count =
        read_non_negative_int(
            "\nNumber of resources/requirements: "
        );

    for (int c = 0;
         c < constraint_count;
         ++c) {

        std::cout
            << "\n----------------------------------------\n"
            << "Requirement "
            << c + 1
            << "\n"
            << "----------------------------------------\n";

        std::string name =
            read_text(
                "What resource or requirement is this? "
            );

        ConstraintSense sense =
            read_constraint_sense();

        double rhs =
            read_double(
                "\nWhat is the available/required amount? "
            );

        int constraint =
            problem.add_constraint(
                name,
                sense,
                rhs
            );

        std::cout
            << "\nNow tell DENT how much of this\n"
            << "resource each decision uses.\n";

        for (int i = 0;
             i < variable_count;
             ++i) {

            double usage =
                read_double(
                    "\n" +
                    problem.variables()[i].name +
                    " uses: "
                );

            problem.set_constraint_coefficient(
                constraint,
                variable_indices[i],
                usage
            );
        }
    }


    // =====================================================
    // Decision rules
    // =====================================================

    std::cout
        << "\n========================================\n"
        << "          DECISION RULES\n"
        << "========================================\n";

    std::cout
        << "\nTell DENT how each decision works.\n";

    bool has_integer =
        false;

    for (int i = 0;
         i < variable_count;
         ++i) {

        std::cout
            << "\n----------------------------------------\n"
            << problem.variables()[i].name
            << "\n"
            << "----------------------------------------\n";

        VariableType type =
            read_decision_type();

        problem.set_variable_type(
            variable_indices[i],
            type
        );

        if (type == VariableType::Integer ||
            type == VariableType::Binary) {

            has_integer = true;
        }

        if (type == VariableType::Binary) {

            std::cout
                << "\nThis will be treated as a Yes/No\n"
                << "decision automatically.\n";
        }
        else {

            double lower =
                read_double(
                    "\nMinimum allowed amount "
                    "(0 if none): "
                );

            double upper =
                read_double(
                    "Maximum allowed amount "
                    "(0 if no maximum): "
                );

            while (upper != 0.0 &&
                   upper < lower) {

                std::cout
                    << "\nMaximum cannot be smaller "
                    << "than minimum.\n";

                upper =
                    read_double(
                        "Maximum allowed amount: "
                    );
            }

            problem.set_variable_bounds(
                variable_indices[i],
                lower,
                upper
            );
        }
    }


    // =====================================================
    // Detect mathematical problem class
    // =====================================================

    bool has_quadratic_terms =
        false;

    const auto& Q =
        problem.quadratic_matrix();

    for (std::size_t i = 0;
         i < Q.size();
         ++i) {

        for (std::size_t j = 0;
             j < Q[i].size();
             ++j) {

            if (
                std::abs(Q[i][j]) > 1e-12
            ) {

                has_quadratic_terms = true;
                break;
            }
        }

        if (has_quadratic_terms) {
            break;
        }
    }


    std::string detected_type;

    if (has_quadratic_terms) {

        detected_type =
            "Quadratic Programming (QP)";
    }
    else if (has_integer) {

        detected_type =
            "Mixed Integer Linear Programming (MILP)";
    }
    else {

        detected_type =
            "Linear Programming (LP)";
    }


    // =====================================================
    // Human-readable confirmation
    // =====================================================

    std::cout
        << "\n========================================\n"
        << "          PROBLEM UNDERSTOOD\n"
        << "========================================\n";

    std::cout
        << "\nProblem:\n"
        << "  "
        << problem_name
        << "\n";

    std::cout
        << "\nGoal:\n";

    if (objective_choice == 1) {

        std::cout
            << "  Maximize profit / output\n";
    }
    else {

        std::cout
            << "  Minimize cost / resource usage\n";
    }

    std::cout
        << "\nDecisions:\n";

    for (int index :
         variable_indices) {

        const auto& variable =
            problem.variables()[index];

        std::cout
            << "  "
            << variable.name;

        if (variable.type ==
            VariableType::Continuous) {

            std::cout
                << " (any amount)";
        }
        else if (variable.type ==
                 VariableType::Integer) {

            std::cout
                << " (whole units)";
        }
        else {

            std::cout
                << " (yes/no)";
        }

        std::cout << "\n";
    }

    std::cout
        << "\nDENT detected:\n"
        << "  "
        << detected_type
        << "\n";

    std::cout
        << "\nDENT has converted your business\n"
        << "requirements into an optimization model.\n";

    std::cout
        << "\n========================================\n"
        << "             MODEL READY\n"
        << "========================================\n";

    return problem;
}

} // namespace dent