#include "state_registry.h"

#include "axioms.h"
#include "factoring.h"
#include "operator.h"
#include "per_state_information.h"
#include "price_tag.h"
#include "search_engine.h"
#include "successor_generator.h"

#include <limits>
#include <stdint.h>

using namespace std;

StateRegistry::StateRegistry()
    : state_data_pool(g_state_packer->get_num_bins()),
      leaf_factor_data_pool(vector<SegmentedArrayVector<PackedStateBin>* >
                                (g_factors.size(), 0)),
      registered_states(0,
                        StateIDSemanticHash(state_data_pool),
                        StateIDSemanticEqual(state_data_pool)),
      registered_leaf_states(vector<LeafFactorIDSet>(g_factors.size(), 
                                LeafFactorIDSet(0, LeafStateIDSemanticHash(leaf_factor_data_pool), 
                                                LeafStateIDSemanticEqual(leaf_factor_data_pool)))),
      cached_initial_state(0) {
    for (size_t i = 0; i < g_factors.size(); ++i){
        leaf_factor_data_pool[i] = new SegmentedArrayVector<PackedStateBin>
                                    (g_leaf_factor_packers[i]->get_num_bins());
    }
}

StateRegistry::~StateRegistry() {
    for (set<PerStateInformationBase *>::iterator it = subscribers.begin();
         it != subscribers.end(); ++it) {
        (*it)->remove_state_registry(this);
    }
    delete cached_initial_state;
    if (g_use_decoupled_search){
        for (size_t i = 0; i < g_factors.size(); i++){
            delete leaf_factor_data_pool[i];
        }
    }
}

StateID StateRegistry::insert_id_or_pop_state() {
    /*
      Attempt to insert a StateID for the last state of state_data_pool
      if none is present yet. If this fails (another entry for this state
      is present), we have to remove the duplicate entry from the
      state data pool.
    */
    StateID id(state_data_pool.size() - 1);
    pair<StateIDSet::iterator, bool> result = registered_states.insert(id);
    bool is_new_entry = result.second;
    if (!is_new_entry) {
        state_data_pool.pop_back();
    }
    assert(registered_states.size() == state_data_pool.size());
    return *result.first;
}

LeafStateID StateRegistry::insert_id_or_pop_leaf_state(int factor) {
    /*
      Attempt to insert a LeafStateID for the last state of leaf_factor_data_pool
      if none is present yet. If this fails (another entry for this state
      is present), we have to remove the duplicate entry from the
      state data pool.
    */
    LeafStateID id(leaf_factor_data_pool[factor]->size() - 1, factor);
    pair<LeafFactorIDSet::iterator, bool> result = registered_leaf_states[factor].insert(id);
    bool is_new_entry = result.second;
    if (!is_new_entry) {
        leaf_factor_data_pool[factor]->pop_back();
    }
    assert(registered_leaf_states[factor].size() == leaf_factor_data_pool[factor]->size());
    return *result.first;
}

State StateRegistry::lookup_state(StateID id) const {
    return State(state_data_pool[id.value], *this, id);
}

LeafState StateRegistry::lookup_leaf_state(LeafStateID id) const {
    return LeafState((*leaf_factor_data_pool[id.factor])[id.value], id);
}

const State &StateRegistry::get_initial_state() {
    if (cached_initial_state == 0) {
        if (g_use_decoupled_search){
            build_leaf_state_spaces();
        }
        
        PackedStateBin *center_buffer = new PackedStateBin[g_state_packer->get_num_bins()];
        for (size_t var = 0; var < g_initial_state_data.size(); ++var) {
            int index = var;
            if (g_use_decoupled_search){
                if (g_belongs_to_factor[var] == -1){
                    index = g_new_index[var];
                } else {
                    continue;
                }
            }
            g_state_packer->set(center_buffer, index, g_initial_state_data[var]);
        }
        
        if (g_use_decoupled_search){
            g_state_packer->set(center_buffer, g_center_vars.size(), 0);
        } else {
            g_axiom_evaluator->evaluate(center_buffer);
        }
        
        state_data_pool.push_back(center_buffer);
        // buffer is copied by push_back
        delete[] center_buffer;
        StateID id = insert_id_or_pop_state();
        cached_initial_state = new State(lookup_state(id));
        
        if (g_use_decoupled_search){
            
            PriceTag init_state_tag(0);
            
            for (size_t i = 0; i < g_factors.size(); ++i){
                init_state_tag.add_state(LeafStateID(0, i), 0);
            }
            
#ifdef DEBUG_SEARCH
            cout << "saturating initial state" << endl;
#endif
            init_state_tag.update_price_tags(-1, *cached_initial_state, 0, *cached_initial_state);

            init_state_tag.cache_leaf_price_tags(*cached_initial_state);
            
            state_duplicate_counter[0] = 0;
            
            g_price_tags[*cached_initial_state] = init_state_tag;
        }
        
#ifdef DEBUG_SEARCH
        cout << "dump init state StateRegistry" << endl;
        cached_initial_state->dump_pddl();
#endif
    }
    return *cached_initial_state;
}

