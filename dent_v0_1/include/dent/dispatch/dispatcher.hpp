#pragma once

#include "dent/dispatch/fingerprint.hpp"

#include <string>

namespace dent {

class Problem;

enum class SolverMethod {
    PrimalSimplex,
    DualSimplex,
    InteriorPoint,
    PDHG,
    PDLP,
    QP,
    MILP,
    Unsupported
};

struct DispatchDecision {
    SolverMethod method = SolverMethod::Unsupported;

    std::string solver_name;
    std::string reason;

    bool use_cpu = true;
    bool use_gpu = false;
};

class AdaptiveDispatcher {
public:
    AdaptiveDispatcher() = default;

    DispatchDecision dispatch(
        const Problem& problem,
        const ProblemFingerprint& fingerprint
    ) const;

private:
    DispatchDecision dispatch_linear_program(
        const ProblemFingerprint& fingerprint
    ) const;

    DispatchDecision dispatch_quadratic_program(
        const ProblemFingerprint& fingerprint
    ) const;

    DispatchDecision dispatch_mixed_integer(
        const ProblemFingerprint& fingerprint
    ) const;
};

const char* solver_method_name(
    SolverMethod method
);

} // namespace dent