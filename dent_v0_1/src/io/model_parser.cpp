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
            static_cast<unsigned char>(
                value[start]
            )
        )
    )
    {
        ++start;
    }

    std::size_t end = value.size();

    while (
        end > start &&
        std::isspace(
            static_cast<unsigned char>(
                value[end - 1]
            )
        )
    )
    {
        --end;
    }

    return value.substr(
        start,
        end - start
    );
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

        if (
            consumed !=
            value.size()
        )
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


bool is_number_start(
    const std::string& expression,
    std::size_t position
)
{
    if (
        position >=
        expression.size()
    )
    {
        return false;
    }

    const char character =
        expression[position];

    if (
        std::isdigit(
            static_cast<unsigned char>(
                character
            )
        )
    )
    {
        return true;
    }

    return character == '.';
}


std::string parse_variable_name(
    const std::string& expression,
    std::size_t& position
)
{
    const std::size_t start =
        position;

    while (
        position <
        expression.size()
    )
    {
        const char character =
            expression[position];

        if (
            std::isalnum(
                static_cast<unsigned char>(
                    character
                )
            ) ||
            character == '_' ||
            character == '.'
        )
        {
            ++position;
        }
        else
        {
            break;
        }
    }

    if (
        position ==
        start
    )
    {
        throw std::runtime_error(
            "Expected a variable name."
        );
    }

    return expression.substr(
        start,
        position - start
    );
}


double parse_coefficient(
    const std::string& expression,
    std::size_t& position,
    bool& explicitly_present
)
{
    explicitly_present = false;

    if (
        position >=
        expression.size() ||
        !is_number_start(
            expression,
            position
        )
    )
    {
        return 1.0;
    }

    const std::size_t start =
        position;

    bool has_digits = false;

    while (
        position <
        expression.size() &&
        std::isdigit(
            static_cast<unsigned char>(
                expression[position]
            )
        )
    )
    {
        has_digits = true;

        ++position;
    }

    if (
        position <
        expression.size() &&
        expression[position] == '.'
    )
    {
        ++position;

        while (
            position <
            expression.size() &&
            std::isdigit(
                static_cast<unsigned char>(
                    expression[position]
                )
            )
        )
        {
            has_digits = true;

            ++position;
        }
    }

    if (!has_digits)
    {
        position = start;

        return 1.0;
    }

    /*
        Scientific notation:

            1e3
            2.5e-4
            3E+2
    */

    if (
        position <
        expression.size() &&
        (
            expression[position] == 'e' ||
            expression[position] == 'E'
        )
    )
    {
        const std::size_t
            exponent_start =
                position;

        ++position;

        if (
            position <
            expression.size() &&
            (
                expression[position] == '+' ||
                expression[position] == '-'
            )
        )
        {
            ++position;
        }

        const std::size_t
            exponent_digits =
                position;

        while (
            position <
            expression.size() &&
            std::isdigit(
                static_cast<unsigned char>(
                    expression[position]
                )
            )
        )
        {
            ++position;
        }

        if (
            position ==
            exponent_digits
        )
        {
            position =
                exponent_start;
        }
    }

    const std::string number =
        expression.substr(
            start,
            position - start
        );

    explicitly_present = true;

    return parse_double(
        number,
        "linear expression"
    );
}


/*
    Parse a linear expression.

    Supported forms:

        x
        -x
        +x

        2x
        -2x
        2*x
        -2*x

        2 x
        -2 x

        2.5x
        1e3x
        2.5e-3x

        x + y
        x - y
        2x + 3y
        2*x - 3*y

    The function deliberately does NOT combine duplicate
    terms here. That is handled later by the model-building
    stage so the parser preserves every term exactly as it
    appeared in the source model.
*/
std::vector<
    std::pair<std::string, double>
