#include "path_price_tag.h"

#include "../factoring.h"
#include "../operator.h"
#include "../operator_cost.h"
#include "../state_registry.h"
#include "../task_utils/successor_generator.h"
#include "../symmetries/decoupled_permutation.h"
#include "../utils/system.h"

#include <limits>
#include <iostream>
#include <numeric>


using namespace std;


void PathPriceInfo::dump() const {
    cout << "PathPriceTagInfo" << endl;
    if (generating_op != 0){   // 0 for initial state facts
        cout << "op: " << g_operators[generating_op].get_name() << endl;
    } else {
        cout << "initial state fact" << endl;
    }
    if (is_new){
        cout << "is new" << endl;
    }
}


PathPriceTag::PathPriceTag() : Prices() {
    Prices::resize();
    paths = vector<vector<PathPriceInfo> >(g_leaves.size());
}

PathPriceTag::PathPriceTag(const PathPriceTag &other) : Prices(other) {
    paths = other.paths;
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        for (LeafStateHash state(0); state < paths[factor].size(); ++state){
            paths[factor][state].reset_generating_op();
        }
    }
}

PathPriceTag::PathPriceTag(const CompliantPathGraph &cpg) {
    number_states = vector<size_t>(g_leaves.size(), 0);
    prices = vector<vector<int> >(g_leaves.size());
    goal_cost = vector<int>(g_leaves.size(), INF);
    paths = vector<vector<PathPriceInfo> >(g_leaves.size());
    
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        for (LeafStateHash id(0); id < g_state_registry->size(factor); ++id){
            if (cpg.has_leaf_state(id, factor)){
                add_state(id, factor, cpg.get_cost_of_state(id, factor), OperatorID::no_operator, LeafStateHash::MAX);
            }
        }
    }
}

bool PathPriceTag::add_state(LeafStateHash id, LeafFactorID factor, int cost, 
                             OperatorID generating_op, LeafStateHash predecessor) {
    bool added = Prices::add_state(id, factor, cost);
    
    size_t leaf_id = leaf_state_id_map[factor][id];
    
    if (leaf_id >= paths[factor].size()){
        paths[factor].resize(leaf_id + 1);
    }
    if (added){
        paths[factor][leaf_id] = PathPriceInfo(generating_op, predecessor, true);
    }
    return added;
}

const PathPriceInfo& PathPriceTag::get_path_info(LeafStateHash id, LeafFactorID factor) const {
    assert((size_t) leaf_state_id_map[factor][id] < paths[factor].size());
    return paths[factor][leaf_state_id_map[factor][id]];
}

void PathPriceTag::update(const State &base_state) {
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        
        if (g_factoring->is_fork_leaf(factor)){
            // TODO do we need this optimization here? try to keep the code simple
            // skip fork leaves that don't have a goal or whose goal is already achieved (only satisficing search)
            if (g_goals_per_factor[factor].empty() || (g_factoring->get_search_type() == SAT && goal_cost[factor] != INF)){
                continue;
            }
        }

        vector<int> best_prices(g_state_registry->size(factor), numeric_limits<int>::max());
        
        bool is_ifork_leaf = g_factoring->is_ifork_leaf(factor);
        
        bool change = true;

        while (change) {
            
            change = false;
            
            size_t num_states = 0;
            LeafStateHash id(0);
            while (num_states < number_states[factor]) {
                
                if (id >= g_state_registry->size(factor)){
                    change = true;
                    break;
                }
                if (has_leaf_state(id, factor)){
                    ++num_states;
                
                    int cost = get_cost_of_state(id, factor);
                    if (best_prices[id] <= cost){
                        ++id;
                        continue;
                    }
                    
                    best_prices[id] = cost;
                    
                    assert(!precompute_leaf_state_spaces[factor] || id < leaf_state_successors[factor].size() || id >= MAX_LEAF_SPACE_STORE);
                    
                    if (precompute_leaf_state_spaces[factor] || 
                        (id < leaf_state_successors[factor].size() && !leaf_state_successors[factor][id].empty())){
                        if (!precompute_leaf_state_spaces[factor] && leaf_state_successors[factor][id][0].first == OperatorID::no_operator){
                            // is "dead-end" leaf state
                            ++id;
                            continue;
                        }
                        for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
                            const Operator &op = g_operators[leaf_state_successors[factor][id][o].first];
                            if (is_ifork_leaf || op.is_center_applicable(base_state)){
                                // TODO what about caching applicability per update?
                                change |= add_state(leaf_state_successors[factor][id][o].second, factor,
                                                    cost + get_adjusted_action_cost(op, cost_type), op.get_id(), id);
                            }
                        }
                    } else {                      
                        const LeafState predecessor = g_state_registry->lookup_leaf_state(id, factor);
                        
                        vector<OperatorID> applicable_ops;
                        leaf_successor_generators[factor]->generate_applicable_ops(predecessor, applicable_ops);

                        // apply all applicable_ops to predecessor and store the outcome          
                        for (OperatorID op_id : applicable_ops){
                            const Operator &op = g_operators[op_id];
                            assert(op.get_affected_factor() != LeafFactorID::CENTER);
                            
                            if (op.has_effect_on(factor) && op.is_center_applicable(base_state)){
                                LeafStateHash lid = g_state_registry->get_successor_leaf_state_hash(predecessor, op);
                                
                                if (lid >= leaf_state_id_map[factor].size()){
                                    // this can happen when using reachability functions in search, since here we might
                                    // find new leaf states
                                    leaf_state_id_map[factor].resize(lid + 1, -1);
                                    best_prices.resize(lid + 1, numeric_limits<int>::max());
                                }
                                
                                change |= add_state(lid, factor, cost + get_adjusted_action_cost(op, cost_type), op_id, id);
                            }
                        }
                    }
                    
                    
                    if (g_factoring->get_search_type() == SAT && g_factoring->is_fork_leaf(factor) && get_goal_cost(factor) != INF){
                        // in satisficing search => stop once a goal is reachable in fork-leaves
                        change = false;
                        break;
                    }
                }
                ++id;
            }
        }
    }
}