//TODO it would be nice to move the actual state creation (and operator application)
//     out of the StateRegistry. This could for example be done by global functions
//     operating on state buffers (PackedStateBin *).
State StateRegistry::get_successor_state(const State &predecessor, const Operator &op) {
    g_inc_g_by = 0;
    
    assert(op.get_affected_factor() == -1);
    assert(op.is_applicable(predecessor));
        
    assert(!op.is_axiom());
    
    state_data_pool.push_back(predecessor.get_packed_buffer());
    PackedStateBin *buffer = state_data_pool[state_data_pool.size() - 1];

#ifdef DEBUG_SEARCH
    cout << "################ applying " << op.get_name() << " in state " << predecessor.get_id() << endl;
    predecessor.dump_pddl();
    const PriceTag &r = g_price_tags[predecessor];
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        cout << "#states in leaf factor " << factor << ": " << r.get_number_facts(factor) << endl;
        for (size_t i = 0; i < size(factor); ++i){
            if (r.has_leaf_state(i, factor)){
                lookup_leaf_state(LeafStateID(i, factor)).dump_pddl();
            }
        }
    }
#endif


    if (!g_use_decoupled_search) {
        for (size_t i = 0; i < op.get_effects().size(); ++i) {
            const Effect &effect = op.get_effects()[i];
            if (effect.does_fire(predecessor)){
                g_state_packer->set(buffer, effect.var, effect.val);
            }
        }
    } else {
        const vector<const Effect*> &effects = op.get_effects(-1);
        for (size_t i = 0; i < effects.size(); i++){
            // no need to check does_fire here, because no conditional effects allowed (yet)
            g_state_packer->set(buffer, g_new_index[effects[i]->var], effects[i]->val);
        }
    }
    
    vector<int> values;
    if (g_use_decoupled_search){
        g_state_packer->set(buffer, g_center_vars.size(), 0);
        
        values.resize(g_center_vars.size());
        for (size_t i = 0; i < values.size(); i++){
            values[i] = g_state_packer->get(buffer, i);
        }
    } else {
        g_axiom_evaluator->evaluate(buffer);
    }
    
    StateID id = insert_id_or_pop_state();
    
    if (g_use_decoupled_search){
        State s = lookup_state(id);
        
        int old_dup_counter = -1;
        DupCounterTable::iterator pos = state_duplicate_counter.find(id.hash());
        if (pos != state_duplicate_counter.end()){
            old_dup_counter = pos->second;
        }
        
        g_factoring->set_ignore_current_state(false);
        
        PriceTag new_price_tag;
        if (g_factoring->get_profile() != FORK){
            new_price_tag = PriceTag(0);
            new_price_tag.apply_center_op_to_leaves(g_price_tags[predecessor], op);
        } else {                
            new_price_tag = PriceTag(&g_price_tags[predecessor]);
        }
        int g = g_factoring->get_search_space()->get_node(predecessor).get_g();
        g += get_adjusted_action_cost(op, g_factoring->get_cost_type());
        int new_dup_counter = new_price_tag.update_price_tags(old_dup_counter, s, g, predecessor);
        
        bool is_new = false;
        if (old_dup_counter == -1){ // is new center state
            g_price_tags[s] = new_price_tag;
            is_new = true;
            state_duplicate_counter[id.hash()] = 0;
        } else {                       
            state_data_pool.push_back(predecessor.get_packed_buffer());
            buffer = state_data_pool[state_data_pool.size() - 1];
            
            for (size_t i = 0; i < values.size(); i++){
                g_state_packer->set(buffer, i, values[i]);
            }
            g_state_packer->set(buffer, g_center_vars.size(), new_dup_counter);
            
            size_t base_state_id = id.hash();
            
            id = insert_id_or_pop_state();
            s = lookup_state(id);
            
            if (new_dup_counter > old_dup_counter){    // is a new decoupled state
                is_new = true;
                g_price_tags[s] = new_price_tag;
                state_duplicate_counter[base_state_id] = new_dup_counter;
            }
        }
        
        if (is_new){
            // only cache leaf price tags if created new states
            // a known state subsuming the new one not necessarily
            // had the same starting leaf state
            new_price_tag.cache_leaf_price_tags(s);
        } else {
            new_price_tag.discard_leaf_price_cache();
        }
        
#ifdef DEBUG_SEARCH
        cout << "created state " << id << endl;
        s.dump_pddl();
        const PriceTag &r = g_price_tags[s];
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            cout << "#states in leaf factor " << factor << ": " << r.get_number_facts(factor) << endl;
            for (size_t i = 0; i < size(factor); ++i){
                if (r.has_leaf_state(i, factor)){
                    lookup_leaf_state(LeafStateID(i, factor)).dump_pddl();
                }
            }
        }
