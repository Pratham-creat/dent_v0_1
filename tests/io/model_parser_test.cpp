#include "dent/io/model_parser.hpp"
#include "dent/model/problem.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

int passed = 0;
int failed = 0;

void check(
    bool condition,
    const std::string& name
)
{
    if (condition)
    {
        std::cout
            << "[PASS] "
            << name
            << '\n';

        ++passed;
    }
    else
    {
        std::cout
            << "[FAIL] "
            << name
            << '\n';

        ++failed;
    }
}

void write_file(
    const std::string& filename,
    const std::string& content
)
{
    std::ofstream output(
        filename
    );

    if (!output.is_open())
    {
        throw std::runtime_error(
            "Could not create parser test file."
        );
    }

    output << content;
}

} // namespace

int main()
{
    std::cout
        << "============================================================\n"
        << " DENT .DENT PARSER REGRESSION SUITE\n"
        << "============================================================\n\n";

    /*
        --------------------------------------------------------
        Test 1
        Existing production format.
        --------------------------------------------------------
    */
    {
        const std::string filename =
            "parser_test_production.dent";

        write_file(
            filename,
            R"(PROBLEM: Production Planning

MAXIMIZE
    40 ProductA + 30 ProductB

SUBJECT TO
    2 ProductA + ProductB <= 100
    ProductA + 2 ProductB <= 80

INTEGER
    ProductA ProductB
)"
        );

        dent::Problem problem =
            dent::ModelParser::parse_file(
                filename
            );

        check(
            problem.variables().size() == 2,
            "Production variable count"
        );

        check(
            problem.constraints().size() == 2,
            "Production constraint count"
        );

        check(
            problem.objective_sense() ==
                dent::ObjectiveSense::Maximize,
            "Production objective sense"
        );

        check(
            problem.variables()[0].type ==
                dent::VariableType::Integer &&
            problem.variables()[1].type ==
                dent::VariableType::Integer,
            "Production integer declarations"
        );

        check(
            std::abs(
                problem.objective()[0] - 40.0
            ) < 1e-12 &&
            std::abs(
                problem.objective()[1] - 30.0
            ) < 1e-12,
            "Production objective coefficients"
        );
    }

    /*
        --------------------------------------------------------
        Test 2
        Negative coefficients and compact multiplication.
        --------------------------------------------------------
    */
    {
        const std::string filename =
            "parser_test_negative.dent";

        write_file(
            filename,
            R"(PROBLEM: Negative Coefficients

MINIMIZE
    -2x + 3*y - z

SUBJECT TO
    -x + 2y + z >= 5
    x + y - z = 2

CONTINUOUS
    x y z
)"
        );

        dent::Problem problem =
            dent::ModelParser::parse_file(
                filename
            );

        check(
            problem.variables().size() == 3,
            "Negative-expression variable count"
        );

        check(
            std::abs(
                problem.objective()[0] + 2.0
            ) < 1e-12 &&
            std::abs(
                problem.objective()[1] - 3.0
            ) < 1e-12 &&
            std::abs(
                problem.objective()[2] + 1.0
            ) < 1e-12,
            "Negative-expression objective"
        );

        check(
            problem.constraints()[0].sense ==
                dent::ConstraintSense::GreaterEqual,
            "Greater-equal parsing"
        );

        check(
            problem.constraints()[1].sense ==
                dent::ConstraintSense::Equal,
            "Equality parsing"
        );
    }

    /*
        --------------------------------------------------------
        Test 3
        Binary variables.
        --------------------------------------------------------
    */
    {
        const std::string filename =
            "parser_test_binary.dent";

        write_file(
            filename,
            R"(PROBLEM: Binary Selection

MAX
    10 x + 5 y

CONSTRAINTS
    x + y <= 1

BINARY
    x y
)"
        );

        dent::Problem problem =
            dent::ModelParser::parse_file(
                filename
            );

        check(
            problem.variables().size() == 2,
            "Binary variable count"
        );

        check(
            problem.variables()[0].type ==
                dent::VariableType::Binary &&
            problem.variables()[1].type ==
                dent::VariableType::Binary,
            "Binary declarations"
        );

        check(
            problem.variables()[0].upper_bound == 1.0 &&
            problem.variables()[1].upper_bound == 1.0,
            "Binary upper bounds"
        );
    }

    /*
        --------------------------------------------------------
        Test 4
        Constraint labels and comments.
        --------------------------------------------------------
    */
    {
        const std::string filename =
            "parser_test_comments.dent";

        write_file(
            filename,
            R"(# model
PROBLEM: Labelled Model

MAXIMIZE
    5 x + 7 y # objective

SUBJECT TO
    capacity: 2 x + y <= 10
    demand: x + 2 y >= 4

INTEGER
    x y
)"
        );

        dent::Problem problem =
            dent::ModelParser::parse_file(
                filename
            );

        check(
            problem.constraints().size() == 2,
            "Comment and label parsing"
        );

        check(
            std::abs(
                problem.matrix()[0][0] - 2.0
            ) < 1e-12 &&
            std::abs(
                problem.matrix()[0][1] - 1.0
            ) < 1e-12,
            "Labelled first constraint"
        );

        check(
            std::abs(
                problem.matrix()[1][0] - 1.0
            ) < 1e-12 &&
            std::abs(
                problem.matrix()[1][1] - 2.0
            ) < 1e-12,
            "Labelled second constraint"
        );
    }

    /*
        --------------------------------------------------------
        Test 5
        Duplicate terms must accumulate.
        --------------------------------------------------------
    */
    {
        const std::string filename =
            "parser_test_duplicate_terms.dent";

        write_file(
            filename,
            R"(PROBLEM: Duplicate Terms

MAXIMIZE
    2 x + 3 x + y

SUBJECT TO
    x + 2 x + y <= 10

CONTINUOUS
    x y
)"
        );

        dent::Problem problem =
            dent::ModelParser::parse_file(
                filename
            );

        check(
            std::abs(
                problem.objective()[0] - 5.0
            ) < 1e-12,
            "Duplicate objective terms accumulate"
        );

        check(
            std::abs(
                problem.matrix()[0][0] - 3.0
            ) < 1e-12,
            "Duplicate constraint terms accumulate"
        );
    }

    /*
        --------------------------------------------------------
        Test 6
        Invalid file must throw.
        --------------------------------------------------------
    */
    {
        const std::string filename =
            "parser_test_invalid.dent";

        write_file(
            filename,
            R"(PROBLEM: Invalid

MAXIMIZE
    5 x

SUBJECT TO
    x ??? 10

CONTINUOUS
    x
)"
        );

        bool threw = false;

        try
        {
            (void)dent::ModelParser::parse_file(
                filename
            );
        }
        catch (const std::exception&)
        {
            threw = true;
        }

        check(
            threw,
            "Invalid constraint rejected"
        );
    }

    std::cout
        << "\n============================================================\n"
        << " TEST SUMMARY\n"
        << "============================================================\n";

    std::cout
        << "Passed : "
        << passed
        << '\n';

    std::cout
        << "Failed : "
        << failed
        << '\n';

    std::cout
        << "Total  : "
        << passed + failed
        << '\n';

    if (failed == 0)
    {
        std::cout
            << "\nDENT .dent parser regression suite PASSED.\n";

        return 0;
    }

    std::cout
        << "\nDENT .dent parser regression suite FAILED.\n";

    return 1;
}