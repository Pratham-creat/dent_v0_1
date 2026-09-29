#include "dent/model/problem.hpp"
#include "dent/solver/miqp.hpp"
#include <cassert>
#include <cmath>

int main() {
    using namespace dent;
    Problem p(ObjectiveSense::Minimize);
    const int x=p.add_variable("x",0,10,VariableType::Integer);
    p.set_objective_coefficient(x,0.0);
    p.set_quadratic_coefficient(x,x,2.0);
    MIQPSolver solver;
    const auto r=solver.solve(p);
    assert(r.status==SolveStatus::Optimal);
    assert(r.variable_values.size()==1);
    assert(std::abs(r.variable_values[0])<1e-6);
    return 0;
}
