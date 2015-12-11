#include "price_tag.h"

#include "operator.h"
#include "per_state_information.h"
#include "pruning/pruning.h"
#include "search_space.h"
#include "state_registry.h"
#include "successor_generator.h"
#include "utilities.h"

#include <limits>


using namespace std;


const int PriceTag::INF = -1;
    
const size_t PriceTag::MAX_LEAF_SPACE_STORE = 100000;

vector<vector<StateID> > PriceTag::leaf_price_cache;

vector<pair<int, int> > PriceTag::leaf_tags_to_cache;


PriceTag::PriceTag(const PriceTag *tag) {
    if (tag == 0){
        number_facts = vector<size_t>(g_factors.size(), 0);
        if (g_factoring->get_search_type() != SAT){
            costs = vector<vector<int> >(g_factors.size());
        } else {
            reachable = vector<boost::dynamic_bitset<> >(g_factors.size());
        }
        goal_cost = vector<int>(g_factors.size(), INF);
    } else {
        number_facts = tag->number_facts;
        costs = tag->costs;
        reachable = tag->reachable;
        goal_cost = tag->goal_cost;
    }
}

bool PriceTag::contains_goal(const LeafStateID &fact) const {
    return g_is_leaf_goal_state[fact.get_factor()][fact.hash()];
}

void PriceTag::reduce_cost_by(int factor, int cost) {
    if (goal_cost[factor] != INF){
        // if there is a goal state, its cost must be >= cost!
        goal_cost[factor] -= cost;
    }
    for (size_t id = 0; id < costs[factor].size(); ++id){
        if (costs[factor][id] != INF){
            costs[factor][id] -= cost;
        }
    }
}

int PriceTag::check_duplicate(const State &base_state, int curr_number_duplicates, int g_value, const State &predecessor) {
    
#ifdef DEBUG_PRUNING
    cout << "start checking duplicates for state " << base_state.get_id() << ", g = " << g_value << endl;
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        for (size_t id = 0; id < g_state_registry->size(factor); ++id){
            if (has_leaf_state(id, factor)){
                g_state_registry->lookup_leaf_state(LeafStateID(id, factor)).dump_pddl();
            }
        }
    }
#endif
    
    vector<size_t> predecessors;
    SearchSpace *search_space = g_factoring->get_search_space();
    
    if (g_factoring->are_closed_nodes_reopened()){
        size_t curr_id = predecessor.get_id().hash();
        while (curr_id != 0){
            if (find(predecessors.begin(), predecessors.end(), curr_id) == predecessors.end()){
                predecessors.push_back(curr_id);
            }
            curr_id = search_space->get_node(g_state_registry->lookup_state(StateID(curr_id))).info.parent_state_id.hash();
        }
        predecessors.push_back(0);
    }
    
    SegmentedArrayVector<PackedStateBin> &state_data_pool = g_state_registry->state_data_pool;
    
    int replaced_state_dup = -1;
    State replaced_state = g_initial_state();
    bool replace_price_tag = false;

    for (int dup = 0; dup <= curr_number_duplicates; dup++){
        // check the price tags of all equal center states
        
#ifdef DEBUG_PRUNING
        cout << "current dup counter " << dup << endl;
#endif

        // get other state
        state_data_pool.push_back(base_state.get_packed_buffer());
        PackedStateBin *buffer = state_data_pool[state_data_pool.size() - 1];
        
        g_state_packer->set(buffer, g_center_vars.size(), dup);
        
        StateID id = g_state_registry->insert_id_or_pop_state();
        State s = g_state_registry->lookup_state(id);
        
#ifdef DEBUG_PRUNING
        cout << "compare to state " << s.get_id() << ", g = " << search_space->get_node(s).get_g() << endl;
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            for (size_t id = 0; id < g_state_registry->size(factor); ++id){
                if (g_price_tags[s].has_leaf_state(id, factor)){
                    g_state_registry->lookup_leaf_state(LeafStateID(id, factor)).dump_pddl();
                }
            }
        }
