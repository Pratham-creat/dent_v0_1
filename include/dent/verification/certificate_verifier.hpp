#pragma once

#include "dent/model/problem.hpp"

#include <string>
#include <vector>

namespace dent {

enum class CertificateType { PrimalFeasibility, FarkasInfeasibility, UnboundedRay };

struct CertificateVerification {
    bool valid = false;
    CertificateType type = CertificateType::PrimalFeasibility;
    double max_residual = 0.0;
    double objective_direction = 0.0;
    std::string message;
};

class CertificateVerifier {
public:
    explicit CertificateVerifier(double tolerance = 1e-7) : tolerance_(tolerance) {}

    CertificateVerification verify_primal(
        const Problem& problem,
        const std::vector<double>& x) const;

    CertificateVerification verify_farkas(
        const Problem& problem,
        const std::vector<double>& y) const;

    CertificateVerification verify_unbounded_ray(
        const Problem& problem,
        const std::vector<double>& ray) const;

private:
    double tolerance_;
};

} // namespace dent