#endif
        return s;
    }
    return lookup_state(id);
}

void StateRegistry::build_leaf_state_spaces() {

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "+++++++++++++++ STARTING RPG for calculation of minimal goal cost" << endl;
#endif
    
    vector<PackedStateBin*> leaf_buffers(g_factors.size(), 0);
    
    for (size_t i = 0; i < g_initial_state_data.size(); ++i) {
        int factor = g_belongs_to_factor[i];
        if (factor != -1){
            if (leaf_buffers[factor] == 0){
                leaf_buffers[factor] = new PackedStateBin[g_leaf_factor_packers[factor]->get_num_bins()];
            }
            g_leaf_factor_packers[factor]->set(leaf_buffers[factor], g_new_index[i], g_initial_state_data[i]);
        }
    }
    
    g_leaf_state_id_map.resize(g_factors.size(), vector<int>(1, -1));
    g_curr_leaf_state_max_id.resize(g_factors.size(), 0);
    
    PriceTag goal_price_tag(0);
    
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        leaf_factor_data_pool[factor]->push_back(leaf_buffers[factor]);
        delete[] leaf_buffers[factor];
        LeafStateID id = insert_id_or_pop_leaf_state(factor);
        store_is_leaf_goal_state(lookup_leaf_state(id));
        goal_price_tag.add_state(id, 0);
    }
    
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        
        if (!g_factoring->precompute_state_space(factor)){
            continue;
        }
        
        if (!g_factoring->has_successors(factor) && g_goals_per_factor[factor].empty()){
            // skip fork-leaf factor without a goal
            continue;
        }

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
	cout << "	starting RPG for leaf factor " << factor << endl;
        int number_states = goal_price_tag.get_number_facts(factor);
        size_t i = 0;
	while (number_states > 0){
	    if (goal_price_tag.has_leaf_state(i, factor)){
		lookup_leaf_state(LeafStateID(i, factor)).dump_pddl();
	    }
	}
