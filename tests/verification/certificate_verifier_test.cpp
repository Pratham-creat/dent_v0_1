#include "dent/verification/certificate_verifier.hpp"
#include <cassert>
int main(){
    using namespace dent;
    Problem p(ObjectiveSense::Maximize);
    int x=p.add_variable("x",0,0);
    int r=p.add_constraint("c",ConstraintSense::LessEqual,4);
    p.set_constraint_coefficient(r,x,1);
    p.set_objective_coefficient(x,1);
    CertificateVerifier v;
    auto primal=v.verify_primal(p,{4});
    assert(primal.valid);
    auto ray=v.verify_unbounded_ray(p,{1});
    assert(!ray.valid);
    return 0;
}
