#pragma once

#include "dent/model/problem.hpp"

#include <string>

namespace dent {

class LPParser {
public:
    static Problem parse_file(const std::string& filename);
};

} // namespace dent
