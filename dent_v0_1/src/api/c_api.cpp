#if defined(_WIN32) && !defined(DENT_EXPORTS)
#define DENT_EXPORTS
#endif

#include "dent/api/c_api.hpp"

#include "dent/dispatch/dispatcher.hpp"
#include "dent/dispatch/fingerprint.hpp"
#include "dent/io/model_parser.hpp"
#include "dent/model/problem.hpp"
#include "dent/solver/interior_point.hpp"
#include "dent/solver/milp.hpp"
#include "dent/solver/pdhg.hpp"
#include "dent/solver/pdlp.hpp"
#include "dent/solver/qp.hpp"
#include "dent/solver/simplex.hpp"

#include <cstdlib>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{

std::string json_escape(
    const std::string& value
)
{
    std::ostringstream output;

    for (const char character : value)
    {
        switch (character)
        {
            case '"':
                output << "\\\"";
                break;

            case '\\':
                output << "\\\\";
                break;

            case '\b':
                output << "\\b";
                break;

            case '\f':
                output << "\\f";
                break;

            case '\n':
                output << "\\n";
                break;

            case '\r':
                output << "\\r";
                break;

            case '\t':
                output << "\\t";
                break;

            default:
                output << character;
                break;
        }
    }

    return output.str();
}


const char* status_name(
    dent::SolveStatus status
)
{
    switch (status)
    {
        case dent::SolveStatus::Optimal:
            return "optimal";

        case dent::SolveStatus::Infeasible:
            return "infeasible";

        case dent::SolveStatus::Unbounded:
            return "unbounded";

        case dent::SolveStatus::IterationLimit:
            return "iteration_limit";

        case dent::SolveStatus::Unsupported:
        default:
            return "unsupported";
    }
}


struct InternalResult
{
    dent::SolveStatus status =
        dent::SolveStatus::Unsupported;

    double objective =
        0.0;

    std::vector<double> values;

    int iterations =
        0;

    std::string solver;

    std::string message;
};


InternalResult solve_problem(
    const dent::Problem& problem
)
{
    const dent::ProblemFingerprint fingerprint =
        dent::fingerprint_problem(
            problem
        );

    const dent::AdaptiveDispatcher dispatcher;

    const dent::DispatchDecision decision =
        dispatcher.dispatch(
            problem,
            fingerprint
        );

    InternalResult result;

    result.solver =
        decision.solver_name;


    switch (decision.method)
    {
        case dent::SolverMethod::PrimalSimplex:
        {
            dent::SimplexSolver solver;

            const dent::SolveResult solved =
                solver.solve(
                    problem
                );

            result.status =
                solved.status;

            result.objective =
                solved.objective_value;

            result.values =
                solved.variable_values;

            result.iterations =
                solved.iterations;

            result.message =
                solved.message;

            break;
        }


        case dent::SolverMethod::InteriorPoint:
        {
            dent::InteriorPointSolver solver;

            const dent::SolveResult solved =
                solver.solve(
                    problem
                );

            result.status =
                solved.status;

            result.objective =
                solved.objective_value;

            result.values =
                solved.variable_values;

            result.iterations =
                solved.iterations;

            result.message =
                solved.message;

            break;
        }


        case dent::SolverMethod::PDHG:
        {
            dent::PDHGSolver solver;

            const dent::SolveResult solved =
                solver.solve(
                    problem
                );

            result.status =
                solved.status;

            result.objective =
                solved.objective_value;

            result.values =
                solved.variable_values;

            result.iterations =
                solved.iterations;

            result.message =
                solved.message;

            break;
        }


        case dent::SolverMethod::PDLP:
        {
            dent::PDLPSolver solver;

            const dent::SolveResult solved =
                solver.solve(
                    problem
                );

            result.status =
                solved.status;

            result.objective =
                solved.objective_value;

            result.values =
                solved.variable_values;

            result.iterations =
                solved.iterations;

            result.message =
                solved.message;

            break;
        }


        case dent::SolverMethod::QP:
        {
            dent::QPSolver solver;

            const dent::QPSolution solved =
                solver.solve(
                    problem
                );

            result.status =
                solved.status;

            result.objective =
                solved.objective_value;

            result.values =
                solved.variable_values;

            result.iterations =
                solved.iterations;

            result.message =
                solved.message;

            break;
        }


        case dent::SolverMethod::MILP:
        {
            dent::MILPSolver solver;

            const dent::MILPSolution solved =
                solver.solve(
                    problem
                );

            result.status =
                solved.status;

            result.objective =
                solved.objective_value;

            result.values =
                solved.variable_values;

            result.iterations =
                solved.nodes_explored;

            result.message =
                solved.message;

            break;
        }


        case dent::SolverMethod::DualSimplex:
        case dent::SolverMethod::Unsupported:
        default:
        {
            result.status =
                dent::SolveStatus::Unsupported;

            result.message =
                "The adaptive dispatcher selected "
                "a solver that is not exposed by "
                "the current public API.";

            break;
        }
    }


    return result;
}


