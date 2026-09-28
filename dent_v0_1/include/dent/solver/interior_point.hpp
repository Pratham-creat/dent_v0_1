#pragma once

#include "dent/solver/solver.hpp"

namespace dent {

class InteriorPointSolver final : public Solver {
public:
    explicit InteriorPointSolver(
        double tolerance = 1e-8,
        int max_iterations = 100
    );

    SolveResult solve(const Problem& problem) const override;

    void set_warm_start(const WarmStart& warm_start) override;
    void clear_warm_start() override;
    bool has_warm_start() const override;

private:
    double tolerance_;
    int max_iterations_;

    mutable WarmStart warm_start_;
};

} // namespace dent