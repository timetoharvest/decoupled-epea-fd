#include "search_engine.h"

#include "countdown_timer.h"
#include "factoring.h"
#include "globals.h"
#include "option_parser.h"
#include "compliant_paths/compliant_path_graph.h"
#include "compliant_paths/cpg_storage.h"

#include <cassert>
#include <iostream>
#include <limits>


using namespace std;


SearchEngine::SearchEngine(const Options &opts)
    : status(IN_PROGRESS),
      solution_found(false),
      cheapest_goal_state(StateID::no_state),
      search_space(OperatorCost(opts.get_enum("cost_type"))),
      cost_type(OperatorCost(opts.get_enum("cost_type"))),
      max_time(opts.get<double>("max_time")) {
    if (opts.get<int>("bound") < 0) {
        cerr << "error: negative cost bound " << opts.get<int>("bound") << endl;
        exit_with(EXIT_INPUT_ERROR);
    }
    bound = opts.get<int>("bound");
    if (g_use_decoupled_search){
	CompliantPathGraph::set_cost_type(cost_type);
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
        if (g_use_decoupled_search){         
            const SearchNode &info = search_space.get_node(state);
            int solution_cost = info.get_g();
            const CompliantPathGraph *cpg = CPGStorage::storage->get_cpg(state);
            for (size_t factor = 0; factor < g_factors.size(); ++factor){
                if (!g_goals_per_factor[factor].empty()){
                    solution_cost += cpg->get_goal_cost(factor);
                }
            }
            cout << "found a plan of cost " << solution_cost;
            
            if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
                solution_found = true;
                cheapest_goal_state = state.get_id();
            } else if (g_factoring->get_search_type() == FRA){
                if (!is_optimal_leaf_plan(*cpg)){
                    cout << ", cannot guarantee optimality => exit [t=" << g_timer << "]" << endl;
                    exit_with(EXIT_UNSOLVED_INCOMPLETE);
                }
                solution_found = true;
                cheapest_goal_state = state.get_id();
            } else if (g_factoring->get_search_type() == ASDA){
                if (is_optimal_leaf_plan(*cpg)){
                    solution_found = true;
                    bound = solution_cost;
                    cheapest_goal_state = state.get_id();
                } else {
                    if (bound > solution_cost){
                        solution_found = true;
                        bound = solution_cost;
                        cheapest_goal_state = state.get_id();
                    }                    
                    cout << ", not leaf-optimal => continue search [t=" << g_timer << "]" << endl;                            
                    return false;
                }
            } else if (g_factoring->get_search_type() == SDA){
                if (!found_solution()){
                    solution_found = true;
                    bound = solution_cost;
                    cheapest_goal_state = state.get_id();
                } else if (solution_cost < bound){
                    bound = solution_cost;
                    cheapest_goal_state = state.get_id();
                }
                cout << " [t=" << g_timer << "]" << endl;
                return false;
            }
            cout << " [t=" << g_timer << "]" << endl;
            return true;
	}
	Plan plan;
        search_space.trace_path(state, plan);
	cout << "Solution found!" << endl;
        set_plan(plan);
        return true;
    }
    return false;
}

bool SearchEngine::is_optimal_leaf_plan(const CompliantPathGraph &cpg) {
    for (size_t factor = 0; factor < g_min_goal_cost.size(); ++factor){
        if (!g_goals_per_factor[factor].empty()){
            assert(cpg.get_goal_cost(factor) >= 0);
            if (cpg.get_goal_cost(factor) > g_min_goal_cost[factor]){
                return false;
            }
        }
    }
    return true;
}

void SearchEngine::save_plan_if_necessary() const {
    if (found_solution()){
        if (g_use_decoupled_search){
            search_space.trace_path(g_state_registry->lookup_state(cheapest_goal_state), plan);
        }
        save_plan(get_plan(), 0);
    }
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
}