#endif
        
        if (!g_factoring->are_closed_nodes_reopened()){
            // in greedy search, close nodes that are dominated by this one as cannot
            // reach anything new from those that cannot be reached from this node
            // if dominated => do not consider this state
            // in this case, all dominated nodes are also dominated by the state
            // that dominates this one

            DOMINATION res = g_factoring->get_pruning()->check_dominance(s, *this, g_price_tags[s]); 
            
            if (res != NONE){
                if (res == DOMINATES) {
                    // set dominated nodes to closed
                    SearchNode node = search_space->get_node(s);
                    if (node.is_open()){
                        node.close();
                    }
                } else {
                    replaced_state_dup = dup;

                    // otherwise, the path to an existing state dominating this one
                    // will be updated, so that paths to leaf states that are not
                    // reachable in this state cannot be constructed
                    // this is only needed for eager search, where for duplicate states
                    // reached on a cheaper path, the predecessor is updated
                    g_factoring->set_ignore_current_state(true);
                }
            }       
            
        } else if (g_factoring->get_search_type() == SAT){
            
            int old_g = search_space->get_node(s).get_g();
            
            DOMINATION res = g_factoring->get_pruning()->check_dominance(s, *this, g_price_tags[s]);
            
            if (g_value + g_inc_g_by < old_g){
                // new state has a lower g-value and may only replace an
                // existing state if it dominates the existing one
                // => if the new one does not dominate the old state, need
                // to make it a new state, no duplicate
                // in satisficing search, if this state is dominated, mark it as duplicate
                // but don't reopen it in the search
                
                if (res == EQUAL){
                    if (!replace_price_tag){
                        replaced_state_dup = dup;
                        g_factoring->set_ignore_current_state(false);
                    }
                } else if (res == DOMINATES){
                    bool is_predecessor = false;
                    if (find(predecessors.begin(), predecessors.end(), id.hash()) != predecessors.end()){
                        // must not replace a dominated state that is not on path to this state
                        is_predecessor = true;
                    }
                    if (!replace_price_tag && !is_predecessor){
                        replace_price_tag = true;
                        replaced_state_dup = dup;
                        replaced_state = s;
                        g_factoring->set_ignore_current_state(false);
                    } else {
                        // set dominated nodes to closed
                        SearchNode node = search_space->get_node(s);
                        if (node.is_open()){
                            node.close();
                        }
                    }
                } else if (res == DOMINATED){
                    if (!replace_price_tag){
                        g_factoring->set_ignore_current_state(true);
                        replaced_state_dup = dup;
                    }
                }
            } else {
                // new state has a higher/equal g-value than the existing one and can
                // be marked as duplicate if it is subsumed.
                // if it is not subsumed, it must be considered a new state
                // in satisficing search, if this state dominates an existing one
                // close existing and consider this one a new state
                
                if (res == EQUAL || res == DOMINATED){
                    // new state is dominated by existing one
                    if (!replace_price_tag){
                        replaced_state_dup = dup;
                    }
                } else if (res == DOMINATES){
                    SearchNode node = search_space->get_node(s);
                    if (node.is_open()){
                        node.close();
                    }
                }
            }
        } else {
            // pruning for optimal search
            
            int old_g = search_space->get_node(s).get_g();
        
            if (g_value + g_inc_g_by < old_g){
#ifdef DEBUG_PRUNING
                cout << "new g is lower: " << (g_value + g_inc_g_by) << " < " << old_g << endl;
#endif
                // new state has a lower g-value and may only replace an
                // existing state if it dominates the existing one
                // => if the new one does not dominate the old state, need
                // to make it a new state, no duplicate
                
                DOMINATION res = g_factoring->get_pruning()->check_dominance(s, *this, g_price_tags[s], DOMINATES);
                
                if (res == EQUAL){
#ifdef DEBUG_PRUNING
                    cout << "new state is equal to old one" << endl;
#endif
                    return dup;
                } else if (res == DOMINATES){
#ifdef DEBUG_PRUNING
                    cout << "new state dominates old one" << endl;
#endif
                    if (find(predecessors.begin(), predecessors.end(), id.hash()) != predecessors.end()){
                        // must not replace a dominated state that is not on path to this state
                        continue;
                    }
                    // TODO see also below
                    // whenever a PriceTag is overwritten, all classes that store objects indexed
                    // by StateIDs have to be notified, so they can also overwrite the respective data
                    g_price_tags[s] = *this;
                    return dup;
                }
                
            } else {
#ifdef DEBUG_PRUNING
                cout << "old g is lower: " << (g_value + g_inc_g_by) << " > " << old_g << endl;
#endif
                // new state has a higher/equal g-value than the existing one and can
                // be marked as duplicate if it is subsumed.
                // if it is not subsumed, it must be considered a new state

                DOMINATION res = g_factoring->get_pruning()->check_dominance(s, *this, g_price_tags[s], DOMINATED);
                
                if (res == EQUAL || res == DOMINATED){
                    // new state is dominated by existing one
#ifdef DEBUG_PRUNING
                    cout << "new state is equal to/dominated by old one" << endl;
#endif
                    return dup;
                }
            }
        }
    }
    if (replaced_state_dup != -1){
        if (replace_price_tag){
            // replace old PriceTag as this is done in StateRegistry only for new states
            // TODO see above
            g_price_tags[replaced_state] = *this;
        }
        return replaced_state_dup;
    } else {
        return curr_number_duplicates + 1;
    }
}

