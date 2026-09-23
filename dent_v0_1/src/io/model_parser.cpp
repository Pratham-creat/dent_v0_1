#include "dent/io/model_parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace dent {

namespace {

std::string trim(const std::string& text)
{
    const auto first =
        text.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const auto last =
        text.find_last_not_of(" \t\r\n");

    return text.substr(
        first,
        last - first + 1
    );
}

std::string upper(std::string text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::toupper(c)
            );
        }
    );

    return text;
}

std::string normalize_expression(std::string text)
{
    text = trim(text);

    // Convert '-' into '+-' so that terms can be split on '+'.
    std::string result;

    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        if (c == '-') {
            result += "+-";
        } else {
            result += c;
        }
    }

    return result;
}

struct ParsedTerm {
    double coefficient;
    std::string variable;
};

std::vector<ParsedTerm> parse_terms(
    const std::string& expression
)
{
    std::vector<ParsedTerm> terms;

    std::string normalized =
        normalize_expression(expression);

    std::stringstream stream(normalized);
    std::string token;

    while (std::getline(stream, token, '+')) {

        token = trim(token);

        if (token.empty()) {
            continue;
        }

        // Remove spaces inside a term.
        token.erase(
            std::remove_if(
                token.begin(),
                token.end(),
                [](unsigned char c) {
                    return std::isspace(c);
                }
            ),
            token.end()
        );

        std::size_t variable_position =
            std::string::npos;

        for (std::size_t i = 0;
             i < token.size();
             ++i) {

            if (std::isalpha(
                    static_cast<unsigned char>(token[i]))) {

                variable_position = i;
                break;
            }
        }

        if (variable_position == std::string::npos) {
            throw std::runtime_error(
                "Invalid term: " + token
            );
        }

        std::string coefficient_text =
            token.substr(
                0,
                variable_position
            );

        std::string variable =
            token.substr(variable_position);

        double coefficient = 1.0;

        if (!coefficient_text.empty()) {

            if (coefficient_text == "-") {
                coefficient = -1.0;
            } else {
                coefficient =
                    std::stod(coefficient_text);
            }
        }

        terms.push_back({
            coefficient,
            variable
        });
    }

    return terms;
}

int find_variable(
    const Problem& problem,
    const std::string& name
)
{
    const auto& variables =
        problem.variables();

    for (int i = 0;
         i < static_cast<int>(variables.size());
         ++i) {

        if (variables[i].name == name) {
            return i;
        }
    }

    return -1;
}

int get_or_create_variable(
    Problem& problem,
    const std::string& name
)
{
    int index =
        find_variable(problem, name);

    if (index >= 0) {
        return index;
    }

    return problem.add_variable(
        name,
        0.0,
        0.0,
        VariableType::Continuous
    );
}

} // anonymous namespace


Problem ModelParser::parse_file(
    const std::string& filename
)
{
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Could not open model file: " + filename
        );
    }

    std::vector<std::string> lines;

    std::string line;

    while (std::getline(file, line)) {

        line = trim(line);

        if (line.empty()) {
            continue;
        }

        // Ignore comments.
        if (line[0] == '#') {
            continue;
        }

        lines.push_back(line);
    }

    if (lines.empty()) {
        throw std::runtime_error(
            "Model file is empty."
        );
    }

    ObjectiveSense sense =
        ObjectiveSense::Maximize;

    bool objective_found = false;

    Problem problem(sense);

    enum class Section {
        None,
        Objective,
        Constraints,
        Integer,
        Binary,
        Continuous
    };

    Section section = Section::None;

    int constraint_number = 1;

    for (const std::string& original_line : lines) {

        std::string line = trim(original_line);
        std::string upper_line = upper(line);

        if (upper_line.rfind("PROBLEM:", 0) == 0) {
            continue;
        }

        if (upper_line == "MAXIMIZE") {
            problem = Problem(
                ObjectiveSense::Maximize
            );
            section = Section::Objective;
            continue;
        }

        if (upper_line == "MINIMIZE") {
            problem = Problem(
                ObjectiveSense::Minimize
            );
            section = Section::Objective;
            continue;
        }

        if (upper_line == "SUBJECT TO" ||
            upper_line == "SUCH THAT" ||
            upper_line == "CONSTRAINTS") {

            section = Section::Constraints;
            continue;
        }

        if (upper_line == "INTEGER") {
            section = Section::Integer;
            continue;
        }

        if (upper_line == "BINARY") {
            section = Section::Binary;
            continue;
        }

        if (upper_line == "CONTINUOUS") {
            section = Section::Continuous;
            continue;
        }

        switch (section) {

        case Section::Objective:
            parse_objective(
                problem,
                line
            );
            objective_found = true;
            break;

        case Section::Constraints:
            parse_constraint(
                problem,
                line,
                constraint_number++
            );
            break;

        case Section::Integer:
            parse_variable_list(
                problem,
                line,
                VariableType::Integer
            );
            break;

        case Section::Binary:
            parse_variable_list(
                problem,
                line,
                VariableType::Binary
            );
            break;

        case Section::Continuous:
            parse_variable_list(
                problem,
                line,
                VariableType::Continuous
            );
            break;

        default:
            throw std::runtime_error(
                "Unexpected line: " + line
            );
        }
    }

    if (!objective_found) {
        throw std::runtime_error(
            "No objective function found."
        );
    }

    return problem;
}


