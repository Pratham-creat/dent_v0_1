#include "dent/dispatch/dispatcher.hpp"
#include "dent/dispatch/fingerprint.hpp"
#include "dent/io/interactive_builder.hpp"
#include "dent/io/model_parser.hpp"
#include "dent/model/problem.hpp"
#include "dent/presolve/presolve.hpp"
#include "dent/solver/dual_simplex.hpp"
#include "dent/solver/interior_point.hpp"
#include "dent/solver/milp.hpp"
#include "dent/solver/qp.hpp"
#include "dent/solver/simplex.hpp"

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

const char* solve_status_name(
    dent::SolveStatus status
)
{
    switch (status)
    {
        case dent::SolveStatus::Optimal:
            return "OPTIMAL";

        case dent::SolveStatus::Infeasible:
            return "INFEASIBLE";

        case dent::SolveStatus::Unbounded:
            return "UNBOUNDED";

        case dent::SolveStatus::IterationLimit:
            return "ITERATION LIMIT";

        case dent::SolveStatus::Unsupported:
            return "UNSUPPORTED";
    }

    return "UNKNOWN";
}


const char* presolve_status_name(
    dent::PresolveStatus status
)
{
    switch (status)
    {
        case dent::PresolveStatus::Success:
            return "SUCCESS";

        case dent::PresolveStatus::Infeasible:
            return "INFEASIBLE";

        case dent::PresolveStatus::Unsupported:
            return "UNSUPPORTED";
    }

    return "UNKNOWN";
}


void print_presolve(
    const dent::PresolveResult& result
)
{
    const auto& statistics =
        result.statistics;

    std::cout
        << "\n========================================\n"
        << "             PRESOLVE\n"
        << "========================================\n\n";

    std::cout
        << "Status: "
        << presolve_status_name(
               result.status
           )
        << "\n\n";

    std::cout
        << "Variables:\n"
        << "  Original : "
        << statistics.original_variables
        << "\n"
        << "  Reduced  : "
        << statistics.reduced_variables
        << "\n"
        << "  Removed  : "
        << statistics.variables_removed
        << "\n\n";

    std::cout
        << "Constraints:\n"
        << "  Original : "
        << statistics.original_constraints
        << "\n"
        << "  Reduced  : "
        << statistics.reduced_constraints
        << "\n"
        << "  Removed  : "
        << statistics.constraints_removed
        << "\n\n";

    std::cout
        << "Presolve operations:\n"
        << "  Fixed variables      : "
        << statistics.fixed_variables
        << "\n"
        << "  Bounds tightened     : "
        << statistics.bounds_tightened
        << "\n"
        << "  Trivial constraints  : "
        << statistics.trivial_constraints_removed
        << "\n"
        << "  Rows scaled          : "
        << statistics.rows_scaled
        << "\n\n";

    std::cout
        << "Message:\n"
        << "  "
        << result.message
        << "\n";
}


