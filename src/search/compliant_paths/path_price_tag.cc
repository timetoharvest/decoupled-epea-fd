#include "path_price_tag.h"

#include "../factoring.h"
#include "../operator.h"
#include "../operator_cost.h"
#include "../state_registry.h"
#include "../successor_generator.h"

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


PathPriceTag::PathPriceTag(const PathPriceTag *cpg) {
    if (cpg == 0){
        number_states = vector<size_t>(g_factors.size(), 0);
        prices = vector<vector<int> >(g_factors.size());
        goal_cost = vector<int>(g_factors.size(), INF);
        paths = vector<vector<PathPriceTagInfo> >(g_factors.size());
    } else {
        number_states = cpg->number_states;
        goal_cost = cpg->goal_cost;
        prices = cpg->prices;
        paths = cpg->paths;
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            for (size_t state = 0; state < prices[factor].size(); ++state){
                paths[factor][state].reset_supporter();
            }
        }
    }
}

bool PathPriceTag::add_state(const LeafStateID &state, int cost, const Operator* bs, 
                        const LeafStateID &predecessor) {
    bool added = Prices::add_state(state, cost);
    
    size_t factor = state.get_factor();
    size_t leaf_id = leaf_state_id_map[factor][state.hash()];
    
    if (leaf_id >= paths[factor].size()){
        paths[factor].resize(leaf_id + 1);
    }
    if (added){
        paths[factor][leaf_id] = PathPriceTagInfo(bs, predecessor, true);
    }
    return added;
}

const PathPriceTagInfo& PathPriceTag::get_path_info_for_fact(const LeafStateID &id) const {
    size_t factor = id.get_factor();
    assert((size_t) leaf_state_id_map[factor][id.hash()] < paths[factor].size());
    return paths[factor][leaf_state_id_map[factor][id.hash()]];
}

