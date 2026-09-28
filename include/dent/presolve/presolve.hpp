#pragma once

#include "dent/model/problem.hpp"

#include <string>
#include <vector>

namespace dent
{

enum class PresolveStatus
{
    Success,
    Infeasible,
    Unsupported
};

struct PresolveStatistics
{
    int original_variables = 0;
    int original_constraints = 0;

    int reduced_variables = 0;
    int reduced_constraints = 0;

    int variables_removed = 0;
    int constraints_removed = 0;

    int fixed_variables = 0;
    int trivial_constraints_removed = 0;

    int bounds_tightened = 0;
    int rows_scaled = 0;
};

struct PresolveResult
{
    PresolveStatus status =
        PresolveStatus::Success;

    /*
     * Problem after presolve and scaling.
     */
    Problem reduced_problem;

    /*
     * Original variable -> reduced variable.
     *
     * >= 0 : variable survived
     * -1   : variable was fixed/eliminated
     */
    std::vector<int> original_to_reduced;

    /*
     * Reduced variable -> original variable.
     */
    std::vector<int> reduced_to_original;

    /*
     * Fixed values for eliminated variables.
     */
    std::vector<double> fixed_values;

    /*
     * Constant contribution removed from
     * the objective during fixed-variable
     * elimination.
     *
     * Objective convention:
     *
     *     1/2 x^T Q x + c^T x
     */
    double objective_offset = 0.0;

    PresolveStatistics statistics;

    std::string message;

    /*
     * Reconstruct original variable vector.
     */
    std::vector<double> postsolve_values(
        const std::vector<double>& reduced_values
    ) const;

    /*
     * Restore objective constant removed
     * during presolve.
     */
    double postsolve_objective(
        double reduced_objective
    ) const;
};


class Presolver
{
public:

    explicit Presolver(
        double tolerance = 1e-9,
        int max_passes = 5
    );

    PresolveResult run(
        const Problem& problem
    ) const;

private:

    double tolerance_;

    int max_passes_;

    bool approximately_equal(
        double a,
        double b
    ) const;

    bool is_zero(
        double value
    ) const;

    bool is_finite(
        double value
    ) const;

    /*
     * Tighten bounds using single-variable
     * constraints.
     *
     * infeasible is explicitly separated from
     * "no change".
     */
    bool tighten_single_variable_constraints(
        Problem& problem,
        PresolveStatistics& statistics,
        bool& infeasible
    ) const;

    /*
     * Detect rows of the form:
     *
     *     0 <= b
     *     0 = b
     *     0 >= b
     */
    bool detect_trivial_constraints(
        const Problem& problem,
        std::vector<bool>& removable_constraints,
        bool& infeasible
    ) const;

    /*
     * Construct the reduced model.
     */
    Problem build_reduced_problem(
        const Problem& problem,
        const std::vector<bool>& fixed_variables,
        const std::vector<double>& fixed_values,
        const std::vector<bool>& removable_constraints,
        const Problem& original_problem,
        PresolveResult& result
    ) const;

    /*
     * Scale constraint rows using the maximum
     * absolute coefficient.
     */
    void scale_rows(
        Problem& problem,
        PresolveStatistics& statistics
    ) const;
};

} // namespace dent