bool PriceTag::add_state(const LeafStateID &state, int cost) {
    size_t factor = state.get_factor();
    if (g_leaf_state_id_map[factor][state.hash()] == -1){ // get new index to prevent facts vector from being sparse
        g_leaf_state_id_map[factor][state.hash()] = g_curr_leaf_state_max_id[factor];
        ++g_curr_leaf_state_max_id[factor];
    }
    size_t leaf_id = g_leaf_state_id_map[factor][state.hash()];

    if (g_factoring->get_search_type() != SAT){
        if (leaf_id >= costs[factor].size()){
            costs[factor].resize(leaf_id + 1, INF);
        }
        if (costs[factor][leaf_id] == INF){
            costs[factor][leaf_id] = cost;
            ++number_facts[factor];
            if (!g_goals_per_factor[factor].empty() && contains_goal(state)){
                if (goal_cost[factor] == INF || goal_cost[factor] > cost){
                    goal_cost[factor] = cost;
                }
            }
            return true;
        } else if (costs[factor][leaf_id] > cost){
            costs[factor][leaf_id] = cost;
            if (!g_goals_per_factor[factor].empty() && contains_goal(state)){
                if (goal_cost[factor] > cost){
                    goal_cost[factor] = cost;
                }
            }
            return true;
        }
    } else {
        if (leaf_id >= reachable[factor].size()){
            reachable[factor].resize(leaf_id + 1, false);
        }
        if (!reachable[factor][leaf_id]){
            reachable[factor][leaf_id] = true;
            ++number_facts[factor];
            if (!g_goals_per_factor[factor].empty() && contains_goal(state)){
                if (goal_cost[factor] == INF){
                    goal_cost[factor] = g_min_action_cost;
                }
            }
            return true;
        }
    }
    return false;
}

