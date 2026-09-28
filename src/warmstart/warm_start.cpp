#include "dent/warmstart/warm_start.hpp"

#include <iomanip>
#include <sstream>

namespace dent {

namespace {

void append_double(
    std::ostringstream& stream,
    double value
)
{
    stream
        << std::setprecision(17)
        << value
        << ';';
}

} // namespace

std::string structural_signature_for_problem(
    const Problem& problem
)
{
    std::ostringstream stream;

    stream
        << "sense="
        << static_cast<int>(
               problem.objective_sense())
        << '|';

    stream
        << "variables="
        << problem.variables().size()
        << '|';

    stream
        << "constraints="
        << problem.constraints().size()
        << '|';

    for (const auto& variable :
         problem.variables())
    {
        stream
            << "type="
            << static_cast<int>(
                   variable.type)
            << ',';

        append_double(
            stream,
            variable.lower_bound
        );

        append_double(
            stream,
            variable.upper_bound
        );
    }

    stream << '|';

    for (const auto& constraint :
         problem.constraints())
    {
        stream
            << "sense="
            << static_cast<int>(
                   constraint.sense)
            << '|';

        /*
            RHS is intentionally excluded.

            This is what allows a previous basis
            to survive RHS/bound changes and makes
            Dual Simplex reoptimization possible.
        */
    }

    stream << "matrix|";

    for (const auto& row :
         problem.matrix())
    {
        for (double value :
             row)
        {
            append_double(
                stream,
                value
            );
        }

        stream << '/';
    }

    return stream.str();
}

void WarmStart::clear()
{
    available =
        false;

    variable_values.clear();

    basis.clear();

    rows =
        0;

    columns =
        0;

    structural_signature.clear();

    source.clear();
}

bool WarmStart::matches_problem(
    const Problem& problem
) const
{
    if (!available)
    {
        return false;
    }

    return structural_signature ==
           structural_signature_for_problem(
               problem
           );
}

WarmStart make_basis_warm_start(
    const Problem& problem,
    const std::vector<int>& basis,
    int rows,
    int columns,
    const std::vector<double>& variable_values,
    const std::string& source
)
{
    WarmStart warm_start;

    warm_start.available =
        true;

    warm_start.variable_values =
        variable_values;

    warm_start.basis =
        basis;

    warm_start.rows =
        rows;

    warm_start.columns =
        columns;

    warm_start.structural_signature =
        structural_signature_for_problem(
            problem
        );

    warm_start.source =
        source;

    return warm_start;
}

} // namespace dent