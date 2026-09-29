#include "dent/numerics/forrest_tomlin.hpp"
#include "dent/solver/simplex_pivot.hpp"
#include <cassert>
#include <cmath>
int main(){
 dent::ForrestTomlin ft; assert(ft.factorize({{4,1},{2,3}}));
 std::vector<double>x; assert(ft.solve({9,11},x)); assert(std::abs(x[0]-1.6)<1e-8 && std::abs(x[1]-2.6)<1e-8);
 assert(ft.update_eta(0,{2,1})); assert(ft.updates()==1);
 assert(dent::choose_harris_leaving({1,2},{1,2})>=0);
 std::vector<double> rc={-1,-.1,0}; std::vector<std::vector<double>> rows={{1,0,1},{0,1,1}}; assert(dent::choose_steepest_edge_entering(rc,rows)>=0);
 dent::apply_objective_perturbation(rc); return 0;
}