#endif
        
        vector<int> best_prices;
        
        bool change = true;
        while (change) {

            change = false;

	    for (size_t id = 0; id < size(factor); ++id){
		if (!goal_price_tag.has_leaf_state(id, factor)){
		    continue;
		}
                int cost = goal_price_tag.get_cost_of_state(id, factor);
		if (id < best_prices.size()){
                    if (cost > best_prices[id]
                        || (g_factoring->get_search_type() == SAT && best_prices[id] != numeric_limits<int>::max())){
                        continue;
                    }
                }
		
		const LeafState predecessor = lookup_leaf_state(LeafStateID(id, factor));
                
                if (id >= best_prices.size()){
                    best_prices.resize(id + 1, numeric_limits<int>::max());
                }
                bool first_time_seen = best_prices[id] == numeric_limits<int>::max();
                best_prices[id] = cost;
                
                
                vector<const Operator*> applicable_ops;
                
                if (g_factoring->has_successors(factor)){       // is non-fork leaf
                    if (first_time_seen){
                        vector<const Operator*> center_ops;
                        g_successor_generator->generate_applicable_ops_ignore_others(predecessor, center_ops, true);
                        
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
                        if (g_factoring->has_predecessors(factor)){
                            // no need to apply center ops if none of them changes this leaf factor
                            applicable_ops.swap(center_ops);
                        }
                    } else if (g_factoring->has_predecessors(factor)){
                        // no need to apply center ops if none of them changes this leaf factor
                        applicable_ops.resize(g_center_successors[factor][id].size());
                        for (size_t o = 0; o < g_center_successors[factor][id].size(); ++o){
                            applicable_ops[o] = &g_operators[g_center_successors[factor][id][o]];
                        }
                    }
                }

                if (first_time_seen){
                    g_successor_generator->generate_applicable_ops_ignore_others(predecessor, applicable_ops);
                } else {
                    size_t old_size = applicable_ops.size();
                    applicable_ops.resize(applicable_ops.size() + g_leaf_state_successors[factor][id].size());
                    for (size_t o = 0; o < g_leaf_state_successors[factor][id].size(); ++o){
                        applicable_ops[old_size + o] = g_leaf_state_successors[factor][id][o].first;
                    }
                }
                

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
		cout << "	generated " << applicable_ops.size() << " applicable operators" << endl;
#endif
                
                if (id >= g_leaf_state_successors[factor].size()) {
                    g_leaf_state_successors[factor].resize(id + 1);
                }

                // apply all applicable_ops to predecessor and store the outcome          
                for (size_t j = 0; j < applicable_ops.size(); ++j){
                    const Operator *op = applicable_ops[j];

                    if (!op->get_effects(factor).empty()){                        
                        bool is_center_op = op->get_affected_factor() == -1;
                        
                        leaf_factor_data_pool[factor]->push_back(predecessor.get_packed_buffer());
                        PackedStateBin *buffer = (*leaf_factor_data_pool[factor])[leaf_factor_data_pool[factor]->size() - 1];
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                        cout << "################ trying to apply " << op->get_name() << " in state " << predecessor.get_id() << endl;
                        predecessor.dump_pddl();
#endif
                        const vector<const Effect*> &effects = op->get_effects(factor);
                        for (size_t k = 0; k < effects.size(); ++k) {
                            g_leaf_factor_packers[factor]->set(buffer, g_new_index[effects[k]->var], effects[k]->val);
                        }
                        LeafStateID lid = insert_id_or_pop_leaf_state(factor);                        

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                        cout << "created leaf state " << lid << " of cost " << (cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type())) << endl;
                        lookup_leaf_state(lid).dump_pddl();
#endif
                        int op_cost = 0;
                        if (!is_center_op){
                            op_cost = get_adjusted_action_cost(*op, g_factoring->get_cost_type());
                        }
                        
                        store_is_leaf_goal_state(lookup_leaf_state(lid));
                        
                        if (lid.hash() >= g_leaf_state_id_map[factor].size()){
                            g_leaf_state_id_map[factor].resize(lid.hash() + 1, -1);
                        }
                        
                        change |= goal_price_tag.add_state(lid, cost + op_cost);
                        
                        // TODO once the irrelevance_pruning methods are added, it might make sense to do something about
                        // that here, i.e. e.g. the goal states of fork leaves dont need successors
                        // is that actually true? what about finding a globally cheaper goal state behind a goal?
                        
                        if (!is_center_op && first_time_seen){
                            g_leaf_state_successors[factor][id].push_back(make_pair(op, lid.hash()));
                        }
                    }
                }
	    }
	}
	
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "	RPG for leaf factor " << factor << " finished" << endl;
#endif
    
    }
    
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "+++++++++++++++ RPG  for calculation of minimal goal cost FINISHED" << endl;
#endif
   
    size_t min_leaf_factor_size = numeric_limits<int>::max();
    double avg_leaf_factor_size = 0;
    size_t max_leaf_factor_size = 0;
    size_t tmp;
    for (size_t i = 0; i < g_factors.size(); ++i){
        tmp = size(i);
        min_leaf_factor_size = min(min_leaf_factor_size, tmp);
        avg_leaf_factor_size += tmp;
        max_leaf_factor_size = max(max_leaf_factor_size, tmp);
    }
    cout << "min reachable leaf factor size "  << min_leaf_factor_size << endl;
    cout << "avg reachable leaf factor size "  << (int) (avg_leaf_factor_size/g_factors.size()) << endl;
    cout << "max reachable leaf factor size "  << max_leaf_factor_size << endl;
    
    
    g_min_goal_cost = vector<int>(g_factors.size(), numeric_limits<int>::max());
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (!g_factoring->precompute_state_space(factor)){
            cout << "min goal cost of factor " << factor << " not computed" << endl;
            g_min_goal_cost[factor] = 0;
            continue;
        }
        g_min_goal_cost[factor] = goal_price_tag.get_goal_cost(factor);
        
        if (g_min_goal_cost[factor] == -1){
            if (!g_goals_per_factor[factor].empty()){
                cout << "There is a goal that cannot be achieved in factor " << factor << endl;
                for (size_t i = 0; i < g_goals_per_factor[factor].size(); i++){
                    cout << g_fact_names[g_goals_per_factor[factor][i].first][g_goals_per_factor[factor][i].second] << endl;
                }
                exit(EXIT_UNSOLVABLE);
            }
            g_min_goal_cost[factor] = 0;
	} else {
            g_sum_min_goal_cost += g_min_goal_cost[factor];
        }
        if (g_goals_per_factor[factor].empty()){
            cout << "factor " << factor << " does not have a goal" << endl;
        } else {
            cout << "min goal cost of factor " << factor << " : " << g_min_goal_cost[factor] << endl;
        }
    }
    
    g_leaf_successor_generators.resize(g_factors.size());
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (!g_factoring->precompute_state_space(factor)){
            g_leaf_successor_generators[factor] = g_successor_generator->copy();
            g_leaf_successor_generators[factor]->split_ops_center_leaf(factor);
        }
    }
    g_successor_generator->split_ops_center_leaf(-1);
    
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        g_leaf_state_id_map[factor] = vector<int>(size(factor), -1);
        g_curr_leaf_state_max_id[factor] = 0;
    }
    
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "leaving calculate_min_goal_cost function" << endl;
#endif
}

