#pragma once
#include "dent/model/problem.hpp"
#include <string>
#include <vector>
namespace dent {
enum class CutKind { Gomory, MIR, Cover, Conflict };
struct GeneratedCut { CutKind kind; std::string name; ConstraintSense sense; double rhs; std::vector<double> coefficients; };
class MILPCutManager {
public:
    explicit MILPCutManager(double tolerance=1e-9):tolerance_(tolerance){}
    std::vector<GeneratedCut> generate(const Problem&,const std::vector<double>& lp_values) const;
    bool add_all(Problem&,const std::vector<GeneratedCut>&,int* added=nullptr) const;
private:
    double tolerance_;
    GeneratedCut rounded_integer_row(const Problem&,int) const;
    GeneratedCut mir_integer_row(const Problem&,int) const;
    GeneratedCut cover_row(const Problem&,int,const std::vector<double>&) const;
};
}
