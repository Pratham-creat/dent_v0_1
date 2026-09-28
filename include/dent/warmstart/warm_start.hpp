#pragma once

#include "dent/model/problem.hpp"

#include <string>
#include <vector>

namespace dent {

struct WarmStart
{
    bool available = false;

    std::vector<double> variable_values;

    /*
        Standard-form basis.

        Example:

            Original variables:
                x1 -> 0
                x2 -> 1

            Slack variables:
                s1 -> 2
                s2 -> 3

            Basis:
                {0, 3}
    */
    std::vector<int> basis;

    /*
        Dimensions of the standard-form representation
        associated with the persisted basis.
    */
    int rows = 0;
    int columns = 0;

    /*
        Structural signature deliberately excludes RHS and
        objective coefficients.

        This allows:

            - RHS changes
            - objective changes

        while rejecting:

            - matrix coefficient changes
            - variable type changes
            - bound changes
            - constraint-sense changes
            - dimension changes
    */
    std::string structural_signature;

    std::string source;

    void clear();

    bool matches_problem(
        const Problem& problem
    ) const;
};


/*
    Generate a deterministic structural signature for a model.

    The signature includes the mathematical structure of the
    optimization model but intentionally excludes:

        - RHS values
        - objective coefficients

    This is what allows a persisted basis to survive
    RHS-only reoptimization.
*/
std::string structural_signature_for_problem(
    const Problem& problem
);


/*
    Construct a basis warm-start record.

    Parameters:

        problem
            Problem associated with the basis.

        basis
            Standard-form basis column indices.

        rows
            Number of rows in the standard-form basis.

        columns
            Number of standard-form columns.

        variable_values
            Optional previous primal solution.

        source
            Human-readable source of the basis.
*/
WarmStart make_basis_warm_start(
    const Problem& problem,
    const std::vector<int>& basis,
    int rows,
    int columns,
    const std::vector<double>& variable_values = {},
    const std::string& source = "external"
);

} // namespace dent