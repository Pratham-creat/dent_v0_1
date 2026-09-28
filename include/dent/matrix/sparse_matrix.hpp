#pragma once

#include <cstddef>
#include <vector>

namespace dent {

struct SparseEntry {
    std::size_t row;
    std::size_t column;
    double value;
};

class SparseMatrix {
public:
    SparseMatrix(
        std::size_t rows = 0,
        std::size_t columns = 0
    );

    void resize(
        std::size_t rows,
        std::size_t columns
    );

    // Add value to an existing entry.
    void add(
        std::size_t row,
        std::size_t column,
        double value
    );

    // Replace an entry with an exact value.
    // If value == 0, the entry is removed.
    void set(
        std::size_t row,
        std::size_t column,
        double value
    );

    std::size_t rows() const;

    std::size_t columns() const;

    std::size_t nonzeros() const;

    const std::vector<SparseEntry>&
    entries() const;

    double get(
        std::size_t row,
        std::size_t column
    ) const;

    std::vector<double> multiply(
        const std::vector<double>& x
    ) const;

    void clear();

private:
    std::size_t rows_;

    std::size_t columns_;

    std::vector<SparseEntry> entries_;
};

} // namespace dent