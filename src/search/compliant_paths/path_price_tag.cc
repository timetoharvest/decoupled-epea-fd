#include "path_price_tag.h"

#include "../factoring.h"
#include "../operator.h"
#include "../operator_cost.h"
#include "../state_registry.h"
#include "../successor_generator.h"
#include "../symmetries/decoupled_permutation.h"

#include <limits>
#include <iostream>
#include <numeric>


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
        number_states = vector<size_t>(g_factors.size(), 0);
        prices = vector<vector<int> >(g_factors.size());
        goal_cost = vector<int>(g_factors.size(), INF);
        paths = vector<vector<PathPriceTagInfo> >(g_factors.size());
    } else {
        number_states = tag->number_states;
        goal_cost = tag->goal_cost;
        prices = tag->prices;
        paths = tag->paths;
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            for (size_t state = 0; state < prices[factor].size(); ++state){
                if (prices[factor][state] != INF){
                    paths[factor][state].is_new = false;
                }
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

const PathPriceTagInfo& PathPriceTag::get_path_info_for_fact(int id, int factor) const {
    assert((size_t) leaf_state_id_map[factor][id] < paths[factor].size());
    return paths[factor][leaf_state_id_map[factor][id]];
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
        cout << "       " << new_tag.get_number_states(factor) << " fact(s) initially true in leaf factor " << factor << endl;
#endif   
        
        vector<int> best_prices(number_leaf_states, numeric_limits<int>::max());
        
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
                if (has_leaf_state(id, factor)){
                    ++num_states;
                
                    int cost = get_cost_of_state(id, factor);
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
                            if (is_ifork_leaf || op->is_center_applicable(base_state)){
                                change |= add_state(LeafStateID(leaf_state_successors[factor][id][o].second, factor),
                                                cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type()), op, predecessor_id);
                            }

                        }
                    } else {                      
                        const LeafState predecessor = g_state_registry->lookup_leaf_state(predecessor_id);
                        
                        vector<const Operator*> applicable_ops;
                        
                        if (g_factoring->has_successors(factor) && g_factoring->has_predecessors(factor)){
                            // there exist center ops that change this leaf factor
                            g_successor_generator->generate_applicable_ops_ignore_others(predecessor, applicable_ops, true);
                        }

                        g_leaf_successor_generators[factor]->generate_applicable_ops_ignore_others(predecessor, applicable_ops);

                        // apply all applicable_ops to predecessor and store the outcome          
                        for (size_t j = 0; j < applicable_ops.size(); ++j){
                            const Operator *op = applicable_ops[j];
                            const vector<const Effect*> &effects = op->get_effects(factor);
                            
                            if (!effects.empty() && op->is_center_applicable(base_state)){
                                LeafStateID lid = g_state_registry->get_successor_leaf_state(predecessor, *op).get_id();
                                
                                if (lid.hash() >= leaf_state_id_map[factor].size()){
                                    // this can happen when using reachability functions in search, since here we might
                                    // find new leaf states
                                    leaf_state_id_map[factor].resize(lid.hash() + 1, -1);
                                    best_prices.resize(lid.hash() + 1, numeric_limits<int>::max());
                                }
                                
                                change |= add_state(lid, cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type()), op, predecessor_id);
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
        
        // TODO why don't we have the special case handling from, e.g., pricing_function, here?
        
        if (has_pre_on_factor && num_effects == g_factors[factor].size()){
            // center op completely overwrites leaf
            const LeafState predecessor = g_state_registry->lookup_leaf_state(LeafStateID(0, factor));

            const LeafStateID succ_state = g_state_registry->get_successor_leaf_state(predecessor, op).get_id();
            
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
                    if (g_factoring->get_search_type() != SAT && g_factoring->get_search_type() != UNSAT){
                        min_price = min(min_price, old_tag.get_cost_of_state(id, factor));
                        assert(min_price != INF);
                    }
                            
                    if (num_effects == 0){
                        add_state(lid, old_tag.get_cost_of_state(id, factor), 0, lid);
                    } else {
                        const LeafState state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));

                        LeafStateID succ_id = g_state_registry->get_successor_leaf_state(state, op).get_id();
            
                        add_state(succ_id, old_tag.get_cost_of_state(id, factor), 0, lid);
                    }
                }
            }
            ++id;
        }

        if (g_factoring->get_search_type() != SAT && g_factoring->get_search_type() != UNSAT && min_price > 0){
            reduce_cost_by(factor, min_price);
        }
        
        assert(g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT || min_price != numeric_limits<int>::max());
