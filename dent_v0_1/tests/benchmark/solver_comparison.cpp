#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

#include "dent/io/model_parser.hpp"
#include "dent/solver/interior_point.hpp"
#include "dent/solver/pdhg.hpp"
#include "dent/solver/pdlp.hpp"
#include "dent/solver/simplex.hpp"

namespace fs = std::filesystem;

struct BenchmarkResult
{
    std::string model;
    std::string solver;
    std::string status;

    double objective = 0.0;
    double objective_delta = 0.0;
    double avg_time_ms = 0.0;

    int iterations = 0;
    bool agrees_with_reference = false;
};

static std::string status_name(
    dent::SolveStatus status)
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
    Solver& solver,
    int repetitions,
    double reference_objective,
    dent::SolveStatus reference_status)
{
    BenchmarkResult result;

    result.model = model_name;
    result.solver = solver_name;

    double total_time = 0.0;

    dent::SolveResult final_solution;

    for (int run = 0; run < repetitions; ++run)
    {
        const auto start =
            std::chrono::high_resolution_clock::now();

        final_solution = solver.solve(problem);

        const auto end =
            std::chrono::high_resolution_clock::now();

        total_time +=
            std::chrono::duration<double, std::milli>(
                end - start
            ).count();
    }

    result.status =
        status_name(final_solution.status);

    result.objective =
        final_solution.objective_value;

    result.iterations =
        final_solution.iterations;

    result.avg_time_ms =
        total_time /
        static_cast<double>(repetitions);

    result.objective_delta =
        std::abs(
            result.objective -
            reference_objective
        );

    if (
        final_solution.status ==
        reference_status
    )
    {
        if (
            reference_status ==
            dent::SolveStatus::Optimal
        )
        {
            result.agrees_with_reference =
                result.objective_delta <= 1e-5;
        }
        else
        {
            result.agrees_with_reference = true;
        }
    }

    return result;
}

static void print_result(
    const BenchmarkResult& result)
{
    std::cout
        << std::left
        << std::setw(28)
        << result.model

        << std::setw(20)
        << result.solver

        << std::setw(18)
        << result.status

        << std::setw(15)
        << std::fixed
        << std::setprecision(6)
        << result.objective

        << std::setw(15)
        << std::setprecision(6)
        << result.objective_delta

        << std::setw(13)
        << std::setprecision(4)
        << result.avg_time_ms

        << std::setw(10)
        << result.iterations

        << (result.agrees_with_reference
                ? "YES"
                : "NO")
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
            << path
            << '\n';

        return;
    }

    file
        << "model,"
        << "solver,"
        << "status,"
        << "objective,"
        << "objective_delta,"
        << "avg_time_ms,"
        << "iterations,"
        << "agrees_with_reference\n";

    for (const auto& result : results)
    {
        file
            << '"'
            << result.model
            << "\",\""

            << result.solver
            << "\","

            << result.status
            << ","

            << std::setprecision(12)
            << result.objective
            << ","

            << result.objective_delta
            << ","

            << result.avg_time_ms
            << ","

            << result.iterations
            << ","

            << (
                result.agrees_with_reference
                    ? "YES"
                    : "NO"
            )

            << '\n';
    }

    std::cout
        << "\nCSV written to: "
        << path
        << '\n';
}

static int parse_repetitions(
    int argc,
    char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (
            std::string(argv[i]) ==
            "--repeat"
        )
        {
            if (i + 1 >= argc)
            {
                return 1;
            }

            return std::max(
                1,
                std::stoi(argv[i + 1])
            );
        }
    }

    return 3;
}

