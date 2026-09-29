#include "dent/solver/parallel_milp.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <algorithm>
namespace dent {
MILPSolution ParallelMILPSolver::solve(const Problem&p)const{
 const int workers=std::max(1,options_.workers?options_.workers:(int)std::thread::hardware_concurrency());
 if(workers==1)return MILPSolver(options_.tolerance,options_.max_nodes).solve(p);
 std::atomic<int> next(0);std::mutex mu;MILPSolution best;bool have=false;
 auto worker=[&](){while(true){int ticket=next.fetch_add(1);if(ticket>=workers)break;MILPSolver s(options_.tolerance,std::max(1,options_.max_nodes/workers));auto r=s.solve(p);std::lock_guard<std::mutex>g(mu);if(!have||(r.status==SolveStatus::Optimal&&(best.status!=SolveStatus::Optimal||(p.objective_sense()==ObjectiveSense::Minimize?r.objective_value<best.objective_value:r.objective_value>best.objective_value)))){best=r;have=true;}}};
 std::vector<std::thread> ts;for(int i=0;i<workers;++i)ts.emplace_back(worker);for(auto&t:ts)t.join();best.message="Parallel MILP portfolio completed with "+std::to_string(workers)+" workers.";return best;
}}