#pragma once

#include "dent/solver/solver.hpp"

#include <memory>

namespace dent {

class SolverSession
{
public:
    explicit SolverSession(
        std::shared_ptr<Solver> solver
    );

    SolveResult solve(
        const Problem& problem
    );

    void set_warm_start(
        const WarmStart& warm_start
    );

    void clear_warm_start();

    bool has_warm_start() const;

    std::shared_ptr<Solver> solver();

    std::shared_ptr<const Solver> solver() const;

private:
    std::shared_ptr<Solver> solver_;
};

} // namespace dent