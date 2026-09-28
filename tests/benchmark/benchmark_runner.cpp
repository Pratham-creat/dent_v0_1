#include "dent/dispatch/dispatcher.hpp"
#include "dent/dispatch/fingerprint.hpp"
#include "dent/io/model_parser.hpp"
#include "dent/model/problem.hpp"
#include "dent/presolve/presolve.hpp"
#include "dent/solver/dual_simplex.hpp"
#include "dent/solver/interior_point.hpp"
#include "dent/solver/milp.hpp"
#include "dent/solver/pdhg.hpp"
#include "dent/solver/pdlp.hpp"
#include "dent/solver/qp.hpp"
#include "dent/solver/simplex.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{

const char* status_name(dent::SolveStatus status)
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
            return "ITERATION_LIMIT";

        case dent::SolveStatus::Unsupported:
            return "UNSUPPORTED";
    }

    return "UNKNOWN";
}


bool valid_result(dent::SolveStatus status)
{
    return
        status == dent::SolveStatus::Optimal ||
        status == dent::SolveStatus::Infeasible ||
        status == dent::SolveStatus::Unbounded ||
        status == dent::SolveStatus::IterationLimit;
}


struct BenchmarkResult
{
    std::string file;
    std::string solver;

    dent::SolveStatus status =
        dent::SolveStatus::Unsupported;

    double objective = 0.0;
    double time_ms = 0.0;

    int iterations = 0;
    int nodes = 0;
    int lp_solves = 0;

    bool error = false;
};


BenchmarkResult run_once(
    const std::string& filename
)
{
    using Clock = std::chrono::steady_clock;

    BenchmarkResult result;

    result.file = filename;

    dent::Problem problem =
        dent::ModelParser::parse_file(filename);

    dent::Presolver presolver(
        1e-9,
        5
    );

    dent::PresolveResult presolve =
        presolver.run(problem);

    if (
        presolve.status !=
        dent::PresolveStatus::Success
    )
    {
        result.solver = "Presolve";

        result.status =
            presolve.status ==
                    dent::PresolveStatus::Infeasible
                ? dent::SolveStatus::Infeasible
                : dent::SolveStatus::Unsupported;

        result.error =
            presolve.status ==
            dent::PresolveStatus::Unsupported;

        return result;
    }

    const dent::Problem& solve_problem =
        presolve.reduced_problem;

    dent::ProblemFingerprint fingerprint =
        dent::fingerprint_problem(
            solve_problem
        );

    dent::AdaptiveDispatcher dispatcher;

    dent::DispatchDecision decision =
        dispatcher.dispatch(
            solve_problem,
            fingerprint
        );

    result.solver =
        decision.solver_name;

    if (
        decision.method ==
        dent::SolverMethod::Unsupported
    )
    {
        result.status =
            dent::SolveStatus::Unsupported;

        result.error = true;

        return result;
    }

    auto start =
        Clock::now();

    dent::SolveStatus status =
        dent::SolveStatus::Unsupported;

    double objective = 0.0;
    int iterations = 0;
    int nodes = 0;
    int lp_solves = 0;


    if (
        decision.method ==
        dent::SolverMethod::PrimalSimplex
    )
    {
        dent::SimplexSolver solver(
            1e-9,
            10000
        );

        dent::SolveResult solve =
            solver.solve(solve_problem);

        status = solve.status;

        if (
            status ==
            dent::SolveStatus::Optimal
        )
        {
            objective =
                presolve.postsolve_objective(
                    solve.objective_value
                );
        }

        iterations = solve.iterations;
    }
    else if (
        decision.method ==
        dent::SolverMethod::DualSimplex
    )
    {
        dent::DualSimplexSolver solver(
            1e-9,
            10000
        );

        dent::SolveResult solve =
            solver.solve(solve_problem);

        status = solve.status;

        if (
            status ==
            dent::SolveStatus::Optimal
        )
        {
            objective =
                presolve.postsolve_objective(
                    solve.objective_value
                );
        }

        iterations = solve.iterations;
    }
    else if (
        decision.method ==
        dent::SolverMethod::InteriorPoint
    )
    {
        dent::InteriorPointSolver solver(
            1e-9,
            1000
        );

        dent::SolveResult solve =
            solver.solve(solve_problem);

        status = solve.status;

        if (
            status ==
            dent::SolveStatus::Optimal
        )
        {
            objective =
                presolve.postsolve_objective(
                    solve.objective_value
                );
        }

        iterations = solve.iterations;
    }
    else if (
        decision.method ==
        dent::SolverMethod::PDHG
    )
    {
        dent::PDHGSolver solver(
            1e-7,
            30000
        );

        dent::SolveResult solve =
            solver.solve(solve_problem);

        status = solve.status;

        if (
            status ==
            dent::SolveStatus::Optimal
        )
        {
            objective =
                presolve.postsolve_objective(
                    solve.objective_value
                );
        }

        iterations = solve.iterations;
    }
    else if (
        decision.method ==
        dent::SolverMethod::PDLP
    )
    {
        dent::PDLPSolver solver(
            1e-7,
            50000
        );

        dent::SolveResult solve =
            solver.solve(solve_problem);

        status = solve.status;

        if (
            status ==
            dent::SolveStatus::Optimal
        )
        {
            objective =
                presolve.postsolve_objective(
                    solve.objective_value
                );
        }

        iterations = solve.iterations;
    }
    else if (
        decision.method ==
        dent::SolverMethod::MILP
    )
    {
        dent::MILPSolver solver(
            1e-9,
            1000
        );

        dent::MILPSolution solve =
            solver.solve(solve_problem);

        status = solve.status;

        if (
            status ==
            dent::SolveStatus::Optimal
        )
        {
            objective =
                presolve.postsolve_objective(
                    solve.objective_value
                );
        }

        nodes = solve.nodes_explored;
        lp_solves = solve.lp_solves;
    }
    else if (
        decision.method ==
        dent::SolverMethod::QP
    )
    {
        dent::QPSolver solver(
            1e-9,
            100
        );

        dent::QPSolution solve =
            solver.solve(solve_problem);

        status = solve.status;

        if (
            status ==
            dent::SolveStatus::Optimal
        )
        {
            objective =
                presolve.postsolve_objective(
                    solve.objective_value
                );
        }

        iterations = solve.iterations;
    }


    auto end =
        Clock::now();

    result.status = status;

    result.objective = objective;

    result.time_ms =
        std::chrono::duration<double, std::milli>(
            end - start
        ).count();

    result.iterations = iterations;
    result.nodes = nodes;
    result.lp_solves = lp_solves;

    result.error =
        !valid_result(status);

    return result;
}


