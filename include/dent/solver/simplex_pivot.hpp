#pragma once
#include <vector>
namespace dent {
struct PivotPolicyOptions { double feasibility_tolerance=1e-9; double perturbation=1e-10; };
int choose_harris_leaving(const std::vector<double>& rhs,const std::vector<double>& column,double tolerance=1e-9);
int choose_steepest_edge_entering(const std::vector<double>& reduced_cost,const std::vector<std::vector<double>>& tableau_rows,double tolerance=1e-9);
void apply_objective_perturbation(std::vector<double>& reduced_cost,double epsilon=1e-10);
}