void StateRegistry::store_is_leaf_goal_state(const LeafState &state) const {
    int factor = state.get_id().get_factor();
    size_t id = state.get_id().hash();
    if (!g_goals_per_factor[factor].empty()){
        if (g_is_leaf_goal_state[factor].size() > id && g_is_leaf_goal_state[factor][id]){
            return;
        }
        bool is_goal = true;
        for (size_t g = 0; g < g_goals_per_factor[factor].size(); g++){
            int var = g_goals_per_factor[factor][g].first;
            int val = g_goals_per_factor[factor][g].second;
            if (state[var] != val){
                is_goal = false;
                break;
            }
        }
        if (g_is_leaf_goal_state[factor].size() <= id){
            g_is_leaf_goal_state[factor].resize(id + 1, false);
        }
        g_is_leaf_goal_state[factor][id] = is_goal;
    }
}

size_t StateRegistry::get_max_number_duplicates() const {
    int m = 0;
    for (DupCounterTable::const_iterator it = state_duplicate_counter.begin(); it != state_duplicate_counter.end(); ++it){
        m = max(m, (int) it->second);
    }
    return m;
}

void StateRegistry::output_decoupled_state_space_size() const {
    size_t dec_size_1 = g_center_vars.size() * size();  // sum_s(|center| + sum_leaves(|leaf| * #reachable leaf states in s))
    size_t dec_size_2 = g_center_vars.size() * size();  // sum_s(|center| + sum_leaves(#reachable leaf states in s))
    size_t dec_size_3 = g_center_vars.size() * size();  // sum_s(|center| + sum_leaves(#leaf states in s))
    for (size_t i = 0; i < size(); ++i){
        const PriceTag &tag = g_price_tags[lookup_state(StateID(i))];
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            size_t number_states = tag.get_number_states(factor);
            dec_size_1 += g_factors[factor].size() * number_states;
            dec_size_2 += number_states;
            dec_size_3 += size(factor);
        }
    }
    cout << "decoupled state space size [1] " << dec_size_1 << endl;
    cout << "decoupled state space size [2] " << dec_size_2 << endl;
    cout << "decoupled state space size [3] " << dec_size_3 << endl;
}

void StateRegistry::subscribe(PerStateInformationBase *psi) const {
    subscribers.insert(psi);
}

void StateRegistry::unsubscribe(PerStateInformationBase *const psi) const {
    subscribers.erase(psi);
}
