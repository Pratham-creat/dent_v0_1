#pragma once

#include "dent/model/problem.hpp"

#include <string>

namespace dent {

struct ProblemFingerprint {
    // Problem dimensions
    int variables = 0;
    int constraints = 0;
    int nonzeros = 0;

    // Matrix structure
    double density = 0.0;
    bool highly_sparse = false;
    bool large_problem = false;

    // Coefficient statistics
    double min_nonzero_coefficient = 0.0;
    double max_nonzero_coefficient = 0.0;
    double coefficient_ratio = 1.0;

    // Row/column norm statistics
    double min_row_norm = 0.0;
    double max_row_norm = 0.0;
    double row_norm_ratio = 1.0;

    double min_column_norm = 0.0;
    double max_column_norm = 0.0;
    double column_norm_ratio = 1.0;

    // RHS statistics
    double min_rhs = 0.0;
    double max_rhs = 0.0;
    double rhs_ratio = 1.0;

    // Numerical conditioning
    bool poorly_scaled = false;

    // Objective structure
    bool has_quadratic_objective = false;
    int quadratic_nonzeros = 0;

    // Variable types
    int continuous_variables = 0;
    int integer_variables = 0;
    int binary_variables = 0;

    // Constraint structure
    int less_equal_constraints = 0;
    int equal_constraints = 0;
    int greater_equal_constraints = 0;

    // Bounds
    int bounded_variables = 0;
    int unbounded_variables = 0;

    // Problem classification
    bool is_linear = true;
    bool is_quadratic = false;
    bool is_mixed_integer = false;

    // Human-readable structural description
    std::string structure;
};

/*
 * Analyze a Problem and generate its structural fingerprint.
 *
 * This is intentionally a free function because the dispatcher,
 * tests, and future APIs can obtain the same fingerprint without
 * having to instantiate another object.
 */
ProblemFingerprint fingerprint_problem(const Problem& problem);

} // namespace dent