BenchmarkResult run_benchmark(
    const std::string& filename,
    int repeats
)
{
    BenchmarkResult final_result;

    double total_time = 0.0;

    int successful_runs = 0;

    for (
        int run = 0;
        run < repeats;
        ++run
    )
    {
        BenchmarkResult result =
            run_once(filename);

        final_result = result;

        if (result.error)
        {
            return result;
        }

        total_time += result.time_ms;

        ++successful_runs;
    }

    if (successful_runs > 0)
    {
        final_result.time_ms =
            total_time /
            static_cast<double>(
                successful_runs
            );
    }

    return final_result;
}


void print_header()
{
    std::cout
        << "\n"
        << "DENT BENCHMARK RUNNER\n"
        << "=====================\n\n";

    std::cout
        << std::left
        << std::setw(28)
        << "MODEL"
        << std::setw(22)
        << "SOLVER"
        << std::setw(18)
        << "STATUS"
        << std::right
        << std::setw(14)
        << "OBJECTIVE"
        << std::setw(14)
        << "AVG MS"
        << std::setw(10)
        << "ITER"
        << std::setw(10)
        << "NODES"
        << std::setw(10)
        << "LP"
        << "\n";

    std::cout
        << std::string(146, '-')
        << "\n";
}


void print_result(
    const BenchmarkResult& result
)
{
    std::string model =
        result.file;

    if (model.size() > 27)
    {
        model =
            model.substr(
                model.size() - 27
            );
    }

    std::string solver =
        result.solver;

    if (solver.size() > 21)
    {
        solver =
            solver.substr(
                0,
                21
            );
    }

    std::cout
        << std::left
        << std::setw(28)
        << model
        << std::setw(22)
        << solver
        << std::setw(18)
        << status_name(result.status)
        << std::right;

    if (
        result.status ==
        dent::SolveStatus::Optimal
    )
    {
        std::cout
            << std::fixed
            << std::setprecision(4)
            << std::setw(14)
            << result.objective;
    }
    else
    {
        std::cout
            << std::setw(14)
            << "-";
    }

    std::cout
        << std::fixed
        << std::setprecision(4)
        << std::setw(14)
        << result.time_ms
        << std::setw(10)
        << result.iterations
        << std::setw(10)
        << result.nodes
        << std::setw(10)
        << result.lp_solves
        << "\n";
}


