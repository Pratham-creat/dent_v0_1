#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "model/model.hpp"
#include "solver/interior_point.hpp"
#include "solver/pdhg.hpp"
#include "solver/pdlp.hpp"
#include "solver/simplex.hpp"
#include "io/model_parser.hpp"

namespace fs = std::filesystem;

struct BenchmarkResult
{
    std::string model;
    std::string solver;
    std::string status;
    double objective = 0.0;
    double time_ms = 0.0;
    int iterations = 0;
};

static std::string status_name(dent::SolveStatus status)
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

template <typename Solver>
BenchmarkResult run_solver(
    const std::string& model_name,
    const dent::Problem& problem,
    const std::string& solver_name,
    Solver& solver)
{
    BenchmarkResult result;

    result.model = model_name;
    result.solver = solver_name;

    const auto start = std::chrono::high_resolution_clock::now();

    const dent::SolveResult solution = solver.solve(problem);

    const auto end = std::chrono::high_resolution_clock::now();

    result.time_ms =
        std::chrono::duration<double, std::milli>(
            end - start).count();

    result.status = status_name(solution.status);
    result.objective = solution.objective_value;
    result.iterations = solution.iterations;

    return result;
}

static void print_result(const BenchmarkResult& result)
{
    std::cout
        << std::left
        << std::setw(30) << result.model
        << std::setw(22) << result.solver
        << std::setw(18) << result.status
        << std::setw(16) << std::fixed
        << std::setprecision(6)
        << result.objective
        << std::setw(14)
        << std::setprecision(4)
        << result.time_ms
        << std::setw(12)
        << result.iterations
        << '\n';
}

static void write_csv(
    const std::string& path,
    const std::vector<BenchmarkResult>& results)
{
    std::ofstream file(path);

    if (!file)
    {
        std::cerr
            << "ERROR: Could not create CSV file: "
            << path << '\n';

        return;
    }

    file << "model,solver,status,objective,time_ms,iterations\n";

    for (const auto& result : results)
    {
        file << '"'
             << result.model
             << "\",\""
             << result.solver
             << "\","
             << result.status
             << ","
             << std::setprecision(12)
             << result.objective
             << ","
             << result.time_ms
             << ","
             << result.iterations
             << '\n';
    }

    std::cout
        << "\nCSV written to: "
        << path
        << '\n';
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout
            << "Usage:\n"
            << "  dent_solver_comparison.exe "
               "<model1.dent> [model2.dent ...] "
               "[--csv output.csv]\n";

        return 1;
    }

    std::vector<std::string> models;
    std::string csv_path;

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];

        if (argument == "--csv")
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "ERROR: --csv requires a filename.\n";

                return 1;
            }

            csv_path = argv[++i];
        }
        else
        {
            models.push_back(argument);
        }
    }

    if (models.empty())
    {
        std::cerr
            << "ERROR: No model files supplied.\n";

        return 1;
    }

    std::vector<BenchmarkResult> results;

    std::cout
        << "\n"
        << "============================================================\n"
        << "             DENT SOLVER COMPARISON BENCHMARK\n"
        << "============================================================\n\n";

    std::cout
        << std::left
        << std::setw(30) << "MODEL"
        << std::setw(22) << "SOLVER"
        << std::setw(18) << "STATUS"
        << std::setw(16) << "OBJECTIVE"
        << std::setw(14) << "TIME MS"
        << std::setw(12) << "ITER"
        << '\n';

    std::cout
        << std::string(112, '-')
        << '\n';

    for (const auto& model_path : models)
    {
        if (!fs::exists(model_path))
        {
            std::cerr
                << "ERROR: Model does not exist: "
                << model_path
                << '\n';

            continue;
        }

        dent::ModelParser parser;

        const auto parse_result =
            parser.parse(model_path);

        if (!parse_result.success)
        {
            std::cerr
                << "ERROR: Failed to parse "
                << model_path
                << '\n'
                << "       "
                << parse_result.message
                << '\n';

            continue;
        }

        const dent::Problem& problem =
            parse_result.problem;

        const std::string model_name =
            fs::path(model_path).filename().string();

        {
            dent::SimplexSolver solver;

            auto result = run_solver(
                model_name,
                problem,
                "Primal Simplex",
                solver);

            print_result(result);
            results.push_back(result);
        }

        {
            dent::InteriorPointSolver solver(
                1e-7,
                10000);

            auto result = run_solver(
                model_name,
                problem,
                "Interior Point",
                solver);

            print_result(result);
            results.push_back(result);
        }

        {
            dent::PDHGSolver solver(
                1e-7,
                30000);

            auto result = run_solver(
                model_name,
                problem,
                "PDHG",
                solver);

            print_result(result);
            results.push_back(result);
        }

        {
            dent::PDLPSolver solver(
                1e-7,
                50000);

            auto result = run_solver(
                model_name,
                problem,
                "PDLP",
                solver);

            print_result(result);
            results.push_back(result);
        }

        std::cout
            << std::string(112, '-')
            << '\n';
    }

    if (!csv_path.empty())
    {
        write_csv(csv_path, results);
    }

    std::cout
        << "\n============================================================\n"
        << "                    BENCHMARK COMPLETE\n"
        << "============================================================\n";

    return 0;
}