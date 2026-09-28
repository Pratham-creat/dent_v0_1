#pragma once

#include "dent/model/problem.hpp"

#include <string>
#include <vector>

namespace dent
{

struct ScalingResult
{
    Problem scaled_problem;

    std::vector<double> row_scale;
    std::vector<double> column_scale;

    int iterations = 0;
    int rows_scaled = 0;
    int columns_scaled = 0;

    bool applied = false;
    std::string message;

    std::vector<double> postsolve_values(
        const std::vector<double>& scaled_values
    ) const;
};

class GeometricScaler
{
public:
    explicit GeometricScaler(
        double tolerance = 1e-9,
        int max_iterations = 5
    );

    ScalingResult scale(
        const Problem& problem
    ) const;

private:
    double tolerance_;
    int max_iterations_;

    bool approximately_one(double value) const;

    std::vector<double> compute_row_scales(
        const Problem& problem,
        const std::vector<double>& column_scale
    ) const;

    std::vector<double> compute_column_scales(
        const Problem& problem,
        const std::vector<double>& row_scale
    ) const;

    Problem build_scaled_problem(
        const Problem& problem,
        const std::vector<double>& row_scale,
        const std::vector<double>& column_scale
    ) const;
};

} // namespace dent