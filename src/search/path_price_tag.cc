#include "path_price_tag.h"

#include "factoring.h"
#include "operator.h"
#include "operator_cost.h"
#include "state_registry.h"

#include <limits>
#include <iostream>

using namespace std;

void PathPriceTagInfo::dump() const {
    cout << "PathPriceTagInfo" << endl;
    if (best_supporter != 0){   // 0 for initial state facts
        cout << "op: " << best_supporter->get_name() << endl;
    } else {
        cout << "initial state fact" << endl;
    }
    if (is_new){
        cout << "is new" << endl;
    }
}

PathPriceTag::PathPriceTag(const PathPriceTag *tag) {
    if (tag == 0){
        number_facts = vector<size_t>(g_factors.size(), 0);
        if (g_factoring->get_search_type() != SAT){
            costs = vector<vector<int> >(g_factors.size());
        } else {
            reachable = vector<vector<bool> >(g_factors.size());
        }
        goal_cost = vector<int>(g_factors.size(), INF);
        paths = vector<vector<PathPriceTagInfo> >(g_factors.size());
    } else {
        number_facts = tag->number_facts;
        goal_cost = tag->goal_cost;
        costs = tag->costs;
        reachable = tag->reachable;
        paths = tag->paths;
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            if (g_factoring->get_search_type() != SAT){
                for (size_t state = 0; state < costs[factor].size(); ++state){
                    if (costs[factor][state] != INF){
                        paths[factor][state].is_new = false;
                    }
                }
            } else {
                for (size_t state = 0; state < reachable[factor].size(); ++state){
                    if (reachable[factor][state]){
                        paths[factor][state].is_new = false;
                    }
                }
            }
        }
    }
}


bool PathPriceTag::add_state(const LeafStateID &state, int cost, const Operator* bs, 
                        const LeafStateID &predecessor) {
    bool added = PriceTag::add_state(state, cost);
    
    size_t factor = state.get_factor();
    size_t leaf_id = g_leaf_state_id_map[factor][state.hash()];
    
    if (leaf_id >= paths[factor].size()){
        paths[factor].resize(leaf_id + 1);
    }
    if (added){
        paths[factor][leaf_id] = PathPriceTagInfo(bs, predecessor, true);
    }
    return added;
}

const PathPriceTagInfo& PathPriceTag::get_path_info_for_fact(int id, int factor) const {
    assert((size_t) g_leaf_state_id_map[factor][id] < paths[factor].size());
    return paths[factor][g_leaf_state_id_map[factor][id]];
}

void PathPriceTag::update_price_tags(const State &base_state) {
    
#ifdef DEBUG_PATH_RECONSTRUCTION
    cout << "+++++++++++++++ STARTING RPG" << endl;
#endif

    for (size_t factor = 0; factor < g_factors.size(); factor++){
        
        // skip fork leaves that don't have a goal!
        if (!g_factoring->has_successors(factor) && g_goals_per_factor[factor].empty()){
            continue;
        }
        
        size_t number_leaf_states = g_state_registry->size(factor);

#ifdef DEBUG_PATH_RECONSTRUCTION
        cout << "       starting RPG for leaf factor " << factor << endl;
        const vector<PriceTagInfo*> &achieved_facts = new_tag.get_facts(factor);
        for (int i = 0; i < achieved_facts.size(); i++){
            if (achieved_facts[i] == 0){
                continue;
            }
            lookup_leaf_state(LeafStateID(i, factor)).dump_pddl();
        }
        cout << "       " << new_tag.get_number_facts(factor) << " fact(s) initially true in leaf factor " << factor << endl;
#endif   
        
        vector<int> best_prices(number_leaf_states, numeric_limits<int>::max());
        
        bool is_ifork_leaf = !g_factoring->has_predecessors(factor);
        
        bool change = true;

        while (change) {
            
            if (g_factoring->get_search_type() == SAT && !g_factoring->has_successors(factor) && get_goal_cost(factor) >= 0){
                // in satisficing search => stop once a goal is reachable in fork-leaves
                break;
            }
            
            change = false;
            
            size_t number_states = 0;
            size_t id = 0;
            while (number_states < number_facts[factor]) {
                if (id >= g_state_registry->size(factor)){
                    change = true;
                    break;
                }
                if (has_leaf_state(id, factor)){
                    ++number_states;
                
                    int cost = get_cost_of_state(id, factor);
                    if (best_prices[id] <= cost || (!g_leaf_state_successors[factor][id].empty() &&
                                                    g_leaf_state_successors[factor][id][0].first == 0)){
                                                    // leaf state is a "dead-end" or successors have not been computed
                        ++id;
                        continue;
                    }
                    
                    best_prices[id] = cost;

                    LeafStateID predecessor = LeafStateID(id, factor);
                    
                    assert(id < g_leaf_state_successors[factor].size());
                    
                    for (size_t o = 0; o < g_leaf_state_successors[factor][id].size(); ++o){
                        const Operator *op = g_leaf_state_successors[factor][id][o].first;
                        if (is_ifork_leaf || op->is_center_applicable(base_state)){
                            change |= add_state(LeafStateID(g_leaf_state_successors[factor][id][o].second, factor),
                                               cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type()), op, predecessor);
                        }

                    }
                }
                ++id;
            }
        }
        
#ifdef DEBUG_PATH_RECONSTRUCTION
    cout << "   RPG for leaf factor " << factor << " finished" << endl;
#endif

    
    }
        
