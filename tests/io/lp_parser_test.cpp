#include "dent/io/lp_parser.hpp"
#include <cassert>
#include <fstream>

int main() {
    const char* path="lp_parser_regression.lp";
    { std::ofstream f(path);
      f<<"Maximize\n obj: 3 x + 2 y\nSubject To\n c1: x + y <= 4\nBounds\n 0 <= x <= 10\nBinary\n y\nEnd\n";
    }
    auto p=dent::LPParser::parse_file(path);
    assert(p.variables().size()==2);
    assert(p.constraints().size()==1);
    assert(p.objective_sense()==dent::ObjectiveSense::Maximize);
    assert(p.variables()[1].type==dent::VariableType::Binary);
    assert(p.variables()[0].upper_bound==10.0);
    std::remove(path);
    return 0;
}
