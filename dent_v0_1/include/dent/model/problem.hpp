#pragma once

#include "dent/matrix/sparse_matrix.hpp"

#include <string>
#include <vector>

namespace dent {

enum class ObjectiveSense {
    Minimize,
    Maximize
};

enum class ConstraintSense {
    LessEqual,
    Equal,
    GreaterEqual
};

enum class VariableType {
    Continuous,
    Integer,
    Binary
};

struct Variable {
    std::string name;
    double lower_bound = 0.0;
    double upper_bound = 0.0; // 0 means +infinity for the prototype
    VariableType type = VariableType::Continuous;
};

struct Constraint {
    std::string name;
    ConstraintSense sense;
    double rhs;
};

class Problem {
public:
    explicit Problem(
        ObjectiveSense sense = ObjectiveSense::Maximize
    );

    int add_variable(
        const std::string& name,
        double lower_bound = 0.0,
        double upper_bound = 0.0,
        VariableType type = VariableType::Continuous
    );

    int add_constraint(
        const std::string& name,
        ConstraintSense sense,
        double rhs
    );

    void set_objective_coefficient(
        int variable,
        double coefficient
    );

    void set_constraint_coefficient(
        int constraint,
        int variable,
        double coefficient
    );

    // Quadratic objective:
    // 1/2 x^T Q x + c^T x
    void set_quadratic_coefficient(
        int row_variable,
        int column_variable,
        double coefficient
    );

    void set_variable_type(
        int variable,
        VariableType type
    );

    void set_variable_bounds(
        int variable,
        double lower_bound,
        double upper_bound
    );

    ObjectiveSense objective_sense() const;

    const std::vector<Variable>& variables() const;
    const std::vector<Constraint>& constraints() const;
    const std::vector<double>& objective() const;
    const std::vector<std::vector<double>>& matrix() const;
    const SparseMatrix& sparse_matrix() const;
    const std::vector<std::vector<double>>& quadratic_matrix() const;

private:
    ObjectiveSense objective_sense_;

    std::vector<Variable> variables_;

    std::vector<Constraint> constraints_;

    std::vector<double> objective_;

    std::vector<std::vector<double>> matrix_;

    SparseMatrix sparse_matrix_;

    std::vector<std::vector<double>> quadratic_matrix_;
};

} // namespace dent