>
parse_linear_expression(
    const std::string& input,
    const std::string& context
)
{
    std::vector<
        std::pair<std::string, double>
    > terms;

    const std::string expression =
        trim(input);

    if (
        expression.empty()
    )
    {
        throw std::runtime_error(
            "Empty expression in " +
            context +
            "."
        );
    }

    std::size_t position = 0;

    while (
        position <
        expression.size()
    )
    {
        /*
            Skip whitespace.
        */

        while (
            position <
            expression.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    expression[position]
                )
            )
        )
        {
            ++position;
        }

        if (
            position >=
            expression.size()
        )
        {
            break;
        }

        /*
            Read term sign.
        */

        double sign = 1.0;

        if (
            expression[position] ==
            '+'
        )
        {
            ++position;
        }
        else if (
            expression[position] ==
            '-'
        )
        {
            sign = -1.0;

            ++position;
        }

        /*
            Skip whitespace after sign.
        */

        while (
            position <
            expression.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    expression[position]
                )
            )
        )
        {
            ++position;
        }

        if (
            position >=
            expression.size()
        )
        {
            throw std::runtime_error(
                "Expression in " +
                context +
                " ends after a sign."
            );
        }

        /*
            Parse optional coefficient.
        */

        bool coefficient_present =
            false;

        double coefficient =
            parse_coefficient(
                expression,
                position,
                coefficient_present
            );

        /*
            Skip whitespace between coefficient
            and multiplication / variable.
        */

        while (
            position <
            expression.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    expression[position]
                )
            )
        )
        {
            ++position;
        }

        /*
            Optional multiplication symbol.
        */

        if (
            position <
            expression.size() &&
            expression[position] ==
            '*'
        )
        {
            ++position;

            while (
                position <
                expression.size() &&
                std::isspace(
                    static_cast<unsigned char>(
                        expression[position]
                    )
                )
            )
            {
                ++position;
            }
        }

        /*
            A variable must begin with a letter
            or underscore.
        */

        if (
            position >=
            expression.size() ||
            !(
                std::isalpha(
                    static_cast<unsigned char>(
                        expression[position]
                    )
                ) ||
                expression[position] == '_'
            )
        )
        {
            throw std::runtime_error(
                "Expected variable name in " +
                context +
                " near '" +
                expression.substr(
                    position
                ) +
                "'."
            );
        }

        const std::string variable =
            parse_variable_name(
                expression,
                position
            );

        terms.emplace_back(
            variable,
            sign * coefficient
        );

        /*
            Skip whitespace before next term.
        */

        while (
            position <
            expression.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    expression[position]
                )
            )
        )
        {
            ++position;
        }

        /*
            Only +, -, or end-of-expression
            may follow a variable.
        */

        if (
            position <
            expression.size()
        )
        {
            if (
                expression[position] != '+' &&
                expression[position] != '-'
            )
            {
                throw std::runtime_error(
                    "Unexpected token in " +
                    context +
                    " near '" +
                    expression.substr(
                        position
                    ) +
                    "'."
                );
            }
        }
    }

    if (
        terms.empty()
    )
    {
        throw std::runtime_error(
            "Could not parse expression in " +
            context +
            "."
        );
    }

    return terms;
}


