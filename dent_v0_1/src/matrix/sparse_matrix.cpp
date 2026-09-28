#include "dent/matrix/sparse_matrix.hpp"

#include <algorithm>
#include <stdexcept>

namespace dent {

SparseMatrix::SparseMatrix(
    std::size_t rows,
    std::size_t columns
)
    : rows_(rows),
      columns_(columns)
{
}

void SparseMatrix::resize(
    std::size_t rows,
    std::size_t columns
)
{
    rows_ = rows;
    columns_ = columns;

    entries_.erase(
        std::remove_if(
            entries_.begin(),
            entries_.end(),
            [rows, columns](const SparseEntry& entry) {
                return entry.row >= rows ||
                       entry.column >= columns;
            }
        ),
        entries_.end()
    );
}

void SparseMatrix::add(
    std::size_t row,
    std::size_t column,
    double value
)
{
    if (row >= rows_ || column >= columns_) {
        throw std::out_of_range(
            "SparseMatrix::add: index out of range"
        );
    }

    if (value == 0.0) {
        return;
    }

    for (auto& entry : entries_) {
        if (entry.row == row &&
            entry.column == column) {

            entry.value += value;

            // Remove exact zero entries.
            if (entry.value == 0.0) {
                entry = entries_.back();
                entries_.pop_back();
            }

            return;
        }
    }

    entries_.push_back({
        row,
        column,
        value
    });
}

void SparseMatrix::set(
    std::size_t row,
    std::size_t column,
    double value
)
{
    if (row >= rows_ || column >= columns_) {
        throw std::out_of_range(
            "SparseMatrix::set: index out of range"
        );
    }

    for (auto& entry : entries_) {
        if (entry.row == row &&
            entry.column == column) {

            if (value == 0.0) {
                entry = entries_.back();
                entries_.pop_back();
            }
            else {
                entry.value = value;
            }

            return;
        }
    }

    // Do not store exact zero values.
    if (value != 0.0) {
        entries_.push_back({
            row,
            column,
            value
        });
    }
}

std::size_t SparseMatrix::rows() const
{
    return rows_;
}

std::size_t SparseMatrix::columns() const
{
    return columns_;
}

std::size_t SparseMatrix::nonzeros() const
{
    return entries_.size();
}

const std::vector<SparseEntry>&
SparseMatrix::entries() const
{
    return entries_;
}

double SparseMatrix::get(
    std::size_t row,
    std::size_t column
) const
{
    if (row >= rows_ || column >= columns_) {
        throw std::out_of_range(
            "SparseMatrix::get: index out of range"
        );
    }

    for (const auto& entry : entries_) {
        if (entry.row == row &&
            entry.column == column) {

            return entry.value;
        }
    }

    return 0.0;
}

std::vector<double> SparseMatrix::multiply(
    const std::vector<double>& x
) const
{
    if (x.size() != columns_) {
        throw std::invalid_argument(
            "SparseMatrix::multiply: vector size does not match matrix columns"
        );
    }

    std::vector<double> result(rows_, 0.0);

    for (const auto& entry : entries_) {
        result[entry.row] +=
            entry.value * x[entry.column];
    }

    return result;
}

void SparseMatrix::clear()
{
    entries_.clear();
}

} // namespace dent