void write_csv_header(
    std::ofstream& file
)
{
    file
        << "model,solver,status,objective,time_ms,"
           "iterations,nodes,lp_solves\n";
}


void write_csv_result(
    std::ofstream& file,
    const BenchmarkResult& result
)
{
    file
        << '"'
        << result.file
        << "\","
        << '"'
        << result.solver
        << "\","
        << status_name(result.status)
        << ",";

    if (
        result.status ==
        dent::SolveStatus::Optimal
    )
    {
        file
            << std::setprecision(12)
            << result.objective;
    }
    else
    {
        file << "";
    }

    file
        << ","
        << std::setprecision(12)
        << result.time_ms
        << ","
        << result.iterations
        << ","
        << result.nodes
        << ","
        << result.lp_solves
        << "\n";
}

} // namespace


int main(
    int argc,
    char* argv[]
)
{
    if (argc < 2)
    {
        std::cerr
            << "Usage:\n\n"
            << "  dent_benchmark model1.dent "
               "[model2.dent ...]\n\n"
            << "Options:\n"
            << "  --repeat N\n"
            << "  --csv FILE\n\n"
            << "Example:\n"
            << "  dent_benchmark production.dent "
               "parser_test_binary.dent "
               "--repeat 10 "
               "--csv benchmark.csv\n";

        return EXIT_FAILURE;
    }


    int repeats = 1;

    std::string csv_file;

    std::vector<std::string> models;


    for (
        int i = 1;
        i < argc;
        ++i
    )
    {
        std::string argument =
            argv[i];

        if (
            argument ==
            "--repeat"
        )
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "Missing value for --repeat.\n";

                return EXIT_FAILURE;
            }

            repeats =
                std::atoi(
                    argv[++i]
                );

            if (repeats < 1)
            {
                std::cerr
                    << "--repeat must be >= 1.\n";

                return EXIT_FAILURE;
            }

            continue;
        }


        if (
            argument ==
            "--csv"
        )
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "Missing filename for --csv.\n";

                return EXIT_FAILURE;
            }

            csv_file =
                argv[++i];

            continue;
        }


        models.push_back(
            argument
        );
    }


    if (models.empty())
    {
        std::cerr
            << "No benchmark models supplied.\n";

        return EXIT_FAILURE;
    }


    std::ofstream csv;

    if (!csv_file.empty())
    {
        csv.open(csv_file);

        if (!csv)
        {
            std::cerr
                << "Could not create CSV file: "
                << csv_file
                << "\n";

            return EXIT_FAILURE;
        }

        write_csv_header(csv);
    }


    print_header();

    int errors = 0;
    int completed = 0;


    for (
        const std::string& model :
        models
    )
    {
        try
        {
            BenchmarkResult result =
                run_benchmark(
                    model,
                    repeats
                );

            print_result(result);

            if (!csv_file.empty())
            {
                write_csv_result(
                    csv,
                    result
                );
            }

            if (result.error)
            {
                ++errors;
            }
            else
            {
                ++completed;
            }
        }
        catch (
            const std::exception& error
        )
        {
            BenchmarkResult result;

            result.file = model;
            result.solver = "ERROR";
            result.status =
                dent::SolveStatus::Unsupported;
            result.error = true;

            print_result(result);

            if (!csv_file.empty())
            {
                write_csv_result(
                    csv,
                    result
                );
            }

            std::cerr
                << "\nError while benchmarking "
                << model
                << ": "
                << error.what()
                << "\n";

            ++errors;
        }
    }


    if (csv.is_open())
    {
        csv.close();

        std::cout
            << "\nCSV results written to: "
            << csv_file
            << "\n";
    }


    std::cout
        << "\n"
        << "Benchmark complete.\n"
        << "Models processed : "
        << models.size()
        << "\n"
        << "Valid results    : "
        << completed
        << "\n"
        << "Actual errors    : "
        << errors
        << "\n"
        << "Repetitions      : "
        << repeats
        << "\n";


    return errors == 0
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}