std::string make_json(
    const dent::Problem& problem,
    const InternalResult& result
)
{
    const dent::ProblemFingerprint fingerprint =
        dent::fingerprint_problem(
            problem
        );

    std::ostringstream output;

    output << std::setprecision(17);

    output << "{";

    output
        << "\"status\":\""
        << status_name(
               result.status
           )
        << "\",";

    output
        << "\"objective\":"
        << result.objective
        << ",";

    output
        << "\"solver\":\""
        << json_escape(
               result.solver
           )
        << "\",";

    output
        << "\"iterations\":"
        << result.iterations
        << ",";

    output
        << "\"message\":\""
        << json_escape(
               result.message
           )
        << "\",";


    /*
     * Variable values
     */

    output
        << "\"variables\":[";

    for (
        std::size_t i = 0;
        i < problem.variables().size();
        ++i
    )
    {
        if (i != 0)
        {
            output << ",";
        }

        const double value =
            i < result.values.size()
                ? result.values[i]
                : 0.0;

        output << "{";

        output
            << "\"name\":\""
            << json_escape(
                   problem.variables()[i].name
               )
            << "\",";

        output
            << "\"value\":"
            << value;

        output << "}";
    }

    output << "],";


    /*
     * Problem fingerprint
     */

    output
        << "\"fingerprint\":{";

    output
        << "\"variables\":"
        << fingerprint.variables
        << ",";

    output
        << "\"constraints\":"
        << fingerprint.constraints
        << ",";

    output
        << "\"nonzeros\":"
        << fingerprint.nonzeros
        << ",";

    output
        << "\"density\":"
        << fingerprint.density
        << ",";

    output
        << "\"mixed_integer\":"
        << (
            fingerprint.is_mixed_integer
                ? "true"
                : "false"
        )
        << ",";

    output
        << "\"quadratic\":"
        << (
            fingerprint.has_quadratic_objective
                ? "true"
                : "false"
        );

    output << "}";

    output << "}";

    return output.str();
}

} // namespace


extern "C" const char*
dent_solve_file_json(
    const char* model_path
)
{
    if (
        model_path == nullptr ||
        *model_path == '\0'
    )
    {
        return nullptr;
    }

    try
    {
        const dent::Problem problem =
            dent::ModelParser::parse_file(
                model_path
            );

        const InternalResult result =
            solve_problem(
                problem
            );

        const std::string json =
            make_json(
                problem,
                result
            );

        const std::size_t size =
            json.size() + 1;

        char* buffer =
            static_cast<char*>(
                std::malloc(
                    size
                )
            );

        if (buffer == nullptr)
        {
            return nullptr;
        }

        std::memcpy(
            buffer,
            json.c_str(),
            size
        );

        return buffer;
    }
    catch (...)
    {
        return nullptr;
    }
}


extern "C" void
dent_free_string(
    const char* value
)
{
    std::free(
        const_cast<char*>(
            value
        )
    );
}