#ifdef DEBUG_PATH_RECONSTRUCTION
    cout << "+++++++++++++++ RPG FINISHED" << endl;
#endif
}

void PathPriceTag::apply_center_op_to_leaves(const PriceTag &old_tag, const Operator &op) {
     
#ifdef DEBUG_PATH_RECONSTRUCTION
    cout << "start to apply center op " << op.get_name() << " to leaves" << endl;
#endif
    
    vector<SegmentedArrayVector<PackedStateBin>* > &leaf_factor_data_pool = g_state_registry->leaf_factor_data_pool;

    for (size_t factor = 0; factor < g_factors.size(); factor++){
          
        bool has_pre_on_factor = op.has_precondition_on(factor);
        const vector<const Effect*> &post_per_factor = op.get_effects(factor);
        
        if (has_pre_on_factor && post_per_factor.size() == g_factors[factor].size()){
            // center op completely overwrites leaf
            LeafState predecessor = g_state_registry->lookup_leaf_state(LeafStateID(0, factor));

            leaf_factor_data_pool[factor]->push_back(predecessor.get_packed_buffer());
            PackedStateBin *buffer = (*leaf_factor_data_pool[factor])[leaf_factor_data_pool[factor]->size() - 1];

            for (size_t post = 0; post < post_per_factor.size(); post++) {
                int var = g_new_index[post_per_factor[post]->var];
                int val = post_per_factor[post]->val;
                g_leaf_factor_packers[factor]->set(buffer, var, val);
            }
            LeafStateID succ_state = g_state_registry->insert_id_or_pop_leaf_state(factor);
            
            add_state(succ_state, 0, 0, LeafStateID::no_state);
            
            continue;
        }
        
        int min_price = has_pre_on_factor ? numeric_limits<int>::max() : 0;

        // check which leaf states satisfy center precondition and apply effects
        int number_states = old_tag.get_number_states(factor);
        size_t id = 0;
        while (number_states > 0) {
            if (old_tag.has_leaf_state(id, factor)){
                --number_states;
                
                LeafStateID lid(id, factor);
                
                if (op.is_applicable(lid)){
                    if (g_factoring->get_search_type() != SAT){
                        min_price = min(min_price, old_tag.get_cost_of_state(id, factor));
                        assert(min_price != -1);
                    }
                            
                    if (post_per_factor.empty()){
                        add_state(lid, old_tag.get_cost_of_state(id, factor), 0, lid);
                    } else {
                        const LeafState state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
                        leaf_factor_data_pool[factor]->push_back(state.get_packed_buffer());
                        PackedStateBin *buffer = (*leaf_factor_data_pool[factor])[leaf_factor_data_pool[factor]->size() - 1];

                        for (size_t post = 0; post < post_per_factor.size(); post++) {
                            int var = g_new_index[post_per_factor[post]->var];
                            int val = post_per_factor[post]->val;
                            g_leaf_factor_packers[factor]->set(buffer, var, val);
                        }

                        LeafStateID succ_id = g_state_registry->insert_id_or_pop_leaf_state(factor);
            
                        add_state(succ_id, old_tag.get_cost_of_state(id, factor), 0, lid);
                    }
                }
            }
            ++id;
        }

        if (g_factoring->get_search_type() != SAT && min_price > 0){
            reduce_cost_by(factor, min_price);
        }
        
        assert(g_factoring->get_search_type() == SAT || min_price != numeric_limits<int>::max());
#ifdef DEBUG_PATH_RECONSTRUCTION
        cout << "min_price of factor " << factor << " = " << min_price << endl;
#endif
    }
}
