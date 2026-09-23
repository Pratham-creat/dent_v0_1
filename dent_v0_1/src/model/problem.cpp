#include "dent/model/problem.hpp"

#include <stdexcept>

namespace dent {

Problem::Problem(ObjectiveSense sense)
    : objective_sense_(sense),
      sparse_matrix_(0, 0)
{
}

int Problem::add_variable(
    const std::string& name,
    double lower_bound,
    double upper_bound,
    VariableType type
)
{
    const int index =
        static_cast<int>(variables_.size());

    variables_.push_back({
        name,
        lower_bound,
        upper_bound,
        type
    });

    objective_.push_back(0.0);

    for (auto& row : matrix_) {
        row.push_back(0.0);
    }

    for (auto& row : quadratic_matrix_) {
        row.push_back(0.0);
    }

    quadratic_matrix_.push_back(
        std::vector<double>(
            variables_.size(),
            0.0
        )
    );

    sparse_matrix_.resize(
        constraints_.size(),
        variables_.size()
    );

    return index;
}

int Problem::add_constraint(
    const std::string& name,
    ConstraintSense sense,
    double rhs
)
{
    const int index =
        static_cast<int>(constraints_.size());

    constraints_.push_back({
        name,
        sense,
        rhs
    });

    matrix_.push_back(
        std::vector<double>(
            variables_.size(),
            0.0
        )
    );

    sparse_matrix_.resize(
        constraints_.size(),
        variables_.size()
    );

    return index;
}

void Problem::set_objective_coefficient(
    int variable,
    double coefficient
)
{
    if (variable < 0 ||
        variable >= static_cast<int>(variables_.size())) {

        throw std::out_of_range(
            "Problem::set_objective_coefficient: "
            "variable index out of range"
        );
    }

    objective_[variable] = coefficient;
}

void Problem::set_constraint_coefficient(
    int constraint,
    int variable,
    double coefficient
)
{
    if (constraint < 0 ||
        constraint >= static_cast<int>(constraints_.size())) {

        throw std::out_of_range(
            "Problem::set_constraint_coefficient: "
            "constraint index out of range"
        );
    }

    if (variable < 0 ||
        variable >= static_cast<int>(variables_.size())) {

        throw std::out_of_range(
            "Problem::set_constraint_coefficient: "
            "variable index out of range"
        );
    }

    matrix_[constraint][variable] = coefficient;

    sparse_matrix_.set(
        static_cast<std::size_t>(constraint),
        static_cast<std::size_t>(variable),
        coefficient
    );
}

void Problem::set_quadratic_coefficient(
    int row_variable,
    int column_variable,
    double coefficient
)
{
    if (row_variable < 0 ||
        row_variable >= static_cast<int>(variables_.size())) {

        throw std::out_of_range(
            "Problem::set_quadratic_coefficient: "
            "row variable index out of range"
        );
    }

    if (column_variable < 0 ||
        column_variable >= static_cast<int>(variables_.size())) {

        throw std::out_of_range(
            "Problem::set_quadratic_coefficient: "
            "column variable index out of range"
        );
    }

    quadratic_matrix_[row_variable][column_variable] =
        coefficient;
}

void Problem::set_variable_type(
    int variable,
    VariableType type
)
{
    if (variable < 0 ||
        variable >= static_cast<int>(variables_.size())) {

        throw std::out_of_range(
            "Problem::set_variable_type: "
            "variable index out of range"
        );
    }

    variables_[variable].type = type;

    if (type == VariableType::Binary) {
        variables_[variable].lower_bound = 0.0;
        variables_[variable].upper_bound = 1.0;
    }
}

void Problem::set_variable_bounds(
    int variable,
    double lower_bound,
    double upper_bound
)
{
    if (variable < 0 ||
        variable >= static_cast<int>(variables_.size())) {

        throw std::out_of_range(
            "Problem::set_variable_bounds: "
            "variable index out of range"
        );
    }

    if (upper_bound != 0.0 &&
        upper_bound < lower_bound) {

        throw std::invalid_argument(
            "Upper bound cannot be smaller "
            "than lower bound."
        );
    }

    variables_[variable].lower_bound =
        lower_bound;

    variables_[variable].upper_bound =
        upper_bound;
}

ObjectiveSense Problem::objective_sense() const
{
    return objective_sense_;
}

const std::vector<Variable>&
Problem::variables() const
{
    return variables_;
}

const std::vector<Constraint>&
Problem::constraints() const
{
    return constraints_;
}

const std::vector<double>&
Problem::objective() const
{
    return objective_;
}

const std::vector<std::vector<double>>&
Problem::matrix() const
{
    return matrix_;
}

const SparseMatrix&
Problem::sparse_matrix() const
{
    return sparse_matrix_;
}

const std::vector<std::vector<double>>&
Problem::quadratic_matrix() const
{
    return quadratic_matrix_;
}

} // namespace dent