ConstraintSense parse_constraint_sense(
    const std::string& sense
)
{
    if (
        sense == "<="
    )
    {
        return ConstraintSense::LessEqual;
    }

    if (
        sense == ">="
    )
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
    std::ifstream input(
        filename
    );

    if (
        !input.is_open()
    )
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


    struct VariableInfo
    {
        std::string name;

        VariableType type =
            VariableType::Continuous;
    };


    struct ConstraintInfo
    {
        ConstraintSense sense =
            ConstraintSense::LessEqual;

        double rhs = 0.0;

        std::vector<
            std::pair<std::string, double>
        > coefficients;
    };


    std::vector<VariableInfo>
        variables;


    std::vector<
        std::pair<std::string, double>
    >
        objective_terms;


    std::vector<ConstraintInfo>
        constraint_data;


    std::string line;

    int line_number = 0;


    /*
        ======================================================
        READ FILE
        ======================================================
    */

    while (
        std::getline(
            input,
            line
        )
    )
    {
        ++line_number;


        /*
            --------------------------------------------------
            REMOVE COMMENTS
            --------------------------------------------------
        */

        const std::size_t
            comment_position =
                line.find('#');


        if (
            comment_position !=
            std::string::npos
        )
        {
            line =
                line.substr(
                    0,
                    comment_position
                );
        }


        line =
            trim(line);


        if (
            line.empty()
        )
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
            continue;
        }


        /*
            --------------------------------------------------
            MAXIMIZE
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


        /*
            --------------------------------------------------
            MINIMIZE
            --------------------------------------------------
        */

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
            lower_line ==
            "subject to" ||
            lower_line ==
            "subject_to" ||
            lower_line ==
            "constraints"
        )
        {
            section =
                Section::Constraints;

            continue;
        }


        /*
            --------------------------------------------------
            INTEGER SECTION
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
            BINARY SECTION
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
            CONTINUOUS SECTION
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
            ==================================================
            OBJECTIVE
            ==================================================
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
            ==================================================
            CONSTRAINT
            ==================================================
        */

        if (
            section ==
            Section::Constraints
        )
        {
            const std::string expression =
                line;


            std::size_t
                operator_position =
                    std::string::npos;


            std::string
                operator_text;


            /*
                Find <= first.
            */

            const std::size_t
                less_equal =
                    expression.find(
                        "<="
                    );


            /*
                Find >=.
            */

            const std::size_t
                greater_equal =
                    expression.find(
                        ">="
                    );


            /*
                Find equality.
            */

            const std::size_t
                equality =
                    expression.find(
                        "="
                    );


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


            /*
                ------------------------------------------------
                LEFT SIDE
                ------------------------------------------------
            */

            std::string
                left_expression =
                    trim(
                        expression.substr(
                            0,
                            operator_position
                        )
                    );


            /*
                ------------------------------------------------
                RIGHT SIDE
                ------------------------------------------------
            */

            const std::string
                right_expression =
                    trim(
                        expression.substr(
                            operator_position +
                            operator_text.size()
                        )
                    );


            /*
                Optional constraint label.

                Example:

                    capacity:
                    2x + 3y <= 100

                becomes:

                    2x + 3y
            */

            const std::size_t
                label_position =
                    left_expression.find(
                        ':'
                    );


            if (
                label_position !=
                std::string::npos
            )
            {
                left_expression =
                    trim(
                        left_expression.substr(
                            label_position + 1
                        )
                    );
            }


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


            /*
                Parse RHS.
            */

            const double rhs =
                parse_double(
                    right_expression,
                    "constraint RHS on line " +
                    std::to_string(
                        line_number
                    )
                );


            /*
                Create constraint data.
            */

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
                Register variables found
                inside this constraint.
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
            ==================================================
            VARIABLE TYPE DECLARATIONS
            ==================================================
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
            std::istringstream stream(
                line
            );


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


        /*
            ==================================================
            UNKNOWN CONTENT
            ==================================================
        */

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
        ======================================================
        VALIDATION
        ======================================================
    */

    if (
        variables.empty()
    )
    {
        throw std::runtime_error(
            "The .dent model contains no variables."
        );
    }


    if (
        objective_terms.empty()
    )
    {
        throw std::runtime_error(
            "The .dent model contains no objective."
        );
    }


    /*
        ======================================================
        CREATE PROBLEM
        ======================================================
    */

    Problem problem(
        objective_sense
    );


    std::unordered_map<
        std::string,
        int
    >
        variable_indices;


    /*
        ======================================================
        ADD VARIABLES
        ======================================================
    */

    for (
        const auto& variable :
        variables
    )
    {
        /*
            Avoid duplicate variable declarations.
        */

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
            Binary variables:

                0 <= x <= 1
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
        ======================================================
        OBJECTIVE
        ======================================================

        IMPORTANT:

        Problem::set_objective_coefficient()
        replaces the coefficient.

        Therefore duplicate terms such as:

            10x + 20x

        must first be accumulated:

            x = 30

        before calling set_objective_coefficient().
    */

    std::unordered_map<
        std::string,
        double
    >
        accumulated_objective;


    for (
        const auto& term :
        objective_terms
    )
    {
        accumulated_objective[
            term.first
        ] += term.second;
    }


    for (
        const auto& entry :
        accumulated_objective
    )
    {
        const auto iterator =
            variable_indices.find(
                entry.first
            );


        if (
            iterator ==
            variable_indices.end()
        )
        {
            throw std::runtime_error(
                "Objective references unknown variable: " +
                entry.first
            );
        }


        problem.set_objective_coefficient(
            iterator->second,
            entry.second
        );
    }


    /*
        ======================================================
        ADD CONSTRAINTS
        ======================================================
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
        ======================================================
        CONSTRAINT COEFFICIENTS
        ======================================================

        IMPORTANT:

        Problem::set_constraint_coefficient()
        replaces the coefficient.

        Therefore:

            2x + 3x <= 10

        must become:

            5x <= 10

        before calling the Problem API.
    */

    for (
        std::size_t i = 0;
        i < constraint_data.size();
        ++i
    )
    {
        std::unordered_map<
            std::string,
            double
        >
            accumulated_coefficients;


        /*
            Accumulate every term belonging
            to this constraint.
        */

        for (
            const auto& term :
            constraint_data[i].coefficients
        )
        {
            accumulated_coefficients[
                term.first
            ] += term.second;
        }


        /*
            Write the accumulated coefficients
            into the Problem.
        */

        for (
            const auto& entry :
            accumulated_coefficients
        )
        {
            const auto iterator =
                variable_indices.find(
                    entry.first
                );


            if (
                iterator ==
                variable_indices.end()
            )
            {
                throw std::runtime_error(
                    "Constraint references unknown variable: " +
                    entry.first
                );
            }


            problem.set_constraint_coefficient(
                static_cast<int>(i),
                iterator->second,
                entry.second
            );
        }
    }


    /*
        ======================================================
        RETURN
        ======================================================
    */

    return problem;
}

} // namespace dent