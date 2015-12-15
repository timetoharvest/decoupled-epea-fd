#include "cost_frontier.h"

#include "../operator.h"
#include "../plugin.h"
#include "../price_tag.h"
#include "../state_registry.h"


using namespace std;

CostFrontierPruning::CostFrontierPruning(const Options&) : registered(false),
    get_new_frontier(true)
#ifndef NDEBUG
    , last_checked_state(StateID::no_state)
#endif
{
    PriceTag::set_store_frontier();
    cout << "using cost frontier pruning" << endl;
}

DOMINATION CostFrontierPruning::check_dominance(const State &center, const PriceTag &check, 
                                                const PriceTag &reference, DOMINATION needed) {
    
    assert(g_factoring->get_profile() == FORK);
    assert(g_factoring->get_search_type() != SAT);
    
    if (!registered){
        registered = true;
        g_state_registry->register_data_store(this);
    }
    
    if (get_new_frontier){
        check.get_frontier().swap(current_frontier);
        get_new_frontier = false;
    }
    
    assert(!current_frontier.empty());
#ifndef NDEBUG
    last_checked_state = center.get_id();
#endif
    
    vector<boost::dynamic_bitset<> > reference_frontier = is_frontier_state[center];
    
    // frontier is only stored for states for which the dominance was already checked at some point
    assert(!reference_frontier.empty() || g_state_registry->state_duplicate_counter[center.get_id().hash()] == 0);
    
    if (reference_frontier.empty()){
        reference_frontier.resize(g_factors.size());
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            boost::dynamic_bitset<>(g_state_registry->size(factor), false).swap(reference_frontier[factor]);
            for (size_t id = 0; id < g_leaf_state_successors[factor].size(); ++id){
                if (!reference.has_leaf_state(id, factor)){
                    continue;
                }
                for (size_t o = 0; o < g_leaf_state_successors[factor][id].size(); ++o){
                    const Operator *op = g_leaf_state_successors[factor][id][o].first;
                    if (op->is_center_applicable(center)){
                        continue;
                    }
                    int successor = g_leaf_state_successors[factor][id][o].second;
                    if (!reference.has_leaf_state(successor, factor)){
                        reference_frontier[factor][id] = true;
                        break;
                    }
                    int new_cost = reference.get_cost_of_state(id, factor) + get_adjusted_action_cost(*op, g_factoring->get_cost_type());
                    if (reference.get_cost_of_state(successor, factor) > new_cost){
                        reference_frontier[factor][id] = true;
                        break;
                    }
                }
            }
        }
        is_frontier_state[center] = reference_frontier;
#ifdef DEBUG_PRUNING
        cout << "not precomputed" << endl;
    } else {
        cout << "precomputed" << endl;
#endif
    }
    
    bool dominated = needed != DOMINATES;
    bool dominates = needed != DOMINATED;
    
    // compare prices
    for (size_t factor = 0; factor < g_factors.size(); ++factor){        
        for (size_t id = 0; id < g_state_registry->size(factor); ++id){
            if (g_is_leaf_goal_state[factor][id]){
                if (reference.has_leaf_state(id, factor)){
                    if (check.has_leaf_state(id, factor)){
                        int old_cost = reference.get_cost_of_state(id, factor);
                        int new_cost = check.get_cost_of_state(id, factor);
                        if (old_cost < new_cost){
                            dominates = false;
                        } else if (new_cost < old_cost){
                            dominated = false;
                        }
                    } else {
                        dominates = false;
                    }
                } else if (check.has_leaf_state(id, factor)){
                    dominated = false;
                }
            } else {
                if (dominates && reference_frontier[factor][id]){
                    assert(reference.has_leaf_state(id, factor));
                    if (check.has_leaf_state(id, factor)){
                        int old_cost = reference.get_cost_of_state(id, factor);
                        int new_cost = check.get_cost_of_state(id, factor);
                        assert(old_cost != -1);
                        assert(new_cost != -1);
                        
                        if (old_cost < new_cost){
                            dominates = false;
                        }
                    } else {
                        dominates = false;
                    }
                }
                if (dominated && current_frontier[factor][id]){
                    assert(check.has_leaf_state(id, factor));
                    if (reference.has_leaf_state(id, factor)){
                        int old_cost = reference.get_cost_of_state(id, factor);
                        int new_cost = check.get_cost_of_state(id, factor);
                        assert(old_cost != -1);
                        assert(new_cost != -1);
                        
                        if (old_cost > new_cost){
                            dominated = false;
                        }
                    } else {
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

void CostFrontierPruning::reset() {
    get_new_frontier = true;
}

void CostFrontierPruning::store_cached_results(const State &state) {
    if (get_new_frontier){
        is_frontier_state[state] = g_price_tags[state].get_frontier();
    } else {
        is_frontier_state[state] = current_frontier;
    }
    get_new_frontier = true;
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

