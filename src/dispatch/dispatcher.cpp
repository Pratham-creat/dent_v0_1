#include "dent/dispatch/dispatcher.hpp"

#include "dent/model/problem.hpp"

namespace dent {

const char* solver_method_name(
    SolverMethod method
)
{
    switch (method) {

        case SolverMethod::PrimalSimplex:
            return "Primal Simplex";

        case SolverMethod::DualSimplex:
            return "Dual Simplex";

        case SolverMethod::InteriorPoint:
            return "Interior Point";

        case SolverMethod::PDHG:
            return "PDHG";

        case SolverMethod::PDLP:
            return "PDLP";

        case SolverMethod::QP:
            return "QP";

        case SolverMethod::MILP:
            return "MILP Branch-and-Bound";

        case SolverMethod::MIQP:
            return "MIQP Branch-and-Bound";

        case SolverMethod::Unsupported:
        default:
            return "Unsupported";
    }
}


DispatchDecision AdaptiveDispatcher::dispatch(
    const Problem& problem,
    const ProblemFingerprint& fingerprint
) const
{
    (void)problem;

    /*
     * Mixed-integer models always take the MILP path.
     */
    if (fingerprint.is_mixed_integer) {
        return dispatch_mixed_integer(
            fingerprint
        );
    }

    /*
     * Quadratic continuous models use QP.
     */
    if (fingerprint.has_quadratic_objective) {
        return dispatch_quadratic_program(
            fingerprint
        );
    }

    /*
     * Remaining supported continuous models are LPs.
     */
    if (fingerprint.is_linear) {
        return dispatch_linear_program(
            fingerprint
        );
    }

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
) const
{
    DispatchDecision decision;

    /*
     * Current continuous LP policy:
     *
     * 1. Large LP
     *      -> Interior Point
     *
     * 2. Large poorly-scaled LP
     *      -> Interior Point
     *
     * 3. Highly sparse but non-large LP
     *      -> PDHG
     *
     * 4. Small/ordinary LP
     *      -> Primal Simplex
     *
     * PDLP is deliberately not automatically selected yet.
     *
     * It is implemented and tested, but its production
     * dispatch policy should be established after benchmarking
     * it against Simplex/IPM/PDHG on representative LP sets.
     *
     * This avoids claiming a performance advantage before
     * benchmark evidence exists.
     */

    if (
        fingerprint.large_problem
    ) {
        decision.method =
            SolverMethod::InteriorPoint;

        if (
            fingerprint.highly_sparse
        ) {
            decision.reason =
                "Large sparse continuous LP; "
                "Interior Point selected for scalable "
                "sparse primal-dual optimization.";
        }
        else if (
            fingerprint.poorly_scaled
        ) {
            decision.reason =
                "Large poorly scaled continuous LP; "
                "Interior Point selected after "
                "presolve/scaling for numerical robustness.";
        }
        else {
            decision.reason =
                "Large continuous LP; "
                "Interior Point selected to avoid "
                "simplex basis growth on larger models.";
        }
    }
    else if (
        fingerprint.highly_sparse
    ) {
        decision.method =
            SolverMethod::PDHG;

        decision.reason =
            "Highly sparse continuous LP; "
            "PDHG selected as the lightweight "
            "primal-dual first-order method.";
    }
    else {
        decision.method =
            SolverMethod::PrimalSimplex;

        decision.reason =
            "Small or ordinary continuous LP; "
            "Primal Simplex selected for direct "
            "basis-based optimization.";
    }

    decision.solver_name =
        solver_method_name(
            decision.method
        );

    decision.use_cpu = true;

    /*
     * CUDA remains intentionally disabled.
     */
    decision.use_gpu = false;

    return decision;
}


DispatchDecision AdaptiveDispatcher::dispatch_quadratic_program(
    const ProblemFingerprint& fingerprint
) const
{
    DispatchDecision decision;

    if (fingerprint.is_mixed_integer) {

        decision.method =
            SolverMethod::MIQP;

        decision.solver_name =
            solver_method_name(
                decision.method
            );

        decision.reason =
            "Mixed-integer quadratic objective detected; "
            "MIQP branch-and-bound selected.";

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
) const
{
    DispatchDecision decision;

    if (
        fingerprint.has_quadratic_objective
    ) {
        decision.method =
            SolverMethod::MIQP;

        decision.solver_name =
            solver_method_name(
                decision.method
            );

        decision.reason =
            "Mixed-integer quadratic objective detected; "
            "MIQP branch-and-bound selected.";

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