void print_fingerprint(
    const dent::ProblemFingerprint& fingerprint
)
{
    const double sparsity =
        1.0 - fingerprint.density;

    std::cout
        << "\n========================================\n"
        << "       PROBLEM FINGERPRINT\n"
        << "========================================\n\n";

    std::cout
        << std::fixed
        << std::setprecision(6);

    std::cout
        << "Dimensions:\n"
        << "  Variables             : "
        << fingerprint.variables
        << "\n"
        << "  Constraints           : "
        << fingerprint.constraints
        << "\n"
        << "  Nonzeros              : "
        << fingerprint.nonzeros
        << "\n\n";

    std::cout
        << "Matrix structure:\n"
        << "  Density               : "
        << fingerprint.density
        << "\n"
        << "  Sparsity              : "
        << sparsity
        << "\n"
        << "  Highly sparse         : "
        << (
            fingerprint.highly_sparse
                ? "YES"
                : "NO"
        )
        << "\n"
        << "  Large problem         : "
        << (
            fingerprint.large_problem
                ? "YES"
                : "NO"
        )
        << "\n\n";

    std::cout
        << "Coefficient range:\n"
        << "  Minimum nonzero       : "
        << fingerprint.min_nonzero_coefficient
        << "\n"
        << "  Maximum nonzero       : "
        << fingerprint.max_nonzero_coefficient
        << "\n"
        << "  Coefficient ratio     : "
        << fingerprint.coefficient_ratio
        << "\n\n";

    std::cout
        << "Row magnitude:\n"
        << "  Minimum row norm      : "
        << fingerprint.min_row_norm
        << "\n"
        << "  Maximum row norm      : "
        << fingerprint.max_row_norm
        << "\n"
        << "  Row norm ratio        : "
        << fingerprint.row_norm_ratio
        << "\n\n";

    std::cout
        << "Column magnitude:\n"
        << "  Minimum column norm   : "
        << fingerprint.min_column_norm
        << "\n"
        << "  Maximum column norm   : "
        << fingerprint.max_column_norm
        << "\n"
        << "  Column norm ratio     : "
        << fingerprint.column_norm_ratio
        << "\n\n";

    std::cout
        << "RHS range:\n"
        << "  Minimum RHS           : "
        << fingerprint.min_rhs
        << "\n"
        << "  Maximum RHS           : "
        << fingerprint.max_rhs
        << "\n"
        << "  RHS ratio             : "
        << fingerprint.rhs_ratio
        << "\n\n";

    std::cout
        << "Numerical classification:\n"
        << "  Poorly scaled         : "
        << (
            fingerprint.poorly_scaled
                ? "YES"
                : "NO"
        )
        << "\n\n";

    std::cout
        << "Variable structure:\n"
        << "  Continuous            : "
        << fingerprint.continuous_variables
        << "\n"
        << "  Integer               : "
        << fingerprint.integer_variables
        << "\n"
        << "  Binary                : "
        << fingerprint.binary_variables
        << "\n"
        << "  Bounded               : "
        << fingerprint.bounded_variables
        << "\n"
        << "  Unbounded             : "
        << fingerprint.unbounded_variables
        << "\n\n";

    std::cout
        << "Constraint structure:\n"
        << "  Less-than/equal       : "
        << fingerprint.less_equal_constraints
        << "\n"
        << "  Equal                 : "
        << fingerprint.equal_constraints
        << "\n"
        << "  Greater-than/equal    : "
        << fingerprint.greater_equal_constraints
        << "\n\n";

    std::cout
        << "Quadratic structure:\n"
        << "  Quadratic objective   : "
        << (
            fingerprint.has_quadratic_objective
                ? "YES"
                : "NO"
        )
        << "\n"
        << "  Quadratic nonzeros    : "
        << fingerprint.quadratic_nonzeros
        << "\n\n";

    std::cout
        << "Classification:\n"
        << "  Structure             : "
        << fingerprint.structure
        << "\n";
}


void print_dispatch(
    const dent::DispatchDecision& decision
)
{
    std::cout
        << "\n========================================\n"
        << "          ADAPTIVE DISPATCH\n"
        << "========================================\n\n";

    std::cout
        << "Selected method : "
        << decision.solver_name
        << "\n";

    std::cout
        << "Execution       : "
        << (
            decision.use_gpu
                ? "GPU"
                : "CPU"
        )
        << "\n";

    std::cout
        << "CPU available   : "
        << (
            decision.use_cpu
                ? "YES"
                : "NO"
        )
        << "\n\n";

    std::cout
        << "Reason:\n"
        << "  "
        << decision.reason
        << "\n";
}