void PathPriceTag::apply_center_op_to_leaves(const PathPriceTag &old_tag, const Operator &op) {
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
          
        bool has_pre_on_factor = op.has_precondition_on(factor);
        size_t num_effects = op.get_effects(factor).size();
        
        if (!has_pre_on_factor && num_effects == 0){
            // op does not affect leaf at all => copy everything
            number_states[factor] = old_tag.number_states[factor];
            prices[factor] = old_tag.prices[factor];
            goal_cost[factor] = old_tag.goal_cost[factor];
            paths[factor] = old_tag.paths[factor];
            for (size_t id = 0; id < paths[factor].size(); ++id){
                paths[factor][id].reset_generating_op();
            }
            continue;
        }
        
        if (num_effects == g_leaves[factor].size()){
            // center op completely overwrites leaf
            LeafState predecessor = g_state_registry->lookup_leaf_state(LeafStateHash(0), factor);
#ifndef NDEBUG
            size_t size_before = g_state_registry->size(factor);
#endif
            LeafStateHash succ_state = g_state_registry->get_successor_leaf_state_hash(predecessor, op);
            assert(size_before == g_state_registry->size(factor));
            
            add_state(succ_state, factor, 0, OperatorID::no_operator, LeafStateHash::MAX);
            paths[factor][leaf_state_id_map[factor][succ_state]].reset_generating_op();
            continue;
        }

        // check which leaf states satisfy center precondition and apply effects
        size_t number_states = old_tag.get_number_states(factor);
        LeafStateHash id(0);
        while (number_states > 0) {
            if (old_tag.has_leaf_state(id, factor)){
                --number_states;
                
                if (!has_pre_on_factor || op.is_applicable(id, factor)){
                    if (num_effects == 0){
                        add_state(id, factor, old_tag.get_cost_of_state(id, factor), OperatorID::no_operator, id);
                        paths[factor][leaf_state_id_map[factor][id]].reset_generating_op();
                    } else {
                        LeafState state = g_state_registry->lookup_leaf_state(id, factor);
#ifndef NDEBUG
                        size_t size_before = g_state_registry->size(factor);
#endif
                        LeafStateHash succ_id = g_state_registry->get_successor_leaf_state_hash(state, op);
                        
                        assert(size_before == g_state_registry->size(factor));
            
                        add_state(succ_id, factor, old_tag.get_cost_of_state(id, factor), OperatorID::no_operator, id);
                        paths[factor][leaf_state_id_map[factor][succ_id]].reset_generating_op();
                    }
                }
            }
            ++id;
        }
    }
}

bool PathPriceTag::apply_symmetry_permutation(const LeavesPermutation &per) {
    // Copy prices and reinitialize affected factors
    vector<vector<int> > old_prices(prices);
    vector<size_t> old_number_states(number_states);
    vector<int> old_goal_cost(goal_cost); // only for debugging
    vector<vector<PathPriceInfo> > old_paths(paths);

    for (LeafFactorID factor : per.get_factors_affected()) {
        number_states[factor] = 0;
        prices[factor] = vector<int>();
        goal_cost[factor] = INF;
        paths[factor] = vector<PathPriceInfo>();
    }
    
    for(LeafFactorID to_factor : per.get_factors_affected()) {
        LeafFactorID from_factor = per.get_from_factor(to_factor);

        assert(find(per.get_factors_affected().begin(), 
                         per.get_factors_affected().end(), to_factor)
               != per.get_factors_affected().end());

        assert(find(per.get_factors_affected().begin(), 
                         per.get_factors_affected().end(), from_factor)
               != per.get_factors_affected().end());

        size_t num_states = old_number_states[from_factor]; 
        for  (LeafStateHash id(0); num_states > 0; ++id){
            assert(id < leaf_state_id_map[from_factor].size());
            if (leaf_state_id_map[from_factor][id] == -1){
                continue;
            }

            size_t from_state_index = leaf_state_id_map[from_factor][id];
            if (from_state_index >= old_prices[from_factor].size() || old_prices[from_factor][from_state_index] == INF){
                continue;
            }

            num_states--;
            pair<LeafStateHash, LeafFactorID> to_pair = per.get_new_factor_state_by_old_factor_state(id, from_factor);
            assert(to_pair.second == to_factor);
            LeafStateHash to_state = to_pair.first;

            // best_supporter and predecessor need to be permuted too. this is not required for plan reconstruction, though.
            add_state(to_state, to_factor, old_prices[from_factor][from_state_index], OperatorID::no_operator, LeafStateHash::MAX);
        }
    }

    assert(accumulate(number_states.begin(), number_states.end(), 0) == accumulate(old_number_states.begin(), old_number_states.end(), 0));
    assert(accumulate(goal_cost.begin(), goal_cost.end(), 0) == accumulate(old_goal_cost.begin(), old_goal_cost.end(), 0));

    // We do not need to check whether the prices changed
    // because the return value is only relevant if
    // if_less is true and in that case we checked this in
    // per.produce_less(prices)
    return true;
}

unique_ptr<CompliantPathGraph> PathPriceTag::clone() const {
    // TODO fix this
    cerr << "this code must not be reached; path_price_tag.cc" << endl;
    utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
}
