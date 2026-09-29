#ifndef DENT_BUILDING_DLL
#define DENT_BUILDING_DLL
#endif

#include "dent/api/c_api.hpp"

#include "dent/dispatch/dispatcher.hpp"
#include "dent/dispatch/fingerprint.hpp"
#include "dent/io/model_parser.hpp"
#include "dent/io/mps_parser.hpp"
#include "dent/io/lp_parser.hpp"
#include "dent/solver/dual_simplex.hpp"
#include "dent/io/model_parser.hpp"
#include "dent/model/problem.hpp"
#include "dent/solver/interior_point.hpp"
#include "dent/solver/milp.hpp"
#include "dent/solver/miqp.hpp"
#include "dent/solver/pdhg.hpp"
#include "dent/solver/pdlp.hpp"
#include "dent/solver/qp.hpp"
#include "dent/solver/simplex.hpp"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


/*
 * ============================================================
 * Internal model handle
 * ============================================================
 */

struct dent_model_t
{
    explicit dent_model_t(
        dent::ObjectiveSense sense
    )
        : problem(sense)
    {
    }

    dent::Problem problem;
};


/*
 * ============================================================
 * Thread-local error storage
 * ============================================================
 */

namespace
{

thread_local std::string g_last_error;


void clear_error()
{
    g_last_error.clear();
}


void set_error(
    const std::string& message
)
{
    g_last_error = message;
}


void set_error(
    const char* message
)
{
    g_last_error =
        message != nullptr
            ? message
            : "Unknown DENT API error.";
}


/*
 * ============================================================
 * JSON helpers
 * ============================================================
 */

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


/*
 * ============================================================
 * Internal solve result
 * ============================================================
 */

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


/*
 * ============================================================
 * Dispatcher + solver bridge
 * ============================================================
 */

InternalResult solve_problem(
    const dent::Problem& problem,
    int solver_method = DENT_SOLVER_AUTO,
    double tolerance = 0.0,
    int max_iterations = 0
)
{
    const dent::ProblemFingerprint fingerprint =
        dent::fingerprint_problem(
            problem
        );

    dent::SolverMethod method =
        dent::SolverMethod::Unsupported;

    std::string solver_name;

    if (solver_method == DENT_SOLVER_AUTO)
    {
        const dent::AdaptiveDispatcher dispatcher;

        const dent::DispatchDecision decision =
            dispatcher.dispatch(
                problem,
                fingerprint
            );

        method = decision.method;
        solver_name = decision.solver_name;
    }
    else
    {
        switch (solver_method)
        {
            case DENT_SOLVER_PRIMAL_SIMPLEX:
                method = dent::SolverMethod::PrimalSimplex;
                solver_name = "Primal Simplex";
                break;

            case DENT_SOLVER_DUAL_SIMPLEX:
                method = dent::SolverMethod::DualSimplex;
                solver_name = "Dual Simplex";
                break;

            case DENT_SOLVER_INTERIOR_POINT:
                method = dent::SolverMethod::InteriorPoint;
                solver_name = "Interior Point";
                break;

            case DENT_SOLVER_PDHG:
                method = dent::SolverMethod::PDHG;
                solver_name = "PDHG";
                break;

            case DENT_SOLVER_PDLP:
                method = dent::SolverMethod::PDLP;
                solver_name = "PDLP";
                break;

            case DENT_SOLVER_QP:
                method = dent::SolverMethod::QP;
                solver_name = "QP";
                break;

            case DENT_SOLVER_MILP:
                method = dent::SolverMethod::MILP;
                solver_name = "MILP";
                break;
            case DENT_SOLVER_MIQP:
                method = dent::SolverMethod::MIQP;
                solver_name = "MIQP";
                break;

            default:
                throw std::invalid_argument(
                    "Invalid DENT solver method."
                );
        }
    }

    InternalResult result;

    result.solver =
        solver_name;


    switch (method)
    {
        case dent::SolverMethod::PrimalSimplex:
        {
            dent::SimplexSolver solver(
                tolerance > 0.0 ? tolerance : 1e-9,
                max_iterations > 0 ? max_iterations : 10000
            );

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
            dent::InteriorPointSolver solver(
                tolerance > 0.0 ? tolerance : 1e-8,
                max_iterations > 0 ? max_iterations : 100
            );

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
            dent::PDHGSolver solver(
                tolerance > 0.0 ? tolerance : 1e-7,
                max_iterations > 0 ? max_iterations : 10000
            );

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
            dent::PDLPSolver solver(
                tolerance > 0.0 ? tolerance : 1e-7,
                max_iterations > 0 ? max_iterations : 20000
            );

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
            dent::QPSolver solver(
                tolerance > 0.0 ? tolerance : 1e-8,
                max_iterations > 0 ? max_iterations : 10000
            );

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


        case dent::SolverMethod::MIQP:
        {
            dent::MIQPSolver solver(
                tolerance > 0.0 ? tolerance : 1e-8,
                max_iterations > 0 ? max_iterations : 1000
            );

            const dent::MIQPSolution solved =
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


        case dent::SolverMethod::MILP:
        {
            dent::MILPSolver solver(
                tolerance > 0.0 ? tolerance : 1e-9,
                max_iterations > 0 ? max_iterations : 1000
            );

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
        {
            dent::DualSimplexSolver solver(
                tolerance > 0.0 ? tolerance : 1e-9,
                max_iterations > 0 ? max_iterations : 10000
            );
            const dent::SolveResult solved = solver.solve(problem);
            result.status = solved.status;
            result.objective = solved.objective_value;
            result.values = solved.variable_values;
            result.iterations = solved.iterations;
            result.message = solved.message;
            break;
        }

        case dent::SolverMethod::Unsupported:
        default:
        {
            result.status = dent::SolveStatus::Unsupported;
            result.message =
                "The requested solver is not exposed by "
                "the current native solve bridge.";
            break;
        }
    }


    return result;
}


/*
 * ============================================================
 * JSON serialization
 * ============================================================
 */

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


/*
 * ============================================================
 * JSON allocation
 * ============================================================
 */

const char* allocate_json(
    const std::string& json
)
{
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
        set_error(
            "DENT API could not allocate the JSON result."
        );

        return nullptr;
    }

    std::memcpy(
        buffer,
        json.c_str(),
        size
    );

    return buffer;
}


/*
 * ============================================================
 * Validation helpers
 * ============================================================
 */

bool valid_model(
    const dent_model_t* model
)
{
    if (model == nullptr)
    {
        set_error(
            "DENT model handle is null."
        );

        return false;
    }

    return true;
}


bool valid_name(
    const char* name
)
{
    if (
        name == nullptr ||
        *name == '\0'
    )
    {
        set_error(
            "DENT model name cannot be null or empty."
        );

        return false;
    }

    return true;
}


bool valid_variable_type(
    int variable_type
)
{
    if (
        variable_type != DENT_VARIABLE_CONTINUOUS &&
        variable_type != DENT_VARIABLE_INTEGER &&
        variable_type != DENT_VARIABLE_BINARY
    )
    {
        set_error(
            "Invalid DENT variable type."
        );

        return false;
    }

    return true;
}


bool valid_constraint_sense(
    int constraint_sense
)
{
    if (
        constraint_sense != DENT_CONSTRAINT_LESS_EQUAL &&
        constraint_sense != DENT_CONSTRAINT_EQUAL &&
        constraint_sense != DENT_CONSTRAINT_GREATER_EQUAL
    )
    {
        set_error(
            "Invalid DENT constraint sense."
        );

        return false;
    }

    return true;
}


dent::VariableType convert_variable_type(
    int variable_type
)
{
    switch (variable_type)
    {
        case DENT_VARIABLE_INTEGER:
            return dent::VariableType::Integer;

        case DENT_VARIABLE_BINARY:
            return dent::VariableType::Binary;

        case DENT_VARIABLE_CONTINUOUS:
        default:
            return dent::VariableType::Continuous;
    }
}


dent::ConstraintSense convert_constraint_sense(
    int constraint_sense
)
{
    switch (constraint_sense)
    {
        case DENT_CONSTRAINT_EQUAL:
            return dent::ConstraintSense::Equal;

        case DENT_CONSTRAINT_GREATER_EQUAL:
            return dent::ConstraintSense::GreaterEqual;

        case DENT_CONSTRAINT_LESS_EQUAL:
        default:
            return dent::ConstraintSense::LessEqual;
    }
}

} // namespace


/*
 * ============================================================
 * Public C API
 * ============================================================
 */

extern "C" DENT_API dent_model_t*
dent_model_create(
    int maximize
)
{
    clear_error();

    try
    {
        return new dent_model_t(
            maximize
                ? dent::ObjectiveSense::Maximize
                : dent::ObjectiveSense::Minimize
        );
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return nullptr;
    }
    catch (...)
    {
        set_error(
            "Unknown error creating DENT model."
        );

        return nullptr;
    }
}


extern "C" DENT_API dent_model_t*
dent_model_create_from_file(
    const char* model_path
)
{
    clear_error();

    if (
        model_path == nullptr ||
        *model_path == '\0'
    )
    {
        set_error(
            "DENT model path cannot be null or empty."
        );

        return nullptr;
    }

    try
    {
        const std::string path(model_path);
        std::string ext;
        const std::size_t dot = path.find_last_of('.');
        if (dot != std::string::npos) {
            ext = path.substr(dot);
            for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        dent::Problem problem;
        if (ext == ".mps" || ext == ".qps") problem = dent::MPSParser::parse_file(path);
        else if (ext == ".lp") problem = dent::LPParser::parse_file(path);
        else problem = dent::ModelParser::parse_file(path);

        dent_model_t* model =
            new dent_model_t(
                problem.objective_sense()
            );

        model->problem =
            std::move(
                problem
            );

        return model;
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return nullptr;
    }
    catch (...)
    {
        set_error(
            "Unknown error parsing DENT model file."
        );

        return nullptr;
    }
}


extern "C" DENT_API void
dent_model_destroy(
    dent_model_t* model
)
{
    clear_error();

    delete model;
}


extern "C" DENT_API int
dent_model_add_variable(
    dent_model_t* model,
    const char* name,
    double lower_bound,
    double upper_bound,
    int variable_type
)
{
    clear_error();

    if (!valid_model(model))
    {
        return -1;
    }

    if (!valid_name(name))
    {
        return -1;
    }

    if (!valid_variable_type(variable_type))
    {
        return -1;
    }

    try
    {
        return model->problem.add_variable(
            name,
            lower_bound,
            upper_bound,
            convert_variable_type(
                variable_type
            )
        );
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return -1;
    }
    catch (...)
    {
        set_error(
            "Unknown error adding DENT variable."
        );

        return -1;
    }
}


extern "C" DENT_API int
dent_model_add_constraint(
    dent_model_t* model,
    const char* name,
    int constraint_sense,
    double rhs
)
{
    clear_error();

    if (!valid_model(model))
    {
        return -1;
    }

    if (!valid_name(name))
    {
        return -1;
    }

    if (!valid_constraint_sense(constraint_sense))
    {
        return -1;
    }

    try
    {
        return model->problem.add_constraint(
            name,
            convert_constraint_sense(
                constraint_sense
            ),
            rhs
        );
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return -1;
    }
    catch (...)
    {
        set_error(
            "Unknown error adding DENT constraint."
        );

        return -1;
    }
}


extern "C" DENT_API int
dent_model_set_objective_coefficient(
    dent_model_t* model,
    int variable,
    double coefficient
)
{
    clear_error();

    if (!valid_model(model))
    {
        return -1;
    }

    try
    {
        model->problem.set_objective_coefficient(
            variable,
            coefficient
        );

        return 0;
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return -1;
    }
    catch (...)
    {
        set_error(
            "Unknown error setting DENT objective coefficient."
        );

        return -1;
    }
}


extern "C" DENT_API int
dent_model_set_constraint_coefficient(
    dent_model_t* model,
    int constraint,
    int variable,
    double coefficient
)
{
    clear_error();

    if (!valid_model(model))
    {
        return -1;
    }

    try
    {
        model->problem.set_constraint_coefficient(
            constraint,
            variable,
            coefficient
        );

        return 0;
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return -1;
    }
    catch (...)
    {
        set_error(
            "Unknown error setting DENT constraint coefficient."
        );

        return -1;
    }
}


extern "C" DENT_API int
dent_model_set_quadratic_coefficient(
    dent_model_t* model,
    int row_variable,
    int column_variable,
    double coefficient
)
{
    clear_error();

    if (!valid_model(model))
    {
        return -1;
    }

    try
    {
        model->problem.set_quadratic_coefficient(
            row_variable,
            column_variable,
            coefficient
        );

        return 0;
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return -1;
    }
    catch (...)
    {
        set_error(
            "Unknown error setting DENT quadratic coefficient."
        );

        return -1;
    }
}


extern "C" DENT_API int
dent_model_set_variable_type(
    dent_model_t* model,
    int variable,
    int variable_type
)
{
    clear_error();

    if (!valid_model(model))
    {
        return -1;
    }

    if (!valid_variable_type(variable_type))
    {
        return -1;
    }

    try
    {
        model->problem.set_variable_type(
            variable,
            convert_variable_type(
                variable_type
            )
        );

        return 0;
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return -1;
    }
    catch (...)
    {
        set_error(
            "Unknown error setting DENT variable type."
        );

        return -1;
    }
}


extern "C" DENT_API int
dent_model_set_variable_bounds(
    dent_model_t* model,
    int variable,
    double lower_bound,
    double upper_bound
)
{
    clear_error();

    if (!valid_model(model))
    {
        return -1;
    }

    try
    {
        model->problem.set_variable_bounds(
            variable,
            lower_bound,
            upper_bound
        );

        return 0;
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return -1;
    }
    catch (...)
    {
        set_error(
            "Unknown error setting DENT variable bounds."
        );

        return -1;
    }
}


extern "C" DENT_API const char*
dent_model_solve_json(
    const dent_model_t* model
)
{
    clear_error();

    if (!valid_model(model))
    {
        return nullptr;
    }

    try
    {
        const InternalResult result =
            solve_problem(
                model->problem
            );

        const std::string json =
            make_json(
                model->problem,
                result
            );

        return allocate_json(
            json
        );
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return nullptr;
    }
    catch (...)
    {
        set_error(
            "Unknown error solving DENT model."
        );

        return nullptr;
    }
}


extern "C" DENT_API const char*
dent_model_solve_json_with_options(
    const dent_model_t* model,
    int solver_method,
    double tolerance,
    int max_iterations
)
{
    clear_error();

    if (!valid_model(model))
    {
        return nullptr;
    }

    if (tolerance < 0.0)
    {
        set_error("DENT solver tolerance cannot be negative.");
        return nullptr;
    }

    if (max_iterations < 0)
    {
        set_error("DENT solver iteration limit cannot be negative.");
        return nullptr;
    }

    try
    {
        const InternalResult result =
            solve_problem(
                model->problem,
                solver_method,
                tolerance,
                max_iterations
            );

        const std::string json =
            make_json(
                model->problem,
                result
            );

        return allocate_json(json);
    }
    catch (const std::exception& exception)
    {
        set_error(exception.what());
        return nullptr;
    }
    catch (...)
    {
        set_error("Unknown error solving DENT model.");
        return nullptr;
    }
}


extern "C" DENT_API const char*
dent_solve_file_json(
    const char* model_path
)
{
    clear_error();

    if (
        model_path == nullptr ||
        *model_path == '\0'
    )
    {
        set_error(
            "DENT model path cannot be null or empty."
        );

        return nullptr;
    }

    try
    {
        const std::string path(model_path);
        std::string ext;
        const std::size_t dot = path.find_last_of('.');
        if (dot != std::string::npos) {
            ext = path.substr(dot);
            for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        dent::Problem problem;
        if (ext == ".mps" || ext == ".qps") problem = dent::MPSParser::parse_file(path);
        else if (ext == ".lp") problem = dent::LPParser::parse_file(path);
        else problem = dent::ModelParser::parse_file(path);

        const InternalResult result =
            solve_problem(
                problem
            );

        const std::string json =
            make_json(
                problem,
                result
            );

        return allocate_json(
            json
        );
    }
    catch (const std::exception& exception)
    {
        set_error(
            exception.what()
        );

        return nullptr;
    }
    catch (...)
    {
        set_error(
            "Unknown error solving DENT model file."
        );

        return nullptr;
    }
}


extern "C" DENT_API const char*
dent_solve_file_json_with_options(
    const char* model_path,
    int solver_method,
    double tolerance,
    int max_iterations
)
{
    clear_error();

    if (model_path == nullptr || *model_path == '\0')
    {
        set_error("DENT model path cannot be null or empty.");
        return nullptr;
    }

    if (tolerance < 0.0)
    {
        set_error("DENT solver tolerance cannot be negative.");
        return nullptr;
    }

    if (max_iterations < 0)
    {
        set_error("DENT solver iteration limit cannot be negative.");
        return nullptr;
    }

    try
    {
        const dent::Problem problem =
            dent::ModelParser::parse_file(model_path);

        const InternalResult result =
            solve_problem(
                problem,
                solver_method,
                tolerance,
                max_iterations
            );

        const std::string json =
            make_json(problem, result);

        return allocate_json(json);
    }
    catch (const std::exception& exception)
    {
        set_error(exception.what());
        return nullptr;
    }
    catch (...)
    {
        set_error("Unknown error solving DENT model file.");
        return nullptr;
    }
}


extern "C" DENT_API const char*
dent_last_error(
    void
)
{
    return g_last_error.c_str();
}


extern "C" DENT_API void
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