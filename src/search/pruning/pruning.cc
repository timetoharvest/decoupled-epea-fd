#include "pruning.h"

#include "../plugin.h"
#include "../price_tag.h"
#include "../state_registry.h"


using namespace std;

BasicPruning::BasicPruning (const Options&) {
    cout << "using BasicPruning method" << endl;
}

DOMINATION BasicPruning::check_dominance(const PriceTag &check, const PriceTag &reference, DOMINATION needed) const {
    
    bool sizes_equal = true;
    bool new_is_larger = false;
    bool new_is_smaller = false;
    bool all_leaves_max_size = g_factoring->get_profile() != FORK &&
                               g_factoring->get_search_type() == SAT;
    bool all_goals_reached = g_factoring->get_profile() == FORK &&
                             g_factoring->get_search_type() == SAT;
    
    vector<bool> skip_leaf(g_factors.size(), false);
    
    // TODO fork-leaves that have a goal actually *can* be compared, i.e. if they are identical
    
    // check number of reachable leaf states
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (g_factoring->get_search_type() == SAT && !g_factoring->has_successors(factor)) {
            // in satisficing search, updating PriceTags stops if a goal 
            // is reached in fork leaves. thus cannot compare number of states 
            // in this case
            if (check.get_goal_cost(factor) >= 0){
                if (reference.get_goal_cost(factor) >= 0){
                    // don't care about size/reachability
                } else {
                    all_goals_reached = false;
                    new_is_larger = true;
                    new_is_smaller = true;
                    sizes_equal = false;
                }
                skip_leaf[factor] = true;
                continue;
            } else if (reference.get_goal_cost(factor) >= 0){
                all_goals_reached = false;
                new_is_smaller = true;
                new_is_larger = true;
                sizes_equal = false;
                skip_leaf[factor] = true;
                continue;
            }
            all_goals_reached = false;
        }
        if (reference.get_number_states(factor) > check.get_number_states(factor)){
            new_is_smaller = true;
            sizes_equal = false;
        } else if (reference.get_number_states(factor) < check.get_number_states(factor)){
            new_is_larger = true;
            sizes_equal = false;
        }
        if (check.get_number_states(factor) < g_state_registry->size(factor) || !g_factoring->precompute_state_space(factor)){
            all_leaves_max_size = false;
        }
    }
    
    if (new_is_smaller && new_is_larger){
        return NONE;
    } else if (new_is_smaller && needed == DOMINATES){
        return NONE;
    } else if (new_is_larger && needed == DOMINATED){
        return NONE;
    }
    
    if (all_goals_reached){
        // this is only relevant in satisficing fork-decoupled search when the center
        // has not reached its goal yet, but all leaves do.
        return EQUAL;
    }
    
    if (sizes_equal && all_leaves_max_size){
        return EQUAL;
    }
    
    bool dominated = (new_is_smaller && !new_is_larger) || sizes_equal;
    bool dominates = (new_is_larger && !new_is_smaller) || sizes_equal;
    
    // compare prices / reachability
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (skip_leaf[factor]) { // is fork-leaf that reached its goal
            continue;
        }
        size_t number_new_states = check.get_number_states(factor);
        size_t number_old_states = reference.get_number_states(factor);
        size_t id = 0;
        while (number_new_states + number_old_states > 0){
            assert(id < g_state_registry->size(factor));
            // TODO probably introduce some kind of hash to compare leaves
            // against each other in satisficing search
            if (check.has_leaf_state(id, factor)){
                --number_new_states;
                if (reference.has_leaf_state(id, factor)){
                    --number_old_states;
                    if (g_factoring->get_search_type() != SAT){
                        int old_cost = reference.get_cost_of_state(id, factor);
                        int new_cost = check.get_cost_of_state(id, factor);
                        assert(old_cost != -1);
                        assert(new_cost != -1);
                        
                        if (old_cost > new_cost){
                            dominated = false;
                        } else if (old_cost < new_cost){
                            dominates = false;
                        }
                    }
                } else {
                    dominated = false;
                }                
            } else if (reference.has_leaf_state(id, factor)){
                --number_old_states;
                dominates = false;
            }
            if (!dominated && !dominates){
                return NONE;
            } else if (!dominated && needed == DOMINATED){
                return NONE;
            } else if (!dominates && needed == DOMINATES){
                return NONE;
            } else if (number_new_states == 0 && number_old_states > 0){
                dominates = false;
                break;
            } else if (number_old_states == 0 && number_new_states > 0){
                dominated = false;
                break;
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


static Pruning *_parse_basic(OptionParser &parser) {
    Options opts = parser.parse();

    BasicPruning *pruning = 0;
    if (!parser.dry_run()) {
        pruning = new BasicPruning(opts);
    }

    return pruning;
}

static Plugin<Pruning> _plugin("basic", _parse_basic);