void PathPriceTag::update_price_tags(const State &base_state) {
    
#ifdef DEBUG_PATH_RECONSTRUCTION
    cout << "+++++++++++++++ STARTING RPG" << endl;
#endif

    for (size_t factor = 0; factor < g_factors.size(); factor++){
        
        if (!g_factoring->has_successors(factor)){
            // skip fork leaves that don't have a goal or whose goal is already achieved (only satisficing search)
            if (g_goals_per_factor[factor].empty() || (g_factoring->get_search_type() == SAT && goal_cost[factor] != INF)){
                continue;
            }
        }

#ifdef DEBUG_PATH_RECONSTRUCTION
        cout << "       starting RPG for leaf factor " << factor << endl;
        const vector<PriceTagInfo*> &achieved_facts = new_tag.get_facts(factor);
        for (int i = 0; i < achieved_facts.size(); i++){
            if (achieved_facts[i] == 0){
                continue;
            }
            lookup_leaf_state(LeafStateID(i, factor)).dump_pddl();
        }
        cout << "       " << new_tag.get_number_states(factor) << " fact(s) initially true in leaf factor " << factor << endl;
#endif   
        
        vector<int> best_prices(g_state_registry->size(factor), numeric_limits<int>::max());
        
        bool is_ifork_leaf = !g_factoring->has_predecessors(factor);
        
        bool change = true;

        while (change) {
            
            change = false;
            
            size_t num_states = 0;
            size_t id = 0;
            while (num_states < number_states[factor]) {
                
                if (id >= g_state_registry->size(factor)){
                    change = true;
                    break;
                }
                if (has_leaf_state(LeafStateID(id, factor))){
                    ++num_states;
                
                    int cost = get_cost_of_state(LeafStateID(id, factor));
                    if (best_prices[id] <= cost){
                        ++id;
                        continue;
                    }
                    
                    best_prices[id] = cost;

                    LeafStateID predecessor_id(id, factor);
                    
                    assert(!precompute_leaf_state_spaces[factor] || id < leaf_state_successors[factor].size() || id >= MAX_LEAF_SPACE_STORE);
                    
                    if (precompute_leaf_state_spaces[factor] || 
                        (id < leaf_state_successors[factor].size() && !leaf_state_successors[factor][id].empty())){
                        if (!precompute_leaf_state_spaces[factor] && leaf_state_successors[factor][id][0].first == 0){
                            // is "dead-end" leaf state
                            ++id;
                            continue;
                        }
                        for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
                            const Operator *op = leaf_state_successors[factor][id][o].first;
                            if (is_ifork_leaf || op->is_center_applicable(base_state)){ // TODO what about caching applicability per update?
                                change |= add_state(LeafStateID(leaf_state_successors[factor][id][o].second, factor),
                                                cost + get_adjusted_action_cost(*op, cost_type), op, predecessor_id);
                            }
                        }
                    } else {                      
                        const LeafState predecessor = g_state_registry->lookup_leaf_state(predecessor_id);
                        
                        vector<const Operator*> applicable_ops;
                        g_leaf_successor_generators[factor]->generate_applicable_ops_ignore_others(predecessor, applicable_ops);

                        // apply all applicable_ops to predecessor and store the outcome          
                        for (size_t j = 0; j < applicable_ops.size(); ++j){
                            const Operator *op = applicable_ops[j];
                            const vector<const Effect*> &effects = op->get_effects(factor);
                            assert(op->get_affected_factor() != -1);
                            
                            if (!effects.empty() && op->is_center_applicable(base_state)){
                                LeafStateID lid = g_state_registry->get_successor_leaf_state_id(predecessor, *op);
                                
                                if (lid.hash() >= leaf_state_id_map[factor].size()){
                                    // this can happen when using reachability functions in search, since here we might
                                    // find new leaf states
                                    leaf_state_id_map[factor].resize(lid.hash() + 1, -1);
                                    best_prices.resize(lid.hash() + 1, numeric_limits<int>::max());
                                }
                                
                                change |= add_state(lid, cost + get_adjusted_action_cost(*op, cost_type), op, predecessor_id);
                            }
                        }
                    }
                    
                    
                    if (g_factoring->get_search_type() == SAT && !g_factoring->has_successors(factor) && get_goal_cost(factor) != INF){
                        // in satisficing search => stop once a goal is reachable in fork-leaves
                        change = false;
                        break;
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

void PathPriceTag::apply_center_op_to_leaves(const PathPriceTag &old_tag, const Operator &op) {
     
#ifdef DEBUG_PATH_RECONSTRUCTION
    cout << "start to apply center op " << op.get_name() << " to leaves" << endl;
#endif

    for (size_t factor = 0; factor < g_factors.size(); ++factor){
          
        bool has_pre_on_factor = op.has_precondition_on(factor);
        size_t num_effects = op.get_effects(factor).size();
        
        if (!has_pre_on_factor && num_effects == 0){
            // op does not affect leaf at all => copy everything
            number_states[factor] = old_tag.number_states[factor];
            prices[factor] = old_tag.prices[factor];
            goal_cost[factor] = old_tag.goal_cost[factor];
            paths[factor] = old_tag.paths[factor];
            for (size_t id = 0; id < paths[factor].size(); ++id){
                paths[factor][id].reset_supporter();
            }
            continue;
        }
        
        if (num_effects == g_factors[factor].size()){
            // center op completely overwrites leaf
            
            const LeafState predecessor = g_state_registry->lookup_leaf_state(LeafStateID(0, factor));
            const LeafStateID succ_state = g_state_registry->get_successor_leaf_state_id(predecessor, op);
            
            add_state(succ_state, 0, 0, LeafStateID::no_state);
            paths[factor][leaf_state_id_map[factor][succ_state.hash()]].reset_supporter();            
            continue;
        }

        // check which leaf states satisfy center precondition and apply effects
        int number_states = old_tag.get_number_states(factor);
        size_t id = 0;
        while (number_states > 0) {
            if (old_tag.has_leaf_state(LeafStateID(id, factor))){
                --number_states;
                
                LeafStateID lid(id, factor);
                
                if (!has_pre_on_factor || op.is_applicable(lid)){
                    if (num_effects == 0){
                        add_state(lid, old_tag.get_cost_of_state(LeafStateID(id, factor)), 0, lid);
                        paths[factor][leaf_state_id_map[factor][lid.hash()]].reset_supporter();
                    } else {
                        const LeafState state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
#ifndef NDEBUG
                        size_t size_before = g_state_registry->size(factor);
#endif
                        LeafStateID succ_id = g_state_registry->get_successor_leaf_state_id(state, op);
                        
                        assert(size_before == g_state_registry->size(factor));
            
                        add_state(succ_id, old_tag.get_cost_of_state(LeafStateID(id, factor)), 0, lid);
                        paths[factor][leaf_state_id_map[factor][succ_id.hash()]].reset_supporter();
                    }
                }
            }
            ++id;
        }
    }
}

unique_ptr<CompliantPathGraph> PathPriceTag::clone() const {
    return unique_ptr<CompliantPathGraph> (new PathPriceTag(this));
}