int PriceTag::update_price_tags(int curr_number_duplicates,
                                const State &base_state,
                                int g_value,
                                const State &predecessor) {
    
#ifdef DEBUG_SEARCH
    cout << "+++++++++++++++ STARTING RPG" << endl;
#endif
    
    assert(leaf_tags_to_cache.empty());

    for (size_t factor = 0; factor < g_factors.size(); factor++){
        
        // skip fork leaves that don't have a goal!
        if (!g_factoring->has_successors(factor) && g_goals_per_factor[factor].empty()){
            continue;
        }
        
        size_t number_leaf_states = g_state_registry->size(factor);

#ifdef DEBUG_SEARCH
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
        
        bool is_ifork_leaf = !g_factoring->has_predecessors(factor);
        
        if (g_factoring->get_profile() != FORK && is_ifork_leaf){     // is inv-fork leaf
            // the two above conditions together prevent errors in case the CG is not connected.
            // i.e. g_leaf_price_cache is empty for fork topologies
            
            // no need to rebuild the reachable leaf state space every time
            // instead: do it once, cache which State has the same start leaf state
            // and copy everything from that state
            if (number_facts[factor] == 1){
                size_t leaf_id = 0;
                for (;;){
                    // TODO there should be a better way to find the state
                    if (has_leaf_state(leaf_id, factor)){
                        break;
                    }
                    ++leaf_id;
                }
                if (leaf_price_cache[factor].size() > leaf_id && leaf_price_cache[factor][leaf_id] != StateID::no_state){                    
                    const PriceTag &tag_to_copy = g_price_tags[g_state_registry->lookup_state(leaf_price_cache[factor][leaf_id])];
                    
                    int cost_delta = get_cost_of_state(leaf_id, factor);
                    cost_delta -= tag_to_copy.get_cost_of_state(leaf_id, factor);
                    
                    size_t number_states = tag_to_copy.get_number_states(factor) - 1;
                    for (size_t fact = 0 ; fact < number_leaf_states; fact++){
                        if (fact != leaf_id && tag_to_copy.has_leaf_state(fact, factor)){
                            int cost = tag_to_copy.get_cost_of_state(fact, factor);
                            add_state(LeafStateID(fact, factor), cost + cost_delta);
                            if (--number_states == 0){
                                break;
                            }
                        }       
                    }
                    continue;
                } else {
                    leaf_tags_to_cache.push_back(make_pair(leaf_id, factor));
                }
            }
        }
        
        vector<int> best_prices(number_leaf_states, numeric_limits<int>::max());
        
        bool change = true;

        while (change) {
           
            change = false;
            
            size_t number_states = 0;
            size_t id = 0;
            while (number_states < number_facts[factor]) {
                
                if (g_factoring->get_search_type() == SAT && !g_factoring->has_successors(factor) && get_goal_cost(factor) >= 0){
                    // in satisficing search => stop once a goal is reachable in fork-leaves
                    change = false;
                    break;
                }
                if (id >= g_state_registry->size(factor)){
                    change = true;
                    break;
                }
                if (has_leaf_state(id, factor)){
                    ++number_states;
                
                    int cost = get_cost_of_state(id, factor);
                    if (best_prices[id] <= cost){
                        ++id;
                        continue;
                    }
                    
                    best_prices[id] = cost;
                    
                    assert(id < g_leaf_state_successors[factor].size() || !g_factoring->precompute_state_space(factor));
                    
                    if (g_factoring->precompute_state_space(factor) || 
                            (id < g_leaf_state_successors[factor].size() && !g_leaf_state_successors[factor][id].empty())){
                        if (!g_factoring->precompute_state_space(factor) && g_leaf_state_successors[factor][id][0].first == 0){
                            // is "dead-end" leaf state
                            ++id;
                            continue;
                        }
                        for (size_t o = 0; o < g_leaf_state_successors[factor][id].size(); ++o){
                            const Operator *op = g_leaf_state_successors[factor][id][o].first;
                            if (is_ifork_leaf || op->is_center_applicable(base_state)){
                                change |= add_state(LeafStateID(g_leaf_state_successors[factor][id][o].second, factor), 
                                                cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type()));
                            }
                        }
                    } else {
                        SegmentedArrayVector<PackedStateBin> *leaf_factor_data_pool = g_state_registry->leaf_factor_data_pool[factor];
                      
                        const LeafState predecessor = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));     
                        
                        vector<const Operator*> applicable_ops;
                        
                        if (g_factoring->has_successors(factor)){       // is non-fork leaf
                            bool has_predecessors = g_factoring->has_predecessors(factor);
                            bool store_actions = id >= g_center_successors[factor].size() || g_center_successors[factor][id].empty();
                            if (has_predecessors || store_actions){
                                vector<const Operator*> center_ops;
                                g_successor_generator->generate_applicable_ops_ignore_others(predecessor, center_ops, true);
                                
                                if (store_actions){
                                    // store center ops whose preconditions are satisfied by predecessor
                                    if (id >= g_center_successors[factor].size()){
                                        g_center_successors[factor].resize(id+1);
                                    }
                                    g_center_successors[factor][id].reserve(center_ops.size());
                                    for (size_t op = 0; op < center_ops.size(); ++op){
                                        if (center_ops[op]->has_precondition_on(factor)){
                                            g_center_successors[factor][id].push_back(center_ops[op]->get_index());
                                        }
                                    }
                                    g_center_successors[factor][id].swap(g_center_successors[factor][id]);
                                }
                            
                                if (has_predecessors){
                                    // no need to apply center ops if none of them changes this leaf factor
                                    applicable_ops.swap(center_ops);
                                }
                            }
                        }

                        g_leaf_successor_generators[factor]->generate_applicable_ops_ignore_others(predecessor, applicable_ops);
                        
                        bool fits_store = id < MAX_LEAF_SPACE_STORE;
                        if (fits_store){
                            if (g_leaf_state_successors[factor].size() <= id){
                                g_leaf_state_successors[factor].resize(id + 1);
                            }
                        }

                        // apply all applicable_ops to predecessor and store the outcome          
                        for (size_t j = 0; j < applicable_ops.size(); ++j){
                            const Operator *op = applicable_ops[j];

                            const vector<const Effect*> &effects = op->get_effects(factor);
                            if (!effects.empty() && (fits_store || op->is_center_applicable(base_state))){
                                leaf_factor_data_pool->push_back(predecessor.get_packed_buffer());
                                PackedStateBin *buffer = (*leaf_factor_data_pool)[leaf_factor_data_pool->size() - 1];

                                for (size_t k = 0; k < effects.size(); ++k) {
                                    g_leaf_factor_packers[factor]->set(buffer, g_new_index[effects[k]->var], effects[k]->val);
                                }
                                LeafStateID lid = g_state_registry->insert_id_or_pop_leaf_state(factor);
                                
                                if (lid.hash() >= best_prices.size()){
                                    best_prices.resize(lid.hash() + 1, numeric_limits<int>::max());
                                }
                                
                                if (lid.hash() >= g_leaf_state_id_map[factor].size()){
                                    g_leaf_state_id_map[factor].resize(lid.hash() + 1, -1);
                                }
                                
                                if(!fits_store || op->is_center_applicable(base_state)){
                                    change |= add_state(lid, cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type()));
                                }
                                
                                if (op->get_affected_factor() != -1 && fits_store){
                                    g_leaf_state_successors[factor][id].push_back(make_pair(op, lid.hash()));
                                }
                            }
                        }
                        if (fits_store && g_leaf_state_successors[factor][id].empty()){
                            // to mark "dead end" leaf states
                            g_leaf_state_successors[factor][id].push_back(make_pair<const Operator*, size_t>(0, 0)); // HACK
                        }
                    }
                }
                ++id;
            }
        }
        
