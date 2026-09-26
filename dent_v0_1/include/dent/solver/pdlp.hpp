#pragma once

#include "dent/model/problem.hpp"
#include "dent/solver/solver.hpp"

namespace dent {

class PDLPSolver final : public Solver
{
public:
    explicit PDLPSolver(
        double tolerance = 1e-7,
        int max_iterations = 20000
    );

    SolveResult solve(
        const Problem& problem
    ) const override;

    void set_warm_start(
        const WarmStart& warm_start
    ) override;

    void clear_warm_start() override;

    bool has_warm_start() const override;

private:
    double tolerance_;
    int max_iterations_;

    mutable WarmStart warm_start_;
};

} // namespace dent