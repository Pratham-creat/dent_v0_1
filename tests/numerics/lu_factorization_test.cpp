#include "dent/numerics/lu_factorization.hpp"
#include <cassert>
#include <cmath>
int main(){
    dent::LUFactorization lu;
    std::vector<std::vector<double>> a={{4,3},{6,3}};
    assert(lu.factorize(a));
    std::vector<double>x;
    assert(lu.solve({10,12},x));
    assert(std::abs(x[0]-1.0)<1e-9 && std::abs(x[1]-2.0)<1e-9);
    return 0;
}