#ifdef DEBUG_SEARCH
    cout << "   RPG for leaf factor " << factor << " finished" << endl;
#endif

    }
        
#ifdef DEBUG_SEARCH
    cout << "+++++++++++++++ RPG FINISHED" << endl;
#endif
    
    if (curr_number_duplicates == -1){
        return 0;
    }
    
    return check_duplicate(base_state, curr_number_duplicates, g_value, predecessor);
}

void PriceTag::apply_center_op_to_leaves(const PriceTag &old_tag, const Operator &op) {
     
#ifdef DEBUG_SEARCH
    cout << "start to apply center op " << op.get_name() << " to leaves" << endl;
#endif
    
    vector<SegmentedArrayVector<PackedStateBin>* > &leaf_factor_data_pool = g_state_registry->leaf_factor_data_pool;

    for (size_t factor = 0; factor < g_factors.size(); ++factor){
          
        bool has_pre_on_factor = op.has_precondition_on(factor);
        const vector<const Effect*> &eff_on_factor = op.get_effects(factor);
        
        if (!has_pre_on_factor && eff_on_factor.empty()){
            // op does not affect leaf at all => copy everything
            
            number_facts[factor] = old_tag.number_facts[factor];
            if (g_factoring->get_search_type() != SAT){
                costs[factor] = old_tag.costs[factor];
            } else {
                reachable[factor] = old_tag.reachable[factor];
            }
            goal_cost[factor] = old_tag.goal_cost[factor];
        }
        
        int min_price = has_pre_on_factor ? numeric_limits<int>::max() : 0;
        
        if (eff_on_factor.size() == g_factors[factor].size()){
            // center op completely overwrites this leaf factor
                    
            const LeafState predecessor = g_state_registry->lookup_leaf_state(LeafStateID(0, factor));
            leaf_factor_data_pool[factor]->push_back(predecessor.get_packed_buffer());
            PackedStateBin *buffer = (*leaf_factor_data_pool[factor])[leaf_factor_data_pool[factor]->size() - 1];

            for (size_t post = 0; post < eff_on_factor.size(); post++) {
                int var = g_new_index[eff_on_factor[post]->var];
                int val = eff_on_factor[post]->val;
                g_leaf_factor_packers[factor]->set(buffer, var, val);
            }
            
            LeafStateID succ_state = g_state_registry->insert_id_or_pop_leaf_state(factor);            
            
            add_state(succ_state, 0);
            
            if (has_pre_on_factor && g_factoring->get_search_type() != SAT){
                int number_states = old_tag.get_number_states(factor);
                size_t id = 0;
                while (number_states > 0 && min_price > 0) {
                    if (old_tag.has_leaf_state(id, factor)){
                        --number_states;
                        min_price = min(min_price, old_tag.get_cost_of_state(id, factor));
                    }
                    ++id;
                }
            }
            
        } else {
            
            if (has_pre_on_factor && g_factors[factor].size() == 1){
                // special case handling for 1-variable leaves
                
                const LeafState state = g_state_registry->lookup_leaf_state(LeafStateID(0, factor));
                leaf_factor_data_pool[factor]->push_back(state.get_packed_buffer());
                PackedStateBin *buffer = (*leaf_factor_data_pool[factor])[leaf_factor_data_pool[factor]->size() - 1];
                
                const Condition *precondition = op.get_preconditions(factor)[0];
                int var = g_new_index[precondition->var];
                g_leaf_factor_packers[factor]->set(buffer, var, precondition->val);
                LeafStateID pre_id = g_state_registry->insert_id_or_pop_leaf_state(factor);
                
                LeafStateID eff_id = LeafStateID::no_state;
                
                if (!eff_on_factor.empty()){
                    leaf_factor_data_pool[factor]->push_back(state.get_packed_buffer());
                    PackedStateBin *buffer = (*leaf_factor_data_pool[factor])[leaf_factor_data_pool[factor]->size() - 1];

                    g_leaf_factor_packers[factor]->set(buffer, var, eff_on_factor[0]->val);

                    eff_id = g_state_registry->insert_id_or_pop_leaf_state(factor);
                } else {
                    eff_id = pre_id;
                }
                
                int cost = old_tag.get_cost_of_state(pre_id.hash(), factor);
                
                min_price = min(min_price, cost);
                
                add_state(eff_id, cost);
                
                
            } else {
                // check which leaf states satisfy center precondition and apply effects
                
                int number_states = old_tag.get_number_states(factor);
                size_t id = 0;
                while (number_states > 0) {
                    if (old_tag.has_leaf_state(id, factor)){
                        --number_states;
                        
                        LeafStateID lid(id, factor);
                        
                        if (!has_pre_on_factor || op.is_applicable(lid)){
                            
                            int cost = old_tag.get_cost_of_state(id, factor);
                            
                            min_price = min(min_price, cost);
                                    
                            if (eff_on_factor.empty()){
                                add_state(lid, cost);
                            } else {
                                const LeafState state = g_state_registry->lookup_leaf_state(lid);
                                leaf_factor_data_pool[factor]->push_back(state.get_packed_buffer());
                                PackedStateBin *buffer = (*leaf_factor_data_pool[factor])[leaf_factor_data_pool[factor]->size() - 1];

                                for (size_t post = 0; post < eff_on_factor.size(); ++post) {
                                    int var = g_new_index[eff_on_factor[post]->var];
                                    int val = eff_on_factor[post]->val;
                                    g_leaf_factor_packers[factor]->set(buffer, var, val);
                                }
                                
                                LeafStateID succ_id = g_state_registry->insert_id_or_pop_leaf_state(factor);
                    
                                add_state(succ_id, cost);
                            }
                        }
                    }
                    ++id;
                }
            }
	}
	
	
        if (g_factoring->get_search_type() != SAT && min_price > 0){
            g_inc_g_by += min_price;
            reduce_cost_by(factor, min_price);
        }
        
        assert(g_factoring->get_search_type() == SAT || (min_price < numeric_limits<int>::max() && min_price >= 0));
#ifdef DEBUG_SEARCH
        cout << "min_price of factor " << factor << " = " << min_price << endl;
#endif
    }
}

