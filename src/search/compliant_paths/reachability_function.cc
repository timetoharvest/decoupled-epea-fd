#include "reachability_function.h"

#include "cpg_storage.h"
#include "../factoring.h"
#include "../operator.h"
#include "../pruning/pruning.h"
#include "../search_space.h"
#include "../state_registry.h"
#include "../successor_generator.h"
#include "../utilities.h"

#include <limits>


using namespace std;


Reachable Reachable::tmp_reach;

ReachableStorage* Reachable::cpg_storage;


void Reachable::register_cpg_storage(ReachableStorage *storage) {
    cpg_storage = storage;
}

Reachable::Reachable(const Reachable *tag) {
    if (tag == 0){
        reachable = vector<boost::dynamic_bitset<> >(g_factors.size());
        goal_reached.resize(g_factors.size(), false);
    } else {
        reachable = tag->reachable;
        goal_reached = tag->goal_reached;
    }
}

bool Reachable::add_state(const LeafStateID &state) {
    size_t factor = state.get_factor();
    if (leaf_state_id_map[factor][state.hash()] == -1){ // get new index to prevent facts vector from being sparse
        leaf_state_id_map[factor][state.hash()] = curr_leaf_state_max_id[factor];
        ++curr_leaf_state_max_id[factor];
    }
    size_t leaf_id = leaf_state_id_map[factor][state.hash()];

    if (leaf_id >= reachable[factor].size()){
        reachable[factor].resize(leaf_id + 1, false);
    }
    if (!reachable[factor][leaf_id]){
        reachable[factor][leaf_id] = true;
        if (!g_goals_per_factor[factor].empty() && is_leaf_goal_state(state)){
            goal_reached[factor] = true;
        }
        return true;
    }
    return false;
}

