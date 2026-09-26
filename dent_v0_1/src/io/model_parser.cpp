#include "dent/io/model_parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace dent
{
namespace
{

std::string trim(const std::string& value)
{
    std::size_t start = 0;

    while (
        start < value.size() &&
        std::isspace(
            static_cast<unsigned char>(value[start])
        )
    )
    {
        ++start;
    }

    std::size_t end = value.size();

    while (
        end > start &&
        std::isspace(
            static_cast<unsigned char>(value[end - 1])
        )
    )
    {
        --end;
    }

    return value.substr(start, end - start);
}


std::string lowercase(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    return value;
}


double parse_double(
    const std::string& value,
    const std::string& context
)
{
    try
    {
        std::size_t consumed = 0;

        const double result =
            std::stod(
                value,
                &consumed
            );

        if (consumed != value.size())
        {
            throw std::runtime_error("");
        }

        return result;
    }
    catch (...)
    {
        throw std::runtime_error(
            "Invalid numeric value '" +
            value +
            "' in " +
            context +
            "."
        );
    }
}


/*
    Converts:

        40 ProductA + 30 ProductB

    into:

        (ProductA, 40)
        (ProductB, 30)

    It also supports:

        ProductA
        -ProductA
        2 ProductA
        -2 ProductA
        2*ProductA
        2 ProductA + 3 ProductB
*/
std::vector<
    std::pair<std::string, double>
>
parse_linear_expression(
    std::string expression,
    const std::string& context
)
{
    std::vector<
        std::pair<std::string, double>
    > terms;

    expression = trim(expression);

    if (expression.empty())
    {
        throw std::runtime_error(
            "Empty expression in " +
            context +
            "."
        );
    }

    // Normalize multiplication.
    std::replace(
        expression.begin(),
        expression.end(),
        '*',
        ' '
    );

    /*
        Put spaces around + and - so that:

            2 ProductA + 3 ProductB

        becomes:

            2 ProductA  +  3 ProductB
    */
    std::string normalized;

    for (std::size_t i = 0; i < expression.size(); ++i)
    {
        const char character = expression[i];

        if (
            character == '+' ||
            character == '-'
        )
        {
            /*
                A minus sign at the beginning of a term
                is handled as a sign.
            */
            normalized += ' ';
            normalized += character;
            normalized += ' ';
        }
        else
        {
            normalized += character;
        }
    }

    std::istringstream stream(normalized);

    std::vector<std::string> tokens;

    std::string token;

    while (stream >> token)
    {
        tokens.push_back(token);
    }

    if (tokens.empty())
    {
        throw std::runtime_error(
            "Could not parse expression in " +
            context +
            "."
        );
    }

    double sign = 1.0;

    std::size_t index = 0;

    while (index < tokens.size())
    {
        if (tokens[index] == "+")
        {
            sign = 1.0;
            ++index;
            continue;
        }

        if (tokens[index] == "-")
        {
            sign = -1.0;
            ++index;
            continue;
        }

        double coefficient = 1.0;

        std::string variable;

        /*
            Form:

                40 ProductA

            or:

                ProductA
        */
        try
        {
            std::size_t consumed = 0;

            const double possible_coefficient =
                std::stod(
                    tokens[index],
                    &consumed
                );

            if (
                consumed == tokens[index].size()
            )
            {
                coefficient =
                    possible_coefficient;

                ++index;

                if (index >= tokens.size())
                {
                    throw std::runtime_error(
                        "Missing variable after coefficient in " +
                        context +
                        "."
                    );
                }

                variable =
                    tokens[index];

                ++index;
            }
            else
            {
                variable =
                    tokens[index];

                ++index;
            }
        }
        catch (const std::invalid_argument&)
        {
            variable =
                tokens[index];

            ++index;
        }

        if (
            variable == "+" ||
            variable == "-"
        )
        {
            throw std::runtime_error(
                "Invalid variable in " +
                context +
                "."
            );
        }

        terms.emplace_back(
            variable,
            sign * coefficient
        );

        sign = 1.0;
    }

    return terms;
}


ConstraintSense parse_constraint_sense(
    const std::string& sense
)
{
    if (sense == "<=")
    {
        return ConstraintSense::LessEqual;
    }

    if (sense == ">=")
    {
        return ConstraintSense::GreaterEqual;
    }

    if (
        sense == "=" ||
        sense == "=="
    )
    {
        return ConstraintSense::Equal;
    }

    throw std::runtime_error(
        "Invalid constraint sense '" +
        sense +
        "'."
    );
}


} // namespace


Problem ModelParser::parse_file(
    const std::string& filename
)
{
    std::ifstream input(filename);

    if (!input.is_open())
    {
        throw std::runtime_error(
            "Could not open .dent file: " +
            filename
        );
    }


    ObjectiveSense objective_sense =
        ObjectiveSense::Maximize;


    enum class Section
    {
        None,
        Objective,
        Constraints,
        Integer,
        Binary,
        Continuous
    };


    Section section =
        Section::None;


    std::string problem_name;


    struct VariableInfo
    {
        std::string name;

        VariableType type =
            VariableType::Continuous;
    };


    std::vector<VariableInfo>
        variables;


    std::vector<
        std::pair<std::string, double>
    > objective_terms;


    struct ConstraintInfo
    {
        ConstraintSense sense =
            ConstraintSense::LessEqual;

        double rhs = 0.0;

        std::vector<
            std::pair<std::string, double>
        > coefficients;
    };


    std::vector<ConstraintInfo>
        constraint_data;


    std::string line;

    int line_number = 0;


    while (std::getline(input, line))
    {
        ++line_number;


        // Remove comments.
        const std::size_t
            comment_position =
                line.find('#');

        if (
            comment_position !=
            std::string::npos
        )
        {
            line.erase(
                comment_position
            );
        }


        line = trim(line);


        if (line.empty())
        {
            continue;
        }


        const std::string
            lower_line =
                lowercase(line);


        /*
            --------------------------------------------------
            PROBLEM
            --------------------------------------------------
        */

        if (
            lower_line.rfind(
                "problem:",
                0
            ) == 0
        )
        {
            problem_name =
                trim(
                    line.substr(
                        8
                    )
                );

            continue;
        }


        /*
            --------------------------------------------------
            OBJECTIVE SENSE
            --------------------------------------------------
        */

        if (
            lower_line == "maximize" ||
            lower_line == "max"
        )
        {
            objective_sense =
                ObjectiveSense::Maximize;

            section =
                Section::Objective;

            continue;
        }


        if (
            lower_line == "minimize" ||
            lower_line == "min"
        )
        {
            objective_sense =
                ObjectiveSense::Minimize;

            section =
                Section::Objective;

            continue;
        }


        /*
            --------------------------------------------------
            CONSTRAINT SECTION
            --------------------------------------------------
        */

        if (
            lower_line == "subject to" ||
            lower_line == "subject_to" ||
            lower_line == "constraints"
        )
        {
            section =
                Section::Constraints;

            continue;
        }


        /*
            --------------------------------------------------
            INTEGER VARIABLES
            --------------------------------------------------
        */

        if (
            lower_line == "integer" ||
            lower_line == "integers"
        )
        {
            section =
                Section::Integer;

            continue;
        }


        /*
            --------------------------------------------------
            BINARY VARIABLES
            --------------------------------------------------
        */

        if (
            lower_line == "binary" ||
            lower_line == "binaries"
        )
        {
            section =
                Section::Binary;

            continue;
        }


        /*
            --------------------------------------------------
            CONTINUOUS VARIABLES
            --------------------------------------------------
        */

        if (
            lower_line == "continuous" ||
            lower_line == "continous"
        )
        {
            section =
                Section::Continuous;

            continue;
        }


        /*
            --------------------------------------------------
            OBJECTIVE EXPRESSION
            --------------------------------------------------
        */

        if (
            section ==
            Section::Objective
        )
        {
            const auto parsed_terms =
                parse_linear_expression(
                    line,
                    "objective on line " +
                    std::to_string(
                        line_number
                    )
                );


            for (
                const auto& term :
                parsed_terms
            )
            {
                objective_terms.push_back(
                    term
                );
            }

            continue;
        }


        /*
            --------------------------------------------------
            CONSTRAINT EXPRESSION
            --------------------------------------------------
        */

        if (
            section ==
            Section::Constraints
        )
        {
            std::string expression =
                line;


            ConstraintSense sense;


            std::size_t
                operator_position =
                std::string::npos;


            std::string
                operator_text;


            /*
                Check >= and <= first.
            */

            const std::size_t
                less_equal =
                expression.find("<=");

            const std::size_t
                greater_equal =
                expression.find(">=");

            const std::size_t
                equality =
                expression.find("=");


            if (
                less_equal !=
                std::string::npos
            )
            {
                operator_position =
                    less_equal;

                operator_text =
                    "<=";
            }
            else if (
                greater_equal !=
                std::string::npos
            )
            {
                operator_position =
                    greater_equal;

                operator_text =
                    ">=";
            }
            else if (
                equality !=
                std::string::npos
            )
            {
                operator_position =
                    equality;

                operator_text =
                    "=";
            }
            else
            {
                throw std::runtime_error(
                    "Constraint on line " +
                    std::to_string(
                        line_number
                    ) +
                    " has no <=, >=, or = operator."
                );
            }


            const std::string
                left_expression =
                    trim(
                        expression.substr(
                            0,
                            operator_position
                        )
                    );


            const std::string
                right_expression =
                    trim(
                        expression.substr(
                            operator_position +
                            operator_text.size()
                        )
                    );


            if (
                left_expression.empty() ||
                right_expression.empty()
            )
            {
                throw std::runtime_error(
                    "Invalid constraint on line " +
                    std::to_string(
                        line_number
                    ) +
                    "."
                );
            }


            const double rhs =
                parse_double(
                    right_expression,
                    "constraint RHS on line " +
                    std::to_string(
                        line_number
                    )
                );


            ConstraintInfo constraint;


            constraint.sense =
                parse_constraint_sense(
                    operator_text
                );


            constraint.rhs =
                rhs;


            constraint.coefficients =
                parse_linear_expression(
                    left_expression,
                    "constraint on line " +
                    std::to_string(
                        line_number
                    )
                );


            constraint_data.push_back(
                constraint
            );


            /*
                Register variables discovered
                in the constraint.
            */

            for (
                const auto& term :
                constraint.coefficients
            )
            {
                bool exists = false;

                for (
                    const auto& variable :
                    variables
                )
                {
                    if (
                        variable.name ==
                        term.first
                    )
                    {
                        exists = true;
                        break;
                    }
                }


                if (!exists)
                {
                    VariableInfo variable;

                    variable.name =
                        term.first;

                    variable.type =
                        VariableType::Continuous;

                    variables.push_back(
                        variable
                    );
                }
            }


            continue;
        }


        /*
            --------------------------------------------------
            VARIABLE TYPE DECLARATIONS
            --------------------------------------------------

            Example:

                INTEGER
                    ProductA ProductB

            We allow multiple variables on
            the same line.
        */

        if (
            section ==
            Section::Integer ||
            section ==
            Section::Binary ||
            section ==
            Section::Continuous
        )
        {
            std::istringstream stream(line);

            std::string variable_name;


            while (
                stream >>
                variable_name
            )
            {
                VariableType type =
                    VariableType::Continuous;


                if (
                    section ==
                    Section::Integer
                )
                {
                    type =
                        VariableType::Integer;
                }
                else if (
                    section ==
                    Section::Binary
                )
                {
                    type =
                        VariableType::Binary;
                }


                bool found = false;


                for (
                    auto& variable :
                    variables
                )
                {
                    if (
                        variable.name ==
                        variable_name
                    )
                    {
                        variable.type =
                            type;

                        found = true;

                        break;
                    }
                }


                if (!found)
                {
                    VariableInfo variable;

                    variable.name =
                        variable_name;

                    variable.type =
                        type;

                    variables.push_back(
                        variable
                    );
                }
            }


            continue;
        }


        throw std::runtime_error(
            "Unrecognized content on line " +
            std::to_string(
                line_number
            ) +
            ": " +
            line
        );
    }


    /*
        ------------------------------------------------------
        VALIDATION
        ------------------------------------------------------
    */

    if (variables.empty())
    {
        throw std::runtime_error(
            "The .dent model contains no variables."
        );
    }


    if (objective_terms.empty())
    {
        throw std::runtime_error(
            "The .dent model contains no objective."
        );
    }


    /*
        ------------------------------------------------------
        CREATE PROBLEM
        ------------------------------------------------------
    */

    Problem problem(
        objective_sense
    );


    std::unordered_map<
        std::string,
        int
    > variable_indices;


    /*
        Add variables.
    */

    for (
        const auto& variable :
        variables
    )
    {
        if (
            variable_indices.find(
                variable.name
            ) !=
            variable_indices.end()
        )
        {
            continue;
        }


        double lower_bound =
            0.0;

        double upper_bound =
            0.0;


        /*
            Binary variables naturally have
            bounds [0,1].
        */

        if (
            variable.type ==
            VariableType::Binary
        )
        {
            lower_bound =
                0.0;

            upper_bound =
                1.0;
        }


        const int index =
            problem.add_variable(
                variable.name,
                lower_bound,
                upper_bound,
                variable.type
            );


        variable_indices[
            variable.name
        ] = index;
    }


    /*
        ------------------------------------------------------
        OBJECTIVE
        ------------------------------------------------------
    */

    for (
        const auto& term :
        objective_terms
    )
    {
        const auto iterator =
            variable_indices.find(
                term.first
            );


        if (
            iterator ==
            variable_indices.end()
        )
        {
            throw std::runtime_error(
                "Objective references unknown variable: " +
                term.first
            );
        }


        problem.set_objective_coefficient(
            iterator->second,
            term.second
        );
    }


    /*
        ------------------------------------------------------
        CONSTRAINTS
        ------------------------------------------------------
    */

    for (
        std::size_t i = 0;
        i < constraint_data.size();
        ++i
    )
    {
        const auto& constraint =
            constraint_data[i];


        const std::string
            constraint_name =
                "constraint_" +
                std::to_string(
                    i + 1
                );


        problem.add_constraint(
            constraint_name,
            constraint.sense,
            constraint.rhs
        );
    }


    /*
        Add constraint coefficients.
    */

    for (
        std::size_t i = 0;
        i < constraint_data.size();
        ++i
    )
    {
        for (
            const auto& term :
            constraint_data[i].coefficients
        )
        {
            const auto iterator =
                variable_indices.find(
                    term.first
                );


            if (
                iterator ==
                variable_indices.end()
            )
            {
                throw std::runtime_error(
                    "Constraint references unknown variable: " +
                    term.first
                );
            }


            problem.set_constraint_coefficient(
                static_cast<int>(i),
                iterator->second,
                term.second
            );
        }
    }


    return problem;
}

} // namespace dent