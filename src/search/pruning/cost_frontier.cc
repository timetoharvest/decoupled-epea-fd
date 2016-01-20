#include "cost_frontier.h"

#include "../operator.h"
#include "../plugin.h"
#include "../price_tag.h"
#include "../state_registry.h"
#include "../ext/boost/dynamic_bitset.hpp"


using namespace std;

CostFrontierPruning::CostFrontierPruning(const Options& opts) : Pruning(opts) {
    PriceTag::set_store_frontier();
    cout << "using cost frontier pruning" << endl;
}

DOMINATION CostFrontierPruning::check_dominance(const State &, PriceTag &check, 
                                                PriceTag &reference, DOMINATION needed) {
    
    assert(g_factoring->get_profile() == FORK);

    const vector<boost::dynamic_bitset<> > &current_frontier = check.get_frontier();
    const vector<boost::dynamic_bitset<> > &reference_frontier = reference.get_frontier();
        
    bool dominated = needed != DOMINATES;
    bool dominates = needed != DOMINATED;
    
    // compare prices
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (check.get_goal_cost(factor) != PriceTag::INF || reference.get_goal_cost(factor) != PriceTag::INF){
            if (check.get_goal_cost(factor) == PriceTag::INF || 
                (reference.get_goal_cost(factor) != PriceTag::INF &&
                    check.get_goal_cost(factor) > reference.get_goal_cost (factor))) {
                dominates = false;
            } else if (reference.get_goal_cost(factor) == PriceTag::INF || 
                        (check.get_goal_cost(factor) != PriceTag::INF &&
                        reference.get_goal_cost(factor) > check.get_goal_cost (factor))) {
                dominated = false;
            } 
#ifndef NDEBUG
            else {
                assert(check.get_goal_cost(factor) == reference.get_goal_cost(factor));	 
            }
#endif
	}

        for (size_t id = 0; id < g_state_registry->size(factor); ++id){
            if (!g_is_leaf_goal_state[factor][id]){
                if (dominates && reference_frontier[factor][id]){
                    assert(reference.has_leaf_state(id, factor));
                    
                    if (check.has_propagated_leaf_state(id, factor)){
                        int old_cost = reference.get_cost_of_state(id, factor);
                        int new_cost = check.get_propagated_cost_of_state(id, factor);
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
                    
                    if (reference.has_propagated_leaf_state(id, factor)){
                        int old_cost = reference.get_propagated_cost_of_state(id, factor);
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

static Pruning *_parse_cost_frontier(OptionParser &parser) {
    Pruning::add_options_to_parser(parser);

    Options opts = parser.parse();

    CostFrontierPruning *pruning = 0;
    if (!parser.dry_run()) {
        pruning = new CostFrontierPruning(opts);
    }

    return pruning;
}

static Plugin<Pruning> _plugin("cost_frontier", _parse_cost_frontier);

