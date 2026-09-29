#include "dent/solver/miqp.hpp"

#include <cmath>
#include <limits>
#include <queue>

namespace dent {

MIQPSolver::MIQPSolver(double tolerance, int max_nodes)
    : tolerance_(tolerance), max_nodes_(max_nodes) {}

bool MIQPSolver::integral(const Problem& p, const std::vector<double>& x) const {
    if (x.size() != p.variables().size()) return false;
    for (std::size_t j=0;j<x.size();++j) {
        const auto t=p.variables()[j].type;
        if (t==VariableType::Integer || t==VariableType::Binary) {
            if (std::abs(x[j]-std::round(x[j]))>tolerance_) return false;
        }
    }
    return true;
}

int MIQPSolver::branch_variable(const Problem& p, const std::vector<double>& x) const {
    int best=-1; double frac=0.0;
    for (std::size_t j=0;j<x.size();++j) {
        const auto t=p.variables()[j].type;
        if (t!=VariableType::Integer && t!=VariableType::Binary) continue;
        const double f=std::abs(x[j]-std::round(x[j]));
        if (f>frac+tolerance_) { frac=f; best=static_cast<int>(j); }
    }
    return best;
}

bool MIQPSolver::better(ObjectiveSense s,double a,double b) const {
    return s==ObjectiveSense::Minimize ? a<b-tolerance_ : a>b+tolerance_;
}

bool MIQPSolver::can_improve(ObjectiveSense s,double bound,double incumbent) const {
    return s==ObjectiveSense::Minimize ? bound<incumbent-tolerance_ : bound>incumbent+tolerance_;
}

Problem MIQPSolver::branch_down(const Problem& p,int j,double value) const {
    Problem q=p;
    const int row=q.add_constraint("__dent_miqp_down_"+std::to_string(j)+"_"+std::to_string(q.constraints().size()),
                                   ConstraintSense::LessEqual,std::floor(value));
    q.set_constraint_coefficient(row,j,1.0);
    return q;
}

Problem MIQPSolver::branch_up(const Problem& p,int j,double value) const {
    Problem q=p;
    const int row=q.add_constraint("__dent_miqp_up_"+std::to_string(j)+"_"+std::to_string(q.constraints().size()),
                                   ConstraintSense::GreaterEqual,std::ceil(value));
    q.set_constraint_coefficient(row,j,1.0);
    return q;
}

MIQPSolution MIQPSolver::solve(const Problem& problem) const {
    MIQPSolution out;
    if (!problem.quadratic_matrix().size()) { out.message="MIQP requires a quadratic objective."; return out; }

    struct Node { Problem p; double bound; int depth; };
    struct Cmp {
        ObjectiveSense s;
        bool operator()(const Node&a,const Node&b) const {
            return s==ObjectiveSense::Minimize ? a.bound>b.bound : a.bound<b.bound;
        }
    };

    std::priority_queue<Node,std::vector<Node>,Cmp> open((Cmp{problem.objective_sense()}));
    open.push({problem, problem.objective_sense()==ObjectiveSense::Minimize ?
        -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity(), 0});

    bool has_incumbent=false;
    double incumbent = problem.objective_sense()==ObjectiveSense::Minimize ?
        std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
    std::vector<double> incumbent_x;

    while (!open.empty() && out.nodes_explored<max_nodes_) {
        Node node=open.top(); open.pop();
        QPSolver qp(tolerance_,10000);
        const QPSolution r=qp.solve(node.p);
        ++out.nodes_explored;
        if (r.status==SolveStatus::Infeasible || r.status==SolveStatus::Unsupported) { ++out.nodes_pruned; continue; }
        if (r.status==SolveStatus::Unbounded) { out.status=SolveStatus::Unbounded; out.message="Continuous QP relaxation is unbounded."; return out; }

        const double bound=r.objective_value;
        if (has_incumbent && !can_improve(problem.objective_sense(),bound,incumbent)) { ++out.nodes_pruned; continue; }

        const int j=branch_variable(node.p,r.variable_values);
        if (j<0 && integral(problem,r.variable_values)) {
            if (!has_incumbent || better(problem.objective_sense(),bound,incumbent)) {
                has_incumbent=true; incumbent=bound; incumbent_x=r.variable_values;
            }
            continue;
        }
        if (j<0) { ++out.nodes_pruned; continue; }

        open.push({branch_down(node.p,j,r.variable_values[j]),bound,node.depth+1});
        open.push({branch_up(node.p,j,r.variable_values[j]),bound,node.depth+1});
    }

    if (!has_incumbent) {
        out.status=(out.nodes_explored>=max_nodes_)?SolveStatus::IterationLimit:SolveStatus::Infeasible;
        out.message=(out.nodes_explored>=max_nodes_)?"MIQP node limit reached without an incumbent.":"MIQP infeasible.";
        return out;
    }
    out.status=(open.empty()?SolveStatus::Optimal:SolveStatus::IterationLimit);
    out.objective_value=incumbent; out.variable_values=incumbent_x;
    out.relative_gap=0.0;
    out.message="MIQP branch-and-bound completed using QP relaxations.";
    return out;
}

} // namespace dent