void PriceTag::cache_leaf_price_tags(const State &state) {
    if (g_factoring->get_profile() != FORK && !leaf_tags_to_cache.empty()){
        for (size_t i = 0; i < leaf_tags_to_cache.size(); i++){
            size_t leaf_id = leaf_tags_to_cache[i].first;
            int factor = leaf_tags_to_cache[i].second;
            if (leaf_price_cache[factor].size() <= leaf_id){
                leaf_price_cache[factor].resize(leaf_id + 1, StateID::no_state);
            }
            leaf_price_cache[factor][leaf_id] = state.get_id();
        }
        vector<pair<int, int> >().swap(leaf_tags_to_cache);
    }
}

void PriceTag::discard_leaf_price_cache() {
    if (g_factoring->get_profile() != FORK && !leaf_tags_to_cache.empty()){
        vector<pair<int, int> >().swap(leaf_tags_to_cache);
    }
}

bool PriceTag::has_leaf_state(int id, int factor) const {
    if (g_leaf_state_id_map[factor][id] == -1){
        return false;
    }
    size_t leaf_id = g_leaf_state_id_map[factor][id];
    if (g_factoring->get_search_type() == SAT){
        return leaf_id < reachable[factor].size() && reachable[factor][leaf_id];
    }
    return leaf_id < costs[factor].size() && costs[factor][leaf_id] != INF;
}

int PriceTag::get_cost_of_state(int id, int factor) const {
    assert(g_factoring->get_search_type() == SAT || (size_t) g_leaf_state_id_map[factor][id] < costs[factor].size());
    assert(g_factoring->get_search_type() != SAT || (size_t) g_leaf_state_id_map[factor][id] < reachable[factor].size());
    if (g_factoring->get_search_type() == SAT){
        return reachable[factor][g_leaf_state_id_map[factor][id]] ? g_min_action_cost : INF;
    }
    return costs[factor][g_leaf_state_id_map[factor][id]];
}

size_t PriceTag::get_number_states(int factor) const {
    return number_facts[factor];
}

int PriceTag::get_goal_cost(int factor) const {
    return goal_cost[factor];
}

void PriceTag::initialize_leaf_cache() {
    // initialize cache if at least one inv-fork leaf present
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (!g_factoring->has_predecessors(factor)){
            leaf_price_cache = vector<vector<StateID> >(g_factors.size());
            break;
        }
    }
}

