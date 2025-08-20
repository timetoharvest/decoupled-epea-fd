#include "lp_factoring.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../factoring.h"
#include "../lp/lp_solver.h"
#include "../task_utils/causal_graph.h"
#include "../globals.h"
#include "../operator.h"
#include "../utils/hash.h"
#include "../utils/timer.h"

#include <algorithm>
#include <iostream>


using namespace std;


static const string CPLEX_ERROR_MEMORY_1 = "CPLEX Error  1001: Out of memory.\n";
static const string CPLEX_ERROR_MEMORY_2 = "not enough memory";

namespace lp_factoring {
LPFactoring::LPFactoring(const options::Options &opts) : Factoring(opts),
        cplex_time_limit(opts.get<int>("cplex_time_limit")),
        memory_limit(opts.get<int>("memory_limit")),
        lp_solver_type(opts.get<lp::LPSolverType>("lpsolver")),
        timer(utils::Timer()) {
    lp::LPSolver solver(lp_solver_type);
    infty = solver.get_infinity();
}

bool LPFactoring::check_lp_size(size_t num_constraints, size_t /*num_variables*/) const {
    if (memory_limit != -1 && num_constraints >= (size_t) memory_limit * 2500){
        // this limit corresponds to around 10 million constraints with 4GB memory limit
        // the maximum-sized ILP that could be solved in the experiments with this limit
        // had around 5 million constraints
        cerr << "Too many LP constraints, aborting: " << num_constraints << endl;
        return false;
    }
    return true;
}

bool LPFactoring::check_timeout() const {
    if (factoring_timer.is_expired()){
        exit_if_timeout();
        return false;
    }
    return true;
}

vector<double> LPFactoring::solve_lp(lp::LinearProgram &lp,
        bool,
        bool solve_lp_relaxation) const {

    int time_limit = cplex_time_limit > 0 ? cplex_time_limit : -1;
    if (factoring_timer.get_remaining_time() != numeric_limits<double>::infinity()){
        // TODO: this check seems to work; make sure that this is actually guaranteed by the standard
        int f_limit = factoring_timer.get_remaining_time();
        time_limit = time_limit > 0 ? min(f_limit, time_limit) : f_limit;
    }

    if (lp.get_constraints().empty()){
        cout << "WARNING: no constraints specified." << endl;
    }

    if (solve_lp_relaxation && time_limit == -1){
        cout << "WARNING: time limit not supported when solving LP relaxation, limit will be ignored." << endl;
        time_limit = -1;
    }

#ifndef NDEBUG
    dump_lp(lp);
#endif


    lp::LPSolver solver(lp_solver_type);
    solver.set_time_limit(time_limit);

    if (solve_lp_relaxation){
        for (auto &var : lp.get_variables()){
            var.is_integer = false;
        }
    }

    solver.load_problem(lp);
    solver.print_statistics();
    cout << "LP construction time: " << timer << endl;
    timer.reset();
    solver.solve();

    cout << "LP solving time: " << timer << endl;

    vector<double> solution;

    if (time_limit == -1) {
        if (!solver.has_optimal_solution()){
            solver.print_failure_analysis();
            cout << "no optimal solution to LP found, factoring failed" << endl;
            return solution;
        }
    } else {
        if (!solver.has_solution()){
            cout << "no solution to LP found, factoring failed" << endl;
            return solution;
        }
    }

    solution = solver.extract_solution();

    cout << "LP objective value: " << solver.get_objective_value() << endl;

    return solution;
}

void LPFactoring::dump_lp(const lp::LinearProgram &lp) const {
    if (lp.get_sense() == lp::LPObjectiveSense::MAXIMIZE){
        cout << "Linear Program: maximize objective value" << endl;
    } else {
        cout << "Linear Program: minimize objective value" << endl;
    }
    cout << lp.get_variables().size() << " variables" << endl;
    cout << lp.get_constraints().size() << " constraints" << endl;

    if (lp.get_variables().size() > 100 || lp.get_constraints().size() > 100){
        cout << "WARNING: not printing LP as it is too big" << endl;
        return;
    }

    cout << "variables: " << endl;
    int i = 0;
    for (auto &var : lp.get_variables()){
        cout << "x" << i++ << ": range " << var.lower_bound << " .. " << var.upper_bound << "; objective value " << var.objective_coefficient << (var.is_integer ? ", is integer variable" : "") << endl;
    }

    cout << endl;

    for (auto &constr : lp.get_constraints()){
        cout << constr.get_lower_bound() << " < ";
        for (size_t i = 0; i < constr.get_variables().size(); ++i){
             cout << constr.get_coefficients()[i] << " * x" << constr.get_variables()[i] << (i < constr.get_variables().size() - 1 ? " + " : "");
        }
        cout << " < " << constr.get_upper_bound() << endl;
    }
}

void LPFactoring::add_options_to_parser(options::OptionParser &parser) {
    parser.add_option<int>(
        "cplex_time_limit",
        "CPLEX time limit in seconds",
        "-1"
    );

    parser.add_option<int>(
        "memory_limit",
        "CPLEX memory limit in MB",
        "-1"
    );

}
}
