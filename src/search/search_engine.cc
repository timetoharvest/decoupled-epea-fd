#include "search_engine.h"

#include "countdown_timer.h"
#include "factoring.h"
#include "globals.h"
#include "option_parser.h"
#include "symmetries/graph_creator.h"

#include <cassert>
#include <iostream>
#include <limits>


using namespace std;


SearchEngine::SearchEngine(const Options &opts)
    : status(IN_PROGRESS),
      solution_found(false),
      search_space(OperatorCost(opts.get_enum("cost_type"))),
      cost_type(OperatorCost(opts.get_enum("cost_type"))),
      max_time(opts.get<double>("max_time")) {
    if (opts.get<int>("bound") < 0) {
        cerr << "error: negative cost bound " << opts.get<int>("bound") << endl;
        exit_with(EXIT_INPUT_ERROR);
    }
    bound = opts.get<int>("bound");
    if (g_use_decoupled_search){
	g_factoring->set_cost_type(cost_type);
    }

    if (opts.contains("symmetry")) {
        g_symmetry_graph = unique_ptr<GraphCreator>(opts.get<GraphCreator *>("symmetry"));
    }
}

SearchEngine::~SearchEngine() {
}

void SearchEngine::statistics() const {
}

bool SearchEngine::found_solution() const {
    return solution_found;
}

SearchStatus SearchEngine::get_status() const {
    return status;
}

const SearchEngine::Plan &SearchEngine::get_plan() const {
    assert(solution_found);
    return plan;
}

void SearchEngine::set_plan(const Plan &p) {
    solution_found = true;
    plan = p;
}

void SearchEngine::search() {
    initialize();

    if(g_symmetry_graph){
	g_symmetry_graph->initialize();
    }
    
    CountdownTimer timer(max_time);
    while (status == IN_PROGRESS) {
        status = step();
        if (timer.is_expired()) {
            cout << "Time limit reached. Abort search." << endl;
            status = TIMEOUT;
            break;
        }
    }

    cout << "Actual search time: " << timer
         << " [t=" << g_timer << "]" << endl;
}

bool SearchEngine::check_goal_and_set_plan(const State &state) {
    if (test_goal(state)) {
        Plan plan;
        search_space.trace_path(state, plan);
	if (g_use_decoupled_search && g_factoring->get_search_type() != SAT && g_factoring->get_search_type() != UNSAT){
	    if (g_factoring->get_search_type() == FRA){
		if (!is_optimal_leaf_plan(plan)){
		    cout << "found plan is not optimal, cannot guarantee optimality => exit [t=" << g_timer << "]" << endl;
		    exit_with(EXIT_UNSOLVED_INCOMPLETE);
		}
	    } else {
                int cost = 0;
                for (size_t i = 0; i < plan.size(); i++){
                    cost += get_adjusted_cost(*plan[i]);
                }
                if (g_factoring->get_search_type() == ASDA){
                    if (!is_optimal_leaf_plan(plan) || (!this->plan.empty() && bound < cost)){
                        if (this->plan.empty() || bound > cost){
                            bound = cost;
                            set_plan(plan);
                        }
                        cout << "found a plan of cost " << cost 
                             << " that is not leaf-optimal => continue search [t=" << g_timer << "]" << endl;
                        
                        return false;
                    }
                } else if (g_factoring->get_search_type() == SDA){
                    cout << "found a plan of cost " << cost;
                    
                    if (!found_solution()){
                        cout << " [t=" << g_timer << "]" << endl;
                        set_plan(plan);
                        bound = cost;
                    } else if (cost < bound){
                        cout << ", is cheaper than current best plan => replace [t=" << g_timer << "]" << endl;
                        set_plan(plan);
                        bound = cost;
                    } else {
                        cout << ", not cheaper than current best plan (cost=" << bound 
                             << ") => ignore [t=" << g_timer << "]" << endl;
                    }
                    return false;
                }
            }
	}
	cout << "Solution found!" << endl;
        set_plan(plan);
        return true;
    }
    return false;
}

bool SearchEngine::is_optimal_leaf_plan(const Plan &plan) {
    vector<int> goal_cost(g_factors.size(), 0);
    for (size_t i = 0; i < plan.size(); ++i){
        const Operator *op = plan[i];
        int affected_factor = op->get_affected_factor();
        if (affected_factor != -1){
            goal_cost[affected_factor] += get_adjusted_cost(*op);
        }
    }
    for (size_t i = 0; i < g_min_goal_cost.size(); ++i){
        assert(goal_cost[i] != 0 || g_min_goal_cost[i] == 0);
        if (goal_cost[i] > g_min_goal_cost[i]){
            return false;
        }
    }
    return true;
}


void SearchEngine::save_plan_if_necessary() const {
    if (found_solution())
        save_plan(get_plan(), 0);
}

int SearchEngine::get_adjusted_cost(const Operator &op) const {
    return get_adjusted_action_cost(op, cost_type);
}

void SearchEngine::add_options_to_parser(OptionParser &parser) {
    ::add_cost_type_option_to_parser(parser);
    parser.add_option<int>(
        "bound",
        "exclusive depth bound on g-values. Cutoffs are always performed according to "
        "the real cost, regardless of the cost_type parameter", "infinity");
    parser.add_option<double>(
        "max_time",
        "maximum time in seconds the search is allowed to run for. The "
        "timeout is only checked after each complete search step "
        "(usually a node expansion), so the actual runtime can be arbitrarily "
        "longer. Therefore, this parameter should not be used for time-limiting "
        "experiments. Timed-out searches are treated as failed searches, "
        "just like incomplete search algorithms that exhaust their search space.",
        "infinity");
    parser.add_option<GraphCreator *>("symmetry", "use symmetries", "", OptionFlags(false));
}