Prunable& Reachable::update_price_tags(const State &base_state,
                                       const Operator *op,
                                       const State &) const {
    
    if (g_factoring->get_profile() != FORK && op != 0){
        tmp_reach = Reachable(0);               // for initial state => op == 0
        tmp_reach.apply_center_op_to_leaves(*this, *op);
    } else {                
        tmp_reach = Reachable(this);
    }
    // the rest of this function works on tmp_reach!
    
#ifdef DEBUG_SEARCH
    cout << "+++++++++++++++ STARTING RPG" << endl;
#endif
    
    assert(leaf_tags_to_cache.empty());

    for (size_t factor = 0; factor < g_factors.size(); factor++){
        
        if (!g_factoring->has_successors(factor)){
            // skip fork leaves that don't have a goal or whose goal is already achieved
            if (g_goals_per_factor[factor].empty() || 
                (g_factoring->get_search_type() == SAT && tmp_reach.goal_reached[factor])){
                continue;
            }
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
            if (tmp_reach.reachable[factor].count() == 1){
                size_t leaf_id = 0;
                for (;;){
                    // TODO there should be a better way to find the state
                    if (tmp_reach.has_leaf_state(leaf_id, factor)){
                        break;
                    }
                    ++leaf_id;
                }
                if (leaf_price_cache[factor].size() > leaf_id && leaf_price_cache[factor][leaf_id] != StateID::no_state){                    
                    const Reachable &tag_to_copy = cpg_storage->reachable[g_state_registry->lookup_state(leaf_price_cache[factor][leaf_id])];
                    
                    int cost_delta = tmp_reach.get_cost_of_state(leaf_id, factor);
                    cost_delta -= tag_to_copy.get_cost_of_state(leaf_id, factor);
                    
                    size_t number_states = tag_to_copy.get_number_states(factor) - 1;
                    for (size_t fact = 0 ; fact < number_leaf_states; fact++){
                        if (fact != leaf_id && tag_to_copy.has_leaf_state(fact, factor)){
                            tmp_reach.add_state(LeafStateID(fact, factor));
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
        
        boost::dynamic_bitset<> handled(number_leaf_states, false);
        
        size_t curr_number_states = tmp_reach.reachable[factor].count();
        
        bool change = true;

        while (change) {
           
            change = false;
            
            size_t num_states = 0;
            size_t id = 0;
            while (num_states < curr_number_states) {
                
                if (id >= g_state_registry->size(factor)){
                    change = true;
                    break;
                }
                
                if (tmp_reach.has_leaf_state(id, factor)){
                    ++num_states;

                    if (handled[id]){
                        ++id;
                        continue;
                    }
                    
                    handled[id] = true;
                    
                    assert(id < leaf_state_successors[factor].size() || !precompute_leaf_state_spaces[factor]);
                    
                    size_t added = 0;
                    
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
                                if(tmp_reach.add_state(LeafStateID(leaf_state_successors[factor][id][o].second, factor))) {
                                    ++added;
                                }
                            }
                        }
                    } else {
                      
                        const LeafState predecessor = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));     
                        
                        vector<const Operator*> applicable_ops;
                        
                        if (g_factoring->has_successors(factor)){       // is non-fork leaf
                            bool has_predecessors = g_factoring->has_predecessors(factor);
                            bool store_actions = id >= center_successors[factor].size() || center_successors[factor][id].empty();
                            if (has_predecessors || store_actions){
                                vector<const Operator*> center_ops;
                                g_successor_generator->generate_applicable_ops_ignore_others(predecessor, center_ops, true);
                                
                                if (store_actions){
                                    // store center ops whose preconditions are satisfied by predecessor
                                    if (id >= center_successors[factor].size()){
                                        center_successors[factor].resize(id+1);
                                    }
                                    center_successors[factor][id].reserve(center_ops.size());
                                    for (size_t op = 0; op < center_ops.size(); ++op){
                                        if (center_ops[op]->has_precondition_on(factor)){
                                            center_successors[factor][id].push_back(center_ops[op]->get_index());
                                        }
                                    }
                                    center_successors[factor][id].swap(center_successors[factor][id]);
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
                            if (leaf_state_successors[factor].size() <= id){
                                leaf_state_successors[factor].resize(id + 1);
                            }
                        }

                        // apply all applicable_ops to predecessor and store the outcome          
                        for (size_t j = 0; j < applicable_ops.size(); ++j){
                            const Operator *op = applicable_ops[j];

                            const vector<const Effect*> &effects = op->get_effects(factor);
                            if (!effects.empty() && (fits_store || op->is_center_applicable(base_state))){

                                LeafStateID lid = g_state_registry->get_successor_leaf_state(predecessor, *op).get_id();
                                
                                if (lid.hash() >= handled.size()){
                                    handled.resize(lid.hash() + 1, false);
                                }
                                
                                if (lid.hash() >= leaf_state_id_map[factor].size()){
                                    leaf_state_id_map[factor].resize(lid.hash() + 1, -1);
                                }
                                
                                if(!fits_store || op->is_center_applicable(base_state)){
                                    if (tmp_reach.add_state(lid)){
                                        ++added;
                                    }
                                }
                                
                                if (op->get_affected_factor() != -1 && fits_store){
                                    leaf_state_successors[factor][id].push_back(make_pair(op, lid.hash()));
                                }
                            }
                        }
                        if (fits_store && leaf_state_successors[factor][id].empty()){
                            // to mark "dead end" leaf states
                            leaf_state_successors[factor][id].push_back(make_pair<const Operator*, size_t>(0, 0)); // HACK
                        }
                    }
                    
                    if (added > 0){
                        curr_number_states += added;
                        change = true;
                    }
                    
                    if (g_factoring->get_search_type() == SAT && !g_factoring->has_successors(factor) && tmp_reach.goal_reached[factor]){
                        // in satisficing search => stop once a goal is reachable in fork-leaves
                        change = false;
                        break;
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
    
    return tmp_reach;
}

void Reachable::apply_center_op_to_leaves(const Reachable &old_tag, const Operator &op) {
     
#ifdef DEBUG_SEARCH
    cout << "start to apply center op " << op.get_name() << " to leaves" << endl;
#endif

    for (size_t factor = 0; factor < g_factors.size(); ++factor){
          
        bool has_pre_on_factor = op.has_precondition_on(factor);
        const vector<const Effect*> &eff_on_factor = op.get_effects(factor);
        
        if (!has_pre_on_factor && eff_on_factor.empty()){
            // op does not affect leaf at all => copy everything            
            reachable[factor] = old_tag.reachable[factor];
            goal_reached[factor] = old_tag.goal_reached[factor];
            continue;
        }
        
        if (eff_on_factor.size() == g_factors[factor].size()){
            // center op completely overwrites this leaf factor
                    
            const LeafState pred = g_state_registry->lookup_leaf_state(LeafStateID(0, factor));
            
            add_state(g_state_registry->get_successor_leaf_state(pred, op).get_id());
               
        } else {
            // check which leaf states satisfy center precondition and apply effects
            
            size_t number_states = old_tag.get_number_states(factor);
            size_t id = 0;
            while (number_states > 0) {
                if (old_tag.has_leaf_state(id, factor)){
                    --number_states;
                    
                    LeafStateID lid(id, factor);
                    
                    if (!has_pre_on_factor || op.is_applicable(lid)){
                                
                        if (eff_on_factor.empty()){
                            add_state(lid);
                        } else {
                            const LeafState state = g_state_registry->lookup_leaf_state(lid);

                            add_state(g_state_registry->get_successor_leaf_state(state, op).get_id());
                        }
                    }
                }
                ++id;
            }
        }
    }
}

bool Reachable::has_leaf_state(int id, int factor) const {
    assert(((size_t) id) < leaf_state_id_map[factor].size());
    if (leaf_state_id_map[factor][id] == -1){
        return false;
    }
    size_t leaf_id = leaf_state_id_map[factor][id];
    return leaf_id < reachable[factor].size() && reachable[factor][leaf_id];
}

bool Reachable::has_effective_leaf_state(int id, int factor) const {
    return has_leaf_state(id, factor);
}

int Reachable::get_cost_of_state(int id, int factor) const {
    assert(has_leaf_state(id, factor));
    assert((size_t) leaf_state_id_map[factor][id] < reachable[factor].size());
    return reachable[factor][leaf_state_id_map[factor][id]] ? g_min_action_cost_per_factor[factor] : INF;
}

int Reachable::get_effective_cost_of_state(int id, int factor) const {
    return get_cost_of_state(id, factor);
}

size_t Reachable::get_number_states(int factor) const {
    return reachable[factor].count();
}

size_t Reachable::get_number_effective_states(int factor) const {
    return get_number_states(factor);
}

int Reachable::get_goal_cost(int factor) const {
    if (goal_reached[factor]){
        return g_min_action_cost_per_factor[factor];
    } else {
        return INF;
    }
}

bool Reachable::goal_reachable(int factor) const {
    return goal_reached[factor];
}

const vector< boost::dynamic_bitset< long unsigned int > >& Reachable::get_frontier() {
    cout << "this code must not be reached (reachablility_function.cc)" << endl;
    exit_with(EXIT_CRITICAL_ERROR);
}

void Reachable::store_new_graph(const State &state) const {
    cpg_storage->reachable[state] = tmp_reach;
}

void Reachable::create_init_state_reachable(const State &init_state) {
    if (do_pruning()){
        PruningReachable::create_init_state_reachable(init_state);        
    } else {
        Reachable reach(0);
            
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            reach.add_state(LeafStateID(0, factor));
        }
            
        reach.update_price_tags(g_state_registry->get_initial_state(), 0, g_state_registry->get_initial_state());
        
        tmp_reach.store_new_graph(init_state);
    }
}

void Reachable::dump() const {
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        for (size_t id = 0; id < g_state_registry->size(factor); ++id){
            if (has_leaf_state(id, factor)){
                LeafState state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
                if (g_factors[factor].size() == 1){
                    cout << g_fact_names[g_factors[factor][0]][state[g_factors[factor][0]]] << endl;
                } else {
                    state.dump_pddl();
                }
            }
        }
    }
}