#ifdef DEBUG_PATH_RECONSTRUCTION
        cout << "min_price of factor " << factor << " = " << min_price << endl;
#endif
    }
}

bool PathPriceTag::apply_symmetry_permutation(const LeavesPermutation &per) {
    // cout << " APPLY SYMMETRY TO: " << endl;
    // dump();
    //Copy prices and reinitialize affected factors
    vector<vector<int> > old_prices(prices);//g_factors.size());
    vector<unsigned int> old_number_states (number_states);
    vector<int> old_goal_cost (goal_cost);
    vector<vector<PathPriceTagInfo> > old_paths(paths);

    for (int factor : per.get_factors_affected()) {
        //old_prices[factor] = move(prices [factor]);
        number_states[factor] = 0;
        prices[factor] = vector<int>();
        goal_cost[factor] = INF;
        paths[factor] = vector<PathPriceTagInfo>();
    }
    
    for(int to_factor : per.get_factors_affected()) {
        int from_factor = per.get_from_factor(to_factor);

        // cout << " from: " << from_factor << " to " << to_factor << endl;

        assert(find(per.get_factors_affected().begin(), 
                         per.get_factors_affected().end(), to_factor)
               != per.get_factors_affected().end());

        assert(find(per.get_factors_affected().begin(), 
                         per.get_factors_affected().end(), from_factor)
               != per.get_factors_affected().end());

        size_t num_states = old_number_states[from_factor]; 
        for  (size_t id = 0; num_states > 0; ++ id){
            assert (id < leaf_state_id_map[from_factor].size());
            if (leaf_state_id_map[from_factor][id] == -1) {
                continue; 
            }

            size_t leaf_state = leaf_state_id_map[from_factor][id];
            if (leaf_state >= old_prices[from_factor].size() ||
                old_prices[from_factor][leaf_state] == INF)  continue;

            num_states--;
            pair<int, int> to_pair = per.get_new_factor_state_by_old_factor_state(from_factor, id);
            assert(to_pair.first == to_factor);
            int to_val = to_pair.second;

             // cout << "Set from_val: " << id << " from_factor " << from_factor
             //          <<  "  to_val: " << to_val << " of "<<  to_factor << ": " << old_prices[from_factor][leaf_state] << endl;
            
             // LeafState state_print_from = g_state_registry->lookup_leaf_state(LeafStateID(id, from_factor));
             // LeafState state_print_to = g_state_registry->lookup_leaf_state(LeafStateID(to_val, to_factor));

             // state_print_from.dump_pddl(); 
             // cout <<  " -> "; 
             // state_print_to.dump_pddl(); 

            add_state(LeafStateID(to_val, to_factor), old_prices[from_factor][leaf_state], old_paths[from_factor][leaf_state].best_supporter, old_paths[from_factor][leaf_state].predecessor);
        }
    }


    // cout << " NUMBER STATES: " << if_less << endl;
    // for (int i : old_number_states) cout << i << " "; cout << endl;
    // for (int i : number_states) cout << i << " ";  cout << endl << endl;

    // cout << " GOAL COST" << endl;
    // for (int i : old_goal_cost) cout << i << " "; cout << endl;
    // for (int i : goal_cost) cout << i << " ";  cout << endl << endl
    assert (accumulate(number_states.begin(), number_states.end(), 0) == accumulate(old_number_states.begin(), old_number_states.end(), 0));
    assert (accumulate(goal_cost.begin(), goal_cost.end(), 0) == accumulate(old_goal_cost.begin(), old_goal_cost.end(), 0));



    //cout << " APPLIED SYMMETRY: " << endl;
    //dump();

    return true; //We do not need to check whether the prices changed
                 //because the return value is only relevant if
                 //if_less is true and in that case we checked this in
                 //per.produce_less(prices)
}

unique_ptr<CompliantPathGraph> PathPriceTag::clone() const {
    return unique_ptr<CompliantPathGraph> (new PathPriceTag(this));
}
