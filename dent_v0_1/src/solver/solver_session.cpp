#include "dent/solver/solver_session.hpp"

#include <stdexcept>
#include <utility>

namespace dent {

SolverSession::SolverSession(
    std::shared_ptr<Solver> solver
)
    : solver_(std::move(solver))
{
    if (!solver_)
    {
        throw std::invalid_argument(
            "SolverSession requires a valid solver."
        );
    }
}

SolveResult SolverSession::solve(
    const Problem& problem
)
{
    return solver_->solve(problem);
}

void SolverSession::set_warm_start(
    const WarmStart& warm_start
)
{
    solver_->set_warm_start(
        warm_start
    );
}

void SolverSession::clear_warm_start()
{
    solver_->clear_warm_start();
}

bool SolverSession::has_warm_start() const
{
    return solver_->has_warm_start();
}

std::shared_ptr<Solver>
SolverSession::solver()
{
    return solver_;
}

std::shared_ptr<const Solver>
SolverSession::solver() const
{
    return solver_;
}

} // namespace dent