#pragma once

#include "dent/model/problem.hpp"

#include <string>

namespace dent {

class ModelParser {
public:
    static Problem parse_file(
        const std::string& filename
    );

private:
    static void parse_objective(
        Problem& problem,
        const std::string& expression
    );

    static void parse_constraint(
        Problem& problem,
        const std::string& expression,
        int constraint_number
    );

    static void parse_variable_list(
        Problem& problem,
        const std::string& expression,
        VariableType type
    );
};

} // namespace dent