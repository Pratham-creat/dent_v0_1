#include "dent/numerics/lu_factorization.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dent {

bool LUFactorization::factorize(
    const SparseMatrix& matrix,
    const LUOptions& options)
{
    std::vector<std::vector<double>> dense(
        matrix.rows(), std::vector<double>(matrix.columns(), 0.0));
    for (const auto& e : matrix.entries()) dense[e.row][e.column] = e.value;
    return factorize(dense, options);
}

bool LUFactorization::factorize(
    const std::vector<std::vector<double>>& matrix,
    const LUOptions& options)
{
    n_ = matrix.size();
    singular_ = true;
    lu_ = matrix;
    row_perm_.resize(n_);
    col_perm_.resize(n_);
    for (std::size_t i=0;i<n_;++i) { row_perm_[i]=static_cast<int>(i); col_perm_[i]=static_cast<int>(i); }
    if (n_ == 0) { singular_ = false; return true; }
    for (const auto& row : matrix) if (row.size() != n_) return false;

    tolerance_ = options.tolerance;
    for (std::size_t k=0;k<n_;++k) {
        std::size_t pivot = k;
        double best = std::abs(lu_[k][k]);

        if (options.markowitz_ordering) {
            std::size_t best_cost = std::numeric_limits<std::size_t>::max();
            for (std::size_t r=k;r<n_;++r) {
                if (std::abs(lu_[r][k]) <= tolerance_) continue;
                std::size_t row_nz=0, col_nz=0;
                for (std::size_t j=k;j<n_;++j) if (std::abs(lu_[r][j])>tolerance_) ++row_nz;
                for (std::size_t i=k;i<n_;++i) if (std::abs(lu_[i][k])>tolerance_) ++col_nz;
                const std::size_t cost = (row_nz ? row_nz-1 : 0) * (col_nz ? col_nz-1 : 0);
                if (cost < best_cost || (cost == best_cost && std::abs(lu_[r][k]) > best)) {
                    best_cost = cost; pivot = r; best = std::abs(lu_[r][k]);
                }
            }
        } else if (options.partial_pivoting) {
            for (std::size_t r=k+1;r<n_;++r) {
                if (std::abs(lu_[r][k]) > best) { best=std::abs(lu_[r][k]); pivot=r; }
            }
        }

        if (best <= tolerance_) return false;
        if (pivot != k) {
            std::swap(lu_[pivot], lu_[k]);
            std::swap(row_perm_[pivot], row_perm_[k]);
        }
        for (std::size_t i=k+1;i<n_;++i) {
            const double f = lu_[i][k] / lu_[k][k];
            lu_[i][k] = f;
            for (std::size_t j=k+1;j<n_;++j) lu_[i][j] -= f * lu_[k][j];
        }
    }
    singular_ = false;
    return true;
}

bool LUFactorization::solve(const std::vector<double>& rhs, std::vector<double>& x) const {
    if (singular_ || rhs.size()!=n_) return false;
    std::vector<double> y(n_);
    for (std::size_t i=0;i<n_;++i) {
        y[i]=rhs[row_perm_[i]];
        for (std::size_t j=0;j<i;++j) y[i]-=lu_[i][j]*y[j];
    }
    x.assign(n_,0.0);
    for (std::size_t ii=0;ii<n_;++ii) {
        const std::size_t i=n_-1-ii;
        double v=y[i];
        for (std::size_t j=i+1;j<n_;++j) v-=lu_[i][j]*x[j];
        if (std::abs(lu_[i][i])<=tolerance_) return false;
        x[i]=v/lu_[i][i];
    }
    return true;
}

bool LUFactorization::solve_transpose(const std::vector<double>& rhs, std::vector<double>& x) const {
    if (singular_ || rhs.size()!=n_) return false;
    // Solve U^T z = rhs, L^T y = z, then undo row permutation.
    std::vector<double> z(n_,0.0), y(n_,0.0);
    for (std::size_t i=0;i<n_;++i) {
        double v=rhs[i];
        for (std::size_t j=0;j<i;++j) v-=lu_[j][i]*z[j];
        if (std::abs(lu_[i][i])<=tolerance_) return false;
        z[i]=v/lu_[i][i];
    }
    for (std::size_t ii=0;ii<n_;++ii) {
        const std::size_t i=n_-1-ii;
        double v=z[i];
        for (std::size_t j=i+1;j<n_;++j) v-=lu_[j][i]*y[j];
        y[i]=v;
    }
    x.assign(n_,0.0);
    for (std::size_t i=0;i<n_;++i) x[row_perm_[i]]=y[i];
    return true;
}

std::size_t LUFactorization::size() const { return n_; }
bool LUFactorization::singular() const { return singular_; }
const std::vector<int>& LUFactorization::row_permutation() const { return row_perm_; }
const std::vector<int>& LUFactorization::column_permutation() const { return col_perm_; }

} // namespace dent
