#include "cost_frontier.h"

#include "../operator.h"
#include "../plugin.h"
#include "../price_tag.h"
#include "../state_registry.h"


using namespace std;

CostFrontierPruning::CostFrontierPruning(const Options&)
#ifndef NDEBUG
    : last_checked_state(StateID::no_state)
#endif
{
    cout << "using cost frontier pruning" << endl;
}

DOMINATION CostFrontierPruning::check_dominance(const State &center, const PriceTag &check, 
                                                const PriceTag &reference, DOMINATION needed) {
    
    assert(g_factoring->get_profile() == FORK);
    assert(g_factoring->get_search_type() != SAT);
    // TODO implement

    
    last_checked.resize(g_factors.size(), boost::dynamic_bitset<>());
    
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        boost::dynamic_bitset<>(g_state_registry->size(factor), false).swap(last_checked[factor]);
        for (size_t id = 0; id < g_leaf_state_successors[factor].size(); ++id){
            if (!check.has_leaf_state(id, factor)){
                continue;
            }
            for (size_t o = 0; o < g_leaf_state_successors[factor][id].size(); ++o){
                const Operator *op = g_leaf_state_successors[factor][id][o].first;
                if (op->is_center_applicable(center)){
                    continue;
                }
                int successor = g_leaf_state_successors[factor][id][o].second;
                if (!check.has_leaf_state(successor, factor)){
                    last_checked[factor][id] = true;
                    break;
                }
                int new_cost = check.get_cost_of_state(id, factor) + get_adjusted_action_cost(*op, g_factoring->get_cost_type());
                if (check.get_cost_of_state(successor, factor) > new_cost){
                    last_checked[factor][id] = true;
                    break;
                }
            }
        }
    }
    
    
    bool dominated = true;
    bool dominates = true;
    
    vector<boost::dynamic_bitset<> > reference_frontier = is_frontier_state[center];
    assert(!reference_frontier.empty());
    
    // compare prices
    for (size_t factor = 0; factor < g_factors.size(); ++factor){        
        for (size_t id = 0; id < g_state_registry->size(factor); ++id){
            assert(id < g_state_registry->size(factor));
            bool is_goal = g_is_leaf_goal_state[factor][id];
            if (needed != DOMINATED && (last_checked[factor][id] || is_goal)){
                // TODO need to handle goal states
                assert(check.has_leaf_state(id, factor));
                if (reference.has_leaf_state(id, factor)){
                    int old_cost = reference.get_cost_of_state(id, factor);
                    int new_cost = check.get_cost_of_state(id, factor);
                    assert(old_cost != -1);
                    assert(new_cost != -1);
                    
                    if (old_cost < new_cost){
                        dominates = false;
                    }
                }
            }
            if (needed != DOMINATES && (reference_frontier[factor][id] || is_goal)){
                // TODO need to handle goal states
                assert(reference.has_leaf_state(id, factor));
                if (check.has_leaf_state(id, factor)){
                    int old_cost = reference.get_cost_of_state(id, factor);
                    int new_cost = check.get_cost_of_state(id, factor);
                    assert(old_cost != -1);
                    assert(new_cost != -1);
                    
                    if (old_cost > new_cost){
                        dominated = false;
                    }
                }
            }
            

            if (!dominated && !dominates){
                return NONE;
            } else if (!dominated && needed == DOMINATED){
                return NONE;
            } else if (!dominates && needed == DOMINATES){
                return NONE;
            }
            ++id;
        }
    }
    if (dominated && dominates){
#ifdef DEBUG_PRUNING
        cout << "old tag is equal to new one" << endl;
#endif          
        return EQUAL;
    } else if (dominated){
#ifdef DEBUG_PRUNING
        cout << "new tag is dominated by old one" << endl;
#endif          
        return DOMINATED;
    } else if (dominates){
#ifdef DEBUG_PRUNING
        cout << "new tag dominates old one" << endl;
#endif          
        return DOMINATES;
    }
    return NONE;
}

void CostFrontierPruning::store_cached_results(const State& state) {
#ifndef NDEBUG
    for (size_t i = 0; i < g_center_vars.size(); ++i){
        int var = g_center_vars[i];
        assert(g_state_registry->lookup_state(last_checked_state)[var] == state[var]);
    }
#endif
    
    is_frontier_state[state] = last_checked;
}


static Pruning *_parse_cost_frontier(OptionParser &parser) {
    Options opts = parser.parse();

    CostFrontierPruning *pruning = 0;
    if (!parser.dry_run()) {
        pruning = new CostFrontierPruning(opts);
    }

    return pruning;
}

static Plugin<Pruning> _plugin("cost_frontier", _parse_cost_frontier);

