#pragma once

#include "dent/model/problem.hpp"
#include "dent/solver/solver.hpp"
#include "dent/warmstart/warm_start.hpp"

#include <string>

namespace dent {

class DualSimplexSolver final : public Solver {
public:
    explicit DualSimplexSolver(
        double tolerance = 1e-9,
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