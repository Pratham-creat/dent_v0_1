#include "dent/io/mps_parser.hpp"
#include <cmath>
#include <fstream>
#include <iostream>

int main() {
    const char* file="mps_parser_regression.mps";
    std::ofstream out(file);
    out<<R"(NAME TESTLP
OBJSENSE
 MIN
ROWS
 N COST
 L CAP
 G DEM
COLUMNS
 X1 COST 3 CAP 1
 X1 DEM 1
 X2 COST 5 CAP 2
 X2 DEM 1
RHS
 RHS1 CAP 10 DEM 4
RANGES
 RNG1 CAP 2
BOUNDS
 LO BND1 X1 0
 UP BND1 X1 6
 BV BND1 X2
ENDATA
)";
    out.close();
    dent::Problem p=dent::MPSParser::parse_file(file);
    bool ok=p.variables().size()==2 && p.constraints().size()==3 &&
      p.objective_sense()==dent::ObjectiveSense::Minimize &&
      std::abs(p.objective()[0]-3)<1e-12 && std::abs(p.objective()[1]-5)<1e-12 &&
      p.variables()[1].type==dent::VariableType::Binary && p.variables()[0].upper_bound==6 &&
      p.constraints()[0].sense==dent::ConstraintSense::LessEqual &&
      p.constraints()[1].sense==dent::ConstraintSense::GreaterEqual;
    std::cout<<(ok?"[PASS] MPS parser regression\n":"[FAIL] MPS parser regression\n");
    return ok?0:1;
}