int main(
    int argc,
    char** argv)
{
    if (argc < 2)
    {
        std::cout
            << "Usage:\n"
            << "  dent_solver_comparison.exe "
            << "<model1.dent> "
            << "[model2.dent ...] "
            << "--repeat N "
            << "--csv output.csv\n";

        return 1;
    }

    std::vector<std::string> models;

    std::string csv_path;

    const int repetitions =
        parse_repetitions(
            argc,
            argv
        );

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument =
            argv[i];

        if (argument == "--csv")
        {
            if (i + 1 < argc)
            {
                csv_path = argv[++i];
            }

            continue;
        }

        if (argument == "--repeat")
        {
            ++i;
            continue;
        }

        models.push_back(argument);
    }

    if (models.empty())
    {
        std::cerr
            << "ERROR: No model files supplied.\n";

        return 1;
    }

    std::vector<BenchmarkResult> results;

    int models_processed = 0;
    int validation_passes = 0;
    int validation_failures = 0;

    std::cout
        << "\n"
        << "==============================================================\n"
        << "              DENT SOLVER COMPARISON BENCHMARK\n"
        << "==============================================================\n\n";

    std::cout
        << "Repetitions: "
        << repetitions
        << "\n\n";

    std::cout
        << std::left

        << std::setw(28)
        << "MODEL"

        << std::setw(20)
        << "SOLVER"

        << std::setw(18)
        << "STATUS"

        << std::setw(15)
        << "OBJECTIVE"

        << std::setw(15)
        << "DELTA"

        << std::setw(13)
        << "AVG MS"

        << std::setw(10)
        << "ITER"

        << "MATCH"
        << '\n';

    std::cout
        << std::string(130, '-')
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

        dent::Problem problem;

        try
        {
            problem =
                dent::ModelParser::parse_file(
                    model_path
                );
        }
        catch (const std::exception& error)
        {
            std::cerr
                << "ERROR: Failed to parse "
                << model_path
                << ": "
                << error.what()
                << '\n';

            continue;
        }

        const std::string model_name =
            fs::path(
                model_path
            ).filename().string();

        ++models_processed;

        /*
         * ========================================================
         * REFERENCE SOLUTION
         * ========================================================
         */

        dent::SimplexSolver reference_solver(
            1e-9,
            10000
        );

        const dent::SolveResult reference =
            reference_solver.solve(
                problem
            );

        std::cout
            << "\nReference: "
            << model_name
            << " -> "
            << status_name(
                   reference.status
               )
            << " / "
            << std::fixed
            << std::setprecision(6)
            << reference.objective_value
            << "\n\n";

        /*
         * ========================================================
         * PRIMAL SIMPLEX
         * ========================================================
         */

        {
            dent::SimplexSolver solver(
                1e-9,
                10000
            );

            auto result =
                run_solver(
                    model_name,
                    problem,
                    "Primal Simplex",
                    solver,
                    repetitions,
                    reference.objective_value,
                    reference.status
                );

            print_result(result);

            results.push_back(result);

            if (result.agrees_with_reference)
                ++validation_passes;
            else
                ++validation_failures;
        }

        /*
         * ========================================================
         * INTERIOR POINT
         * ========================================================
         */

        {
            dent::InteriorPointSolver solver(
                1e-7,
                10000
            );

            auto result =
                run_solver(
                    model_name,
                    problem,
                    "Interior Point",
                    solver,
                    repetitions,
                    reference.objective_value,
                    reference.status
                );

            print_result(result);

            results.push_back(result);

            if (result.agrees_with_reference)
                ++validation_passes;
            else
                ++validation_failures;
        }

        /*
         * ========================================================
         * PDHG
         * ========================================================
         */

        {
            dent::PDHGSolver solver(
                1e-7,
                30000
            );

            auto result =
                run_solver(
                    model_name,
                    problem,
                    "PDHG",
                    solver,
                    repetitions,
                    reference.objective_value,
                    reference.status
                );

            print_result(result);

            results.push_back(result);

            if (result.agrees_with_reference)
                ++validation_passes;
            else
                ++validation_failures;
        }

        /*
         * ========================================================
         * PDLP
         * ========================================================
         */

        {
            dent::PDLPSolver solver(
                1e-7,
                50000
            );

            auto result =
                run_solver(
                    model_name,
                    problem,
                    "PDLP",
                    solver,
                    repetitions,
                    reference.objective_value,
                    reference.status
                );

            print_result(result);

            results.push_back(result);

            if (result.agrees_with_reference)
                ++validation_passes;
            else
                ++validation_failures;
        }

        std::cout
            << std::string(130, '-')
            << '\n';
    }

    if (!csv_path.empty())
    {
        write_csv(
            csv_path,
            results
        );
    }

    std::cout
        << "\n==============================================================\n"
        << "                    BENCHMARK SUMMARY\n"
        << "==============================================================\n\n"

        << "Models processed       : "
        << models_processed
        << "\n"

        << "Solver validations     : "
        << validation_passes
        << "\n"

        << "Validation failures    : "
        << validation_failures
        << "\n"

        << "Total solver runs      : "
        << results.size()
        << "\n"

        << "Repetitions per solver : "
        << repetitions
        << "\n";

    if (validation_failures == 0)
    {
        std::cout
            << "\nAll solver results agree with "
               "the Simplex reference.\n";
    }
    else
    {
        std::cout
            << "\nWARNING: Some solver results "
               "do not match the Simplex reference.\n";
    }

    return 0;
}