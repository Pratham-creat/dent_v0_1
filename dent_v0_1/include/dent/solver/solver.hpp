#pragma once

#include "dent/warmstart/warm_start.hpp"

#include <string>
#include <vector>

namespace dent {

class Problem;

enum class SolveStatus {
    Optimal,
    Infeasible,
    Unbounded,
    IterationLimit,
    Unsupported
};

struct SolveResult
{
    SolveStatus status =
        SolveStatus::Unsupported;

    double objective_value =
        0.0;

    std::vector<double> variable_values;

    int iterations =
        0;

    int warm_start_iterations =
        0;

    bool warm_start_used =
        false;

    /*
        Basis information is exposed so that
        MILP node LPs can reuse the parent basis.
    */
    std::vector<int> basis;

    int basis_rows =
        0;

    int basis_columns =
        0;

    std::string message;
};

class Solver {
public:
    virtual ~Solver() = default;

    virtual SolveResult solve(
        const Problem& problem
    ) const = 0;

    virtual void set_warm_start(
        const WarmStart& warm_start
    ) = 0;

    virtual void clear_warm_start() = 0;

    virtual bool has_warm_start() const = 0;
};

} // namespace dent