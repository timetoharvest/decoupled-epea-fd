#include <cassert>
#include <iostream>
#include <limits>
using namespace std;

#include "globals.h"
#include "operator_cost.h"
#include "option_parser.h"
#include "search_engine.h"
#include "timer.h"

SearchEngine::SearchEngine(const Options &opts)
    : search_space(OperatorCost(opts.get_enum("cost_type"))),
      cost_type(OperatorCost(opts.get_enum("cost_type"))) {
    solved = false;
    if (opts.get<int>("bound") < 0) {
        cerr << "error: negative cost bound " << opts.get<int>("bound") << endl;
        exit_with(EXIT_INPUT_ERROR);
    }
    bound = opts.get<int>("bound");
    if (g_use_decoupled_search){
	g_factoring->set_cost_type(cost_type);
    }
}

SearchEngine::~SearchEngine() {
}

void SearchEngine::statistics() const {
}

bool SearchEngine::found_solution() const {
    return solved;
}

const SearchEngine::Plan &SearchEngine::get_plan() const {
    assert(solved);
    return plan;
}

void SearchEngine::set_plan(const Plan &p) {
    solved = true;
    plan = p;
}

void SearchEngine::search() {
    initialize();
    Timer timer;
    while (step() == IN_PROGRESS)
        ;
    
    if (g_use_decoupled_search){
	cout << "maximum duplicate counter " << g_state_registry->get_max_number_duplicates() << endl;
    }
    
    cout << "Actual search time: " << timer
         << " [t=" << g_timer << "]" << endl;
}

bool SearchEngine::check_goal_and_set_plan(const State &state) {
    if (test_goal(state)) {
        Plan plan;
        search_space.trace_path(state, plan);
	if (g_use_decoupled_search && g_factoring->get_search_type() != SAT){
	    if (g_factoring->get_search_type() == FRA){
		if (!is_optimal_leaf_plan(plan)){
		    cout << "found plan is not optimal, cannot guarantee optimality => exit [t=" << g_timer << "]" << endl;
		    exit(EXIT_UNSOLVED_INCOMPLETE);
		}
	    } else {
                int cost = 0;
                for (int i = 0; i < plan.size(); i++){
                    cost += get_adjusted_cost(*plan[i]);
                }
                if (g_factoring->get_search_type() == AFRA){
                    if (!is_optimal_leaf_plan(plan) || (!this->plan.empty() && bound < cost)){
                        if (this->plan.empty() || bound > cost){
                            bound = cost;
                            set_plan(plan);
                        }
                        cout << "found a plan of cost " << cost 
                             << " that is not leaf-optimal => continue search [t=" << g_timer << "]" << endl;
                        
                        return false;
                    }
                } else if (g_factoring->get_search_type() == FDA){
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
    for (int i = 0; i < plan.size(); i++){
	const Operator *op = plan[i];
	int affected_factor = op->get_affected_factor();
	if (affected_factor != -1){
	    goal_cost[affected_factor] += get_adjusted_cost(*op);
	}
    }
    for (int i = 0; i < g_min_goal_cost.size(); i++){
        if (g_goals_per_factor[i].empty()){
            continue;
        }
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
}
