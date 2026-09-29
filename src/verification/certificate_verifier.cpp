#include "dent/verification/certificate_verifier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dent {
namespace {

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    double s=0.0; for (std::size_t i=0;i<a.size() && i<b.size();++i) s+=a[i]*b[i]; return s;
}

} // namespace

CertificateVerification CertificateVerifier::verify_primal(
    const Problem& p, const std::vector<double>& x) const
{
    CertificateVerification r; r.type=CertificateType::PrimalFeasibility;
    if (x.size()!=p.variables().size()) { r.message="Primal certificate dimension mismatch."; return r; }
    double maxv=0.0;
    for (std::size_t j=0;j<x.size();++j) {
        const auto& v=p.variables()[j];
        maxv=std::max(maxv,std::max(0.0,v.lower_bound-x[j]));
        if (v.upper_bound!=0.0) maxv=std::max(maxv,std::max(0.0,x[j]-v.upper_bound));
        if (v.type==VariableType::Binary) maxv=std::max(maxv,std::min(std::abs(x[j]),std::abs(x[j]-1.0)));
        if (v.type==VariableType::Integer) maxv=std::max(maxv,std::abs(x[j]-std::round(x[j])));
    }
    for (std::size_t i=0;i<p.constraints().size();++i) {
        double lhs=0.0; for (std::size_t j=0;j<x.size();++j) lhs+=p.matrix()[i][j]*x[j];
        const auto& c=p.constraints()[i];
        double v=0.0;
        if(c.sense==ConstraintSense::LessEqual) v=std::max(0.0,lhs-c.rhs);
        else if(c.sense==ConstraintSense::GreaterEqual) v=std::max(0.0,c.rhs-lhs);
        else v=std::abs(lhs-c.rhs);
        maxv=std::max(maxv,v);
    }
    r.max_residual=maxv; r.valid=maxv<=tolerance_; r.message=r.valid?"Primal solution verified.":"Primal feasibility violation detected."; return r;
}

CertificateVerification CertificateVerifier::verify_unbounded_ray(
    const Problem& p, const std::vector<double>& d) const
{
    CertificateVerification r; r.type=CertificateType::UnboundedRay;
    if(d.size()!=p.variables().size()) { r.message="Ray dimension mismatch."; return r; }
    double norm=0.0; for(double v:d) norm+=v*v;
    if(norm<=tolerance_*tolerance_) { r.message="Unbounded ray must be nonzero."; return r; }
    double maxv=0.0;
    for(std::size_t i=0;i<p.constraints().size();++i){
        double lhs=0.0; for(std::size_t j=0;j<d.size();++j) lhs+=p.matrix()[i][j]*d[j];
        const auto& c=p.constraints()[i];
        if(c.sense==ConstraintSense::LessEqual) maxv=std::max(maxv,lhs);
        else if(c.sense==ConstraintSense::GreaterEqual) maxv=std::max(maxv,-lhs);
        else maxv=std::max(maxv,std::abs(lhs));
    }
    for(std::size_t j=0;j<d.size();++j){
        const auto& v=p.variables()[j];
        if(v.upper_bound!=0.0) maxv=std::max(maxv,d[j]);
        if(v.lower_bound>0.0) maxv=std::max(maxv,-d[j]);
    }
    r.objective_direction=dot(p.objective(),d);
    if(p.objective_sense()==ObjectiveSense::Minimize) r.objective_direction=-r.objective_direction;
    r.max_residual=maxv;
    r.valid=maxv<=tolerance_ && r.objective_direction < -tolerance_;
    r.message=r.valid?"Unbounded ray verified.":"Ray is not a valid improving recession direction.";
    return r;
}

CertificateVerification CertificateVerifier::verify_farkas(
    const Problem& p, const std::vector<double>& y) const
{
    CertificateVerification r; r.type=CertificateType::FarkasInfeasibility;
    if(y.size()!=p.constraints().size()) { r.message="Farkas multiplier dimension mismatch."; return r; }
    // Certificate convention: y_i >= 0 for <= rows and y_i <= 0 for >= rows;
    // equalities are unrestricted. A valid certificate also needs A^T y >= 0
    // and b^T y < 0 after normalizing all rows to <= form.
    std::vector<double> yt(y);
    double sign_violation=0.0;
    for(std::size_t i=0;i<yt.size();++i){
        if(p.constraints()[i].sense==ConstraintSense::LessEqual) sign_violation=std::max(sign_violation,std::max(0.0,-yt[i]));
        else if(p.constraints()[i].sense==ConstraintSense::GreaterEqual) sign_violation=std::max(sign_violation,std::max(0.0,yt[i]));
    }
    double maxat=0.0;
    for(std::size_t j=0;j<p.variables().size();++j){
        double s=0.0;
        for(std::size_t i=0;i<yt.size();++i){
            double a=p.matrix()[i][j];
            if(p.constraints()[i].sense==ConstraintSense::GreaterEqual) a=-a;
            s+=yt[i]*a;
        }
        maxat=std::max(maxat,std::max(0.0,-s));
    }
    double by=0.0;
    for(std::size_t i=0;i<yt.size();++i){
        double b=p.constraints()[i].rhs;
        if(p.constraints()[i].sense==ConstraintSense::GreaterEqual) b=-b;
        by+=yt[i]*b;
    }
    r.max_residual=std::max(sign_violation,maxat);
    r.objective_direction=by;
    r.valid=r.max_residual<=tolerance_ && by < -tolerance_;
    r.message=r.valid?"Farkas infeasibility certificate verified.":"Invalid Farkas certificate.";
    return r;
}

} // namespace dent