void ModelParser::parse_objective(
    Problem& problem,
    const std::string& expression
)
{
    const auto terms =
        parse_terms(expression);

    for (const auto& term : terms) {

        int variable =
            get_or_create_variable(
                problem,
                term.variable
            );

        problem.set_objective_coefficient(
            variable,
            term.coefficient
        );
    }
}


void ModelParser::parse_constraint(
    Problem& problem,
    const std::string& expression,
    int constraint_number
)
{
    ConstraintSense sense;

    std::size_t operator_position =
        std::string::npos;

    std::string operator_text;

    const std::vector<std::string> operators = {
        "<=",
        ">=",
        "="
    };

    for (const auto& op : operators) {

        std::size_t position =
            expression.find(op);

        if (position != std::string::npos) {
            operator_position = position;
            operator_text = op;
            break;
        }
    }

    if (operator_position == std::string::npos) {
        throw std::runtime_error(
            "Constraint has no valid operator: " +
            expression
        );
    }

    if (operator_text == "<=") {
        sense = ConstraintSense::LessEqual;
    } else if (operator_text == ">=") {
        sense = ConstraintSense::GreaterEqual;
    } else {
        sense = ConstraintSense::Equal;
    }

    std::string left =
        trim(
            expression.substr(
                0,
                operator_position
            )
        );

    std::string right =
        trim(
            expression.substr(
                operator_position +
                operator_text.size()
            )
        );

    double rhs = std::stod(right);

    std::string name =
        "Constraint" +
        std::to_string(constraint_number);

    int constraint =
        problem.add_constraint(
            name,
            sense,
            rhs
        );

    const auto terms =
        parse_terms(left);

    for (const auto& term : terms) {

        int variable =
            get_or_create_variable(
                problem,
                term.variable
            );

        problem.set_constraint_coefficient(
            constraint,
            variable,
            term.coefficient
        );
    }
}


void ModelParser::parse_variable_list(
    Problem& problem,
    const std::string& expression,
    VariableType type
)
{
    std::stringstream stream(expression);

    std::string variable_name;

    while (stream >> variable_name) {

        int variable =
            find_variable(
                problem,
                variable_name
            );

        if (variable < 0) {
            variable =
                problem.add_variable(
                    variable_name,
                    0.0,
                    type == VariableType::Binary
                        ? 1.0
                        : 0.0,
                    type
                );
        }

        // Binary variables have bounds [0,1].
        if (type == VariableType::Binary) {
            auto& variables =
                const_cast<
                    std::vector<Variable>&
                >(problem.variables());

            variables[variable].lower_bound = 0.0;
            variables[variable].upper_bound = 1.0;
            variables[variable].type =
                VariableType::Binary;
        }
        else {
            auto& variables =
                const_cast<
                    std::vector<Variable>&
                >(problem.variables());

            variables[variable].type = type;
        }
    }
}

} // namespace dent