void print_solution(
    const dent::Problem& problem,
    const std::vector<double>& values,
    double objective,
    dent::SolveStatus status,
    const std::string& message
)
{
    std::cout
        << "\n========================================\n"
        << "             SOLUTION\n"
        << "========================================\n\n";

    std::cout
        << "Status: "
        << solve_status_name(status)
        << "\n\n";

    std::cout
        << std::fixed
        << std::setprecision(6);

    std::cout
        << "Objective value: "
        << objective
        << "\n\n";

    std::cout
        << "Decision values:\n";

    const auto& variables =
        problem.variables();

    for (std::size_t i = 0;
         i < variables.size() &&
         i < values.size();
         ++i)
    {
        std::cout
            << "  "
            << variables[i].name
            << " = "
            << values[i]
            << "\n";
    }

    std::cout
        << "\n"
        << message
        << "\n";
}

} // namespace


int main(
    int argc,
    char* argv[]
)
{
    using namespace dent;

    try
    {
        // ========================================================
        // DENT BANNER
        // ========================================================

        std::cout
            << "\n========================================\n"
            << "       DENT OPTIMIZATION ENGINE\n"
            << "========================================\n\n";

        std::cout
            << "DENT converts real-world planning problems\n"
            << "into mathematical optimization models.\n\n";

        // ========================================================
        // MODEL INPUT
        // ========================================================

        Problem original_problem;

        if (argc >= 2)
        {
            std::cout
                << "Loading DENT model: "
                << argv[1]
                << "\n\n";

            original_problem =
                ModelParser::parse_file(
                    argv[1]
                );

            std::cout
                << "Model loaded successfully.\n";
        }
        else
        {
            std::cout
                << "You do NOT need to write equations.\n"
                << "Simply provide your business information.\n";

            original_problem =
                InteractiveBuilder::build();
        }

        std::cout
            << "\n========================================\n"
            << "             MODEL READY\n"
            << "========================================\n";

        // ========================================================
        // PRESOLVE
        // ========================================================

        std::cout
            << "\n========================================\n"
            << "          PRESOLVE STAGE\n"
            << "========================================\n";

        Presolver presolver(
            1e-9,
            5
        );

        PresolveResult presolve =
            presolver.run(
                original_problem
            );

        print_presolve(
            presolve
        );

        if (
            presolve.status ==
            PresolveStatus::Infeasible
        )
        {
            std::cout
                << "\nDENT stopped because presolve "
                   "proved the model infeasible.\n";

            return 0;
        }

        if (
            presolve.status ==
            PresolveStatus::Unsupported
        )
        {
            std::cout
                << "\nDENT could not safely presolve "
                   "this model.\n";

            return 0;
        }

        // ========================================================
        // FINGERPRINT
        // ========================================================

        ProblemFingerprint fingerprint =
            fingerprint_problem(
                presolve.reduced_problem
            );

        print_fingerprint(
            fingerprint
        );

        // ========================================================
        // ADAPTIVE DISPATCH
        // ========================================================

        AdaptiveDispatcher dispatcher;

        DispatchDecision decision =
            dispatcher.dispatch(
                presolve.reduced_problem,
                fingerprint
            );

        print_dispatch(
            decision
        );

        if (
            decision.method ==
            SolverMethod::Unsupported
        )
        {
            std::cout
                << "\nDENT cannot solve this problem "
                   "with the currently implemented "
                   "solver set.\n";

            return 0;
        }

        // ========================================================
        // SOLVER
        // ========================================================

        const Problem& solve_problem =
            presolve.reduced_problem;

        // ========================================================
        // PRIMAL SIMPLEX
        // ========================================================

        if (
            decision.method ==
            SolverMethod::PrimalSimplex
        )
        {
            std::cout
                << "\nStarting DENT Primal Simplex solver...\n";

            SimplexSolver solver(
                1e-9,
                10000
            );

            SolveResult result =
                solver.solve(
                    solve_problem
                );

            std::vector<double> original_values =
                presolve.postsolve_values(
                    result.variable_values
                );

            double original_objective =
                presolve.postsolve_objective(
                    result.objective_value
                );

            print_solution(
                original_problem,
                original_values,
                original_objective,
                result.status,
                result.message.empty()
                    ? "Primal Simplex completed."
                    : result.message
            );

            return 0;
        }

        // ========================================================
        // DUAL SIMPLEX
        // ========================================================

        if (
            decision.method ==
            SolverMethod::DualSimplex
        )
        {
            std::cout
                << "\nStarting DENT Dual Simplex solver...\n";

            DualSimplexSolver solver(
                1e-9,
                10000
            );

            SolveResult result =
                solver.solve(
                    solve_problem
                );

            std::vector<double> original_values =
                presolve.postsolve_values(
                    result.variable_values
                );

            double original_objective =
                presolve.postsolve_objective(
                    result.objective_value
                );

            print_solution(
                original_problem,
                original_values,
                original_objective,
                result.status,
                result.message.empty()
                    ? "Dual Simplex completed."
                    : result.message
            );

            return 0;
        }

        // ========================================================
        // INTERIOR POINT
        // ========================================================

        if (
            decision.method ==
            SolverMethod::InteriorPoint
        )
        {
            std::cout
                << "\nStarting DENT Interior Point solver...\n";

            InteriorPointSolver solver(
                1e-9,
                1000
            );

            SolveResult result =
                solver.solve(
                    solve_problem
                );

            std::vector<double> original_values =
                presolve.postsolve_values(
                    result.variable_values
                );

            double original_objective =
                presolve.postsolve_objective(
                    result.objective_value
                );

            print_solution(
                original_problem,
                original_values,
                original_objective,
                result.status,
                result.message.empty()
                    ? "Interior Point completed."
                    : result.message
            );

            return 0;
        }

        // ========================================================
        // MILP
        // ========================================================

        if (
            decision.method ==
            SolverMethod::MILP
        )
        {
            std::cout
                << "\nStarting DENT MILP solver...\n";

            MILPSolver solver(
                1e-9,
                1000
            );

            MILPSolution result =
                solver.solve(
                    solve_problem
                );

            std::vector<double> original_values =
                presolve.postsolve_values(
                    result.variable_values
                );

            double original_objective =
                presolve.postsolve_objective(
                    result.objective_value
                );

            std::cout
                << "\n========================================\n"
                << "             SOLUTION\n"
                << "========================================\n\n";

            std::cout
                << "Status: "
                << solve_status_name(
                       result.status
                   )
                << "\n\n";

            std::cout
                << std::fixed
                << std::setprecision(6);

            std::cout
                << "Objective value: "
                << original_objective
                << "\n\n";

            std::cout
                << "Decision values:\n";

            const auto& variables =
                original_problem.variables();

            for (std::size_t i = 0;
                 i < variables.size() &&
                 i < original_values.size();
                 ++i)
            {
                std::cout
                    << "  "
                    << variables[i].name
                    << " = "
                    << original_values[i]
                    << "\n";
            }

            std::cout
                << "\n"
                << "Nodes explored: "
                << result.nodes_explored
                << "\n"
                << "Nodes pruned: "
                << result.nodes_pruned
                << "\n"
                << "LP relaxations: "
                << result.lp_solves
                << "\n\n"
                << result.message
                << "\n";

            return 0;
        }

        // ========================================================
        // QP
        // ========================================================

        if (
            decision.method ==
            SolverMethod::QP
        )
        {
            std::cout
                << "\nStarting DENT QP solver...\n";

            QPSolver solver(
                1e-9,
                100
            );

            QPSolution result =
                solver.solve(
                    solve_problem
                );

            std::vector<double> original_values =
                presolve.postsolve_values(
                    result.variable_values
                );

            double original_objective =
                presolve.postsolve_objective(
                    result.objective_value
                );

            print_solution(
                original_problem,
                original_values,
                original_objective,
                result.status,
                result.message.empty()
                    ? "QP completed."
                    : result.message
            );

            return 0;
        }

        // ========================================================
        // UNKNOWN
        // ========================================================

        std::cout
            << "\nUnexpected dispatcher state.\n";

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "\nDENT ERROR: "
            << error.what()
            << "\n";

        return 1;
    }
}