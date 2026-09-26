#include "dent/dispatch/dispatcher.hpp"

#include "dent/model/problem.hpp"

namespace dent {

const char* solver_method_name(
    SolverMethod method
) {
    switch (method) {
        case SolverMethod::PrimalSimplex:
            return "Primal Simplex";

        case SolverMethod::DualSimplex:
            return "Dual Simplex";

        case SolverMethod::InteriorPoint:
            return "Interior Point";

        case SolverMethod::QP:
            return "QP";

        case SolverMethod::MILP:
            return "MILP Branch-and-Bound";

        case SolverMethod::Unsupported:
        default:
            return "Unsupported";
    }
}

DispatchDecision AdaptiveDispatcher::dispatch(
    const Problem& problem,
    const ProblemFingerprint& fingerprint
) const {
    (void)problem;

    /*
     * Mixed-integer models take the MILP path first.
     *
     * Explicitly mixed-integer models are represented by the
     * corresponding fingerprint flag.
     */
    if (fingerprint.is_mixed_integer) {

        return dispatch_mixed_integer(
            fingerprint
        );
    }

    /*
     * Quadratic continuous models use the QP solver.
     */
    if (fingerprint.has_quadratic_objective) {
        return dispatch_quadratic_program(
            fingerprint
        );
    }

    /*
     * Remaining supported continuous linear models
     * are LPs.
     */
    return dispatch_linear_program(
        fingerprint
    );

    DispatchDecision decision;

    decision.method =
        SolverMethod::Unsupported;

    decision.solver_name =
        solver_method_name(
            decision.method
        );

    decision.reason =
        "Problem structure is not supported "
        "by the current solver portfolio.";

    decision.use_cpu = true;
    decision.use_gpu = false;

    return decision;
}

DispatchDecision AdaptiveDispatcher::dispatch_linear_program(
    const ProblemFingerprint& fingerprint
) const {
    DispatchDecision decision;

    /*
     * Current IPM selection policy:
     *
     * Use IPM when the LP is sufficiently large or
     * strongly sparse.
     *
     * Small/ordinary LPs remain on primal Simplex.
     *
     * This is intentionally conservative. We do not
     * claim IPM is universally faster.
     */

    const bool large =
        fingerprint.large_problem;

    const bool highly_sparse =
        fingerprint.highly_sparse;

    const bool poorly_scaled =
        fingerprint.poorly_scaled;

    if (large && highly_sparse) {

        decision.method =
            SolverMethod::InteriorPoint;

        decision.reason =
            "Large sparse continuous LP; "
            "Interior Point selected for scalable "
            "primal-dual iterations.";

    }
    else if (large) {

        decision.method =
            SolverMethod::InteriorPoint;

        decision.reason =
            "Large continuous LP; "
            "Interior Point selected to avoid "
            "simplex basis-growth on larger models.";

    }
    else if (highly_sparse) {

        decision.method =
            SolverMethod::InteriorPoint;

        decision.reason =
            "Highly sparse continuous LP; "
            "Interior Point selected for sparse "
            "matrix-oriented computation.";

    }
    else if (poorly_scaled) {

        /*
         * Presolve/scaling runs before dispatch.
         *
         * For now we still use IPM for a poorly scaled
         * continuous LP because the IPM path already has
         * regularization in its Newton system.
         */
        decision.method =
            SolverMethod::InteriorPoint;

        decision.reason =
            "Poorly scaled continuous LP; "
            "Interior Point selected after presolve/scaling "
            "for numerical robustness.";

    }
    else {

        decision.method =
            SolverMethod::PrimalSimplex;

        decision.reason =
            "Small or ordinary continuous LP; "
            "Primal Simplex selected for direct basis-based "
            "optimization.";
    }

    decision.solver_name =
        solver_method_name(
            decision.method
        );

    decision.use_cpu = true;

    /*
     * CUDA is intentionally disabled for this stage.
     */
    decision.use_gpu = false;

    return decision;
}

DispatchDecision AdaptiveDispatcher::dispatch_quadratic_program(
    const ProblemFingerprint& fingerprint
) const {
    DispatchDecision decision;

    if (fingerprint.is_mixed_integer) {

        decision.method =
            SolverMethod::Unsupported;

        decision.solver_name =
            solver_method_name(
                decision.method
            );

        decision.reason =
            "Mixed-integer quadratic programming "
            "is not yet supported.";

        decision.use_cpu = true;
        decision.use_gpu = false;

        return decision;
    }

    decision.method =
        SolverMethod::QP;

    decision.solver_name =
        solver_method_name(
            decision.method
        );

    decision.reason =
        "Continuous quadratic objective detected; "
        "QP solver selected.";

    decision.use_cpu = true;
    decision.use_gpu = false;

    return decision;
}

DispatchDecision AdaptiveDispatcher::dispatch_mixed_integer(
    const ProblemFingerprint& fingerprint
) const {
    DispatchDecision decision;

    if (fingerprint.has_quadratic_objective) {

        decision.method =
            SolverMethod::Unsupported;

        decision.solver_name =
            solver_method_name(
                decision.method
            );

        decision.reason =
            "Mixed-integer quadratic programming "
            "is not yet supported.";

        decision.use_cpu = true;
        decision.use_gpu = false;

        return decision;
    }

    decision.method =
        SolverMethod::MILP;

    decision.solver_name =
        solver_method_name(
            decision.method
        );

    decision.reason =
        "Integer or binary variables detected; "
        "MILP Branch-and-Bound selected.";

    decision.use_cpu = true;
    decision.use_gpu = false;

    return decision;
}

} // namespace dent