#pragma once

#include "dent/model/problem.hpp"
#include "dent/solver/simplex.hpp"

#include <cstddef>
#include <queue>
#include <string>
#include <vector>

namespace dent {

struct MILPSolution
{
    SolveStatus status =
        SolveStatus::Unsupported;

    double objective_value =
        0.0;

    std::vector<double> variable_values;

    int nodes_explored =
        0;

    int nodes_pruned =
        0;

    int lp_solves =
        0;

    int warm_start_lp_solves =
        0;

    int heuristic_attempts =
        0;

    int heuristic_incumbents =
        0;

    int cuts_generated =
        0;

    int cuts_added =
        0;

    int strong_branching_solves =
        0;

    int max_open_nodes =
        0;

    double best_bound =
        0.0;

    double relative_gap =
        0.0;

    std::string message;
};


class MILPSolver
{
public:

    explicit MILPSolver(
        double tolerance = 1e-9,
        int max_nodes = 1000
    );

    MILPSolution solve(
        const Problem& problem
    ) const;

private:

    struct Node
    {
        Problem problem;

        double bound =
            0.0;

        int depth =
            0;

        std::size_t sequence =
            0;
    };


    struct NodeCompare
    {
        ObjectiveSense sense =
            ObjectiveSense::Maximize;

        bool operator()(
            const Node& left,
            const Node& right
        ) const;
    };


    struct LPNodeResult
    {
        SolveResult solve_result;

        Problem relaxation;
    };


    struct BranchCandidate
    {
        int variable =
            -1;

        double value =
            0.0;

        double fractionality =
            0.0;

        double down_gain =
            0.0;

        double up_gain =
            0.0;

        double score =
            0.0;
    };


    double tolerance_;

    int max_nodes_;


    bool is_integer_problem(
        const Problem& problem
    ) const;


    bool is_integral_solution(
        const Problem& problem,
        const std::vector<double>& values
    ) const;


    bool check_feasibility(
        const Problem& problem,
        const std::vector<double>& values
    ) const;


    Problem build_lp_relaxation(
        const Problem& original
    ) const;


    LPNodeResult solve_node_lp(
        const Problem& problem,
        MILPSolution& result
    ) const;


    bool try_rounding_heuristic(
        const Problem& problem,
        const std::vector<double>& lp_values,
        std::vector<double>& integer_values
    ) const;


    bool try_diving_heuristic(
        const Problem& problem,
        const std::vector<double>& start_values,
        std::vector<double>& integer_values,
        MILPSolution& result
    ) const;


    bool add_cover_cuts(
        Problem& problem,
        const std::vector<double>& lp_values,
        MILPSolution& result
    ) const;


    std::vector<BranchCandidate>
    build_branch_candidates(
        const Problem& problem,
        const std::vector<double>& values
    ) const;


    BranchCandidate choose_strong_branch(
        const Problem& problem,
        const std::vector<BranchCandidate>& candidates,
        double parent_bound,
        MILPSolution& result
    ) const;


    Problem make_branch_down(
        const Problem& problem,
        int variable,
        double value
    ) const;


    Problem make_branch_up(
        const Problem& problem,
        int variable,
        double value
    ) const;


    bool better_objective(
        ObjectiveSense sense,
        double candidate,
        double incumbent
    ) const;


    bool bound_can_improve(
        ObjectiveSense sense,
        double bound,
        bool has_incumbent,
        double incumbent
    ) const;


    double calculate_relative_gap(
        ObjectiveSense sense,
        double incumbent,
        double bound
    ) const;
};

} // namespace dent