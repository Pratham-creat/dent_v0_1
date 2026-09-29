#pragma once

#include "dent/matrix/sparse_matrix.hpp"

#include <cstddef>
#include <vector>

namespace dent {

struct LUOptions {
    double tolerance = 1e-12;
    bool partial_pivoting = true;
    bool markowitz_ordering = true;
};

class LUFactorization {
public:
    bool factorize(const SparseMatrix& matrix, const LUOptions& options = {});
    bool factorize(const std::vector<std::vector<double>>& matrix,
                   const LUOptions& options = {});

    bool solve(const std::vector<double>& rhs, std::vector<double>& x) const;
    bool solve_transpose(const std::vector<double>& rhs, std::vector<double>& x) const;

    std::size_t size() const;
    bool singular() const;
    const std::vector<int>& row_permutation() const;
    const std::vector<int>& column_permutation() const;

private:
    std::size_t n_ = 0;
    double tolerance_ = 1e-12;
    bool singular_ = true;
    std::vector<std::vector<double>> lu_;
    std::vector<int> row_perm_;
    std::vector<int> col_perm_;
};

} // namespace dent
