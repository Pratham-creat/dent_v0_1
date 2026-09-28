#pragma once

#include "dent/model/problem.hpp"
#include "dent/solver/solver.hpp"

#include <vector>

namespace dent {

class PDHGSolver final : public Solver
{
public:
    explicit PDHGSolver(
        double tolerance = 1e-7,
        int max_iterations = 10000
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
