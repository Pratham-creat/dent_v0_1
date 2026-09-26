#include <cmath>
#include <iostream>
#include <string>

#include "dent/io/model_parser.hpp"
#include "dent/solver/pdlp.hpp"

namespace
{

bool approximately_equal(
    double a,
    double b,
    double tolerance = 1e-5)
{
    return std::abs(a - b) <= tolerance;
}

bool test_model(
    const std::string& name,
    const std::string& path,
    double expected_objective)
{
    std::cout
        << "\n[TEST] "
        << name
        << '\n';

    try
    {
        dent::Problem problem =
            dent::ModelParser::parse_file(path);

        dent::PDLPSolver solver(
            1e-7,
            50000);

        dent::SolveResult result =
            solver.solve(problem);

        std::cout
            << "  Status    : ";

        switch (result.status)
        {
            case dent::SolveStatus::Optimal:
                std::cout << "OPTIMAL";
                break;

            case dent::SolveStatus::Infeasible:
                std::cout << "INFEASIBLE";
                break;

            case dent::SolveStatus::Unbounded:
                std::cout << "UNBOUNDED";
                break;

            case dent::SolveStatus::IterationLimit:
                std::cout << "ITERATION_LIMIT";
                break;

            case dent::SolveStatus::Unsupported:
                std::cout << "UNSUPPORTED";
                break;
        }

        std::cout
            << '\n'
            << "  Objective : "
            << result.objective_value
            << '\n'
            << "  Iterations: "
            << result.iterations
            << '\n';

        if (
            result.status !=
            dent::SolveStatus::Optimal)
        {
            std::cout
                << "  FAIL: solver did not reach OPTIMAL.\n";

            return false;
        }

        if (
            !approximately_equal(
                result.objective_value,
                expected_objective))
        {
            std::cout
                << "  FAIL: objective mismatch.\n"
                << "  Expected  : "
                << expected_objective
                << '\n'
                << "  Actual    : "
                << result.objective_value
                << '\n';

            return false;
        }

        std::cout
            << "  PASS\n";

        return true;
    }
    catch (const std::exception& error)
    {
        std::cout
            << "  FAIL: "
            << error.what()
            << '\n';

        return false;
    }
}

} // namespace

int main()
{
    std::cout
        << "========================================\n"
        << "           DENT PDLP TESTS\n"
        << "========================================\n";

    int passed = 0;
    int failed = 0;

    /*
     * Production planning.
     *
     * Expected optimum:
     * 4059.142857
     */
    if (
        test_model(
            "LP Production",
            "benchmarks/lp_production.dent",
            4059.142857))
    {
        ++passed;
    }
    else
    {
        ++failed;
    }

    /*
     * Petrochemical blending.
     *
     * Expected optimum:
     * 15912.592593
     */
    if (
        test_model(
            "LP Blending",
            "benchmarks/lp_blending.dent",
            15912.592593))
    {
        ++passed;
    }
    else
    {
        ++failed;
    }

    /*
     * Transportation.
     *
     * Expected optimum:
     * 1190.0
     */
    if (
        test_model(
            "LP Transportation",
            "benchmarks/lp_transportation.dent",
            1190.0))
    {
        ++passed;
    }
    else
    {
        ++failed;
    }

    std::cout
        << "\n========================================\n"
        << "SUMMARY\n"
        << "========================================\n"
        << "Passed: "
        << passed
        << '\n'
        << "Failed: "
        << failed
        << '\n';

    if (failed == 0)
    {
        std::cout
            << "\nAll PDLP tests passed.\n";

        return 0;
    }

    std::cout
        << "\nPDLP tests failed.\n";

    return 1;
}