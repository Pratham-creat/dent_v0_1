#pragma once
#include <vector>
namespace dent {
class ForrestTomlin {
public:
    bool factorize(const std::vector<std::vector<double>>& matrix, double tolerance=1e-12);
    bool update_eta(int pivot_row, const std::vector<double>& column);
    bool solve(const std::vector<double>& rhs, std::vector<double>& x) const;
    bool solve_transpose(const std::vector<double>& rhs, std::vector<double>& x) const;
    std::size_t size() const;
    std::size_t updates() const;
private:
    std::vector<std::vector<double>> lu_;
    std::vector<int> perm_;
    struct Eta { int row; std::vector<double> column; };
    std::vector<Eta> etas_;
    double tolerance_=1e-12;
};
}
