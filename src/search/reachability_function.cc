#include "reachability_function.h"

#include "cpg_storage.h"
#include "factoring.h"
#include "operator.h"
#include "pruning.h"
#include "search_space.h"
#include "state_registry.h"
#include "successor_generator.h"
#include "utilities.h"

#include <limits>


using namespace std;


Reachable Reachable::tmp_reach;


Reachable::Reachable(const Reachable *tag) {
    if (tag == 0){
        reachable = vector<boost::dynamic_bitset<> >(g_factors.size());
        goal_reached.resize(g_factors.size(), false);
    } else {
        reachable = tag->reachable;
        goal_reached = tag->goal_reached;
    }
}

int Reachable::check_duplicate(const State &base_state, int curr_number_duplicates, int g_value, const State &predecessor) {
    
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
        StateID curr_id = predecessor.get_id();
        while (curr_id.hash() != 0){
            if (find(predecessors.begin(), predecessors.end(), curr_id.hash()) == predecessors.end()){
                predecessors.push_back(curr_id.hash());
            }
            curr_id = search_space->get_node(g_state_registry->lookup_state(curr_id)).info.parent_state_id;
        }
        predecessors.push_back(0);
    }
    
    int replaced_state_dup = -1;
    State replaced_state = g_initial_state();
    bool replace_price_tag = false;

    for (int dup = 0; dup <= curr_number_duplicates; dup++){
        // check the price tags of all equal center states
        
#ifdef DEBUG_PRUNING
        cout << "current dup counter " << dup << endl;
#endif

        // get other state
        State s = g_state_registry->get_center_state(base_state, dup);
        
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

            DOMINATION res = g_factoring->get_pruning()->check_dominance(this, CPGStorage::get_cpg(s)); 
            
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
            
        } else {
            
            int old_g = search_space->get_node(s).get_g();
            
            DOMINATION res = g_factoring->get_pruning()->check_dominance(this, CPGStorage::get_cpg(s));
            
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
                    if (find(predecessors.begin(), predecessors.end(), s.get_id().hash()) != predecessors.end()){
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
        }
    }
    if (replaced_state_dup != -1){
        if (replace_price_tag){
            // replace old PriceTag as this is done in StateRegistry only for new states
            store_new_graph(replaced_state);
        }
        return replaced_state_dup;
    } else {
        return curr_number_duplicates + 1;
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

int Reachable::update_price_tags(int curr_number_duplicates,
                                 const State &base_state,
                                 int g_value,
                                 const State &predecessor,
                                 const Operator *op) const {
    
    if (g_factoring->get_profile() != FORK && op != 0){
        tmp_reach = Reachable(0);               // for initial state => op == 0
        tmp_reach.apply_center_op_to_leaves(*this, *op);
    } else {                
        tmp_reach = Reachable(this);
    }
    
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
                    const Reachable &tag_to_copy = CPGStorage::reach_tags[g_state_registry->lookup_state(leaf_price_cache[factor][leaf_id])];
                    
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
        
        bool change = true;

        while (change) {
           
            change = false;
            
            size_t num_states = 0;
            size_t id = 0;
            while (num_states < tmp_reach.reachable[factor].count()) {
                
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
                    
                    assert(id < leaf_state_successors[factor].size() || !g_factoring->precompute_state_space(factor));
                    
                    if (g_factoring->precompute_state_space(factor) || 
                            (id < leaf_state_successors[factor].size() && !leaf_state_successors[factor][id].empty())){
                        if (!g_factoring->precompute_state_space(factor) && leaf_state_successors[factor][id][0].first == 0){
                            // is "dead-end" leaf state
                            ++id;
                            continue;
                        }
                        for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
                            const Operator *op = leaf_state_successors[factor][id][o].first;
                            if (is_ifork_leaf || op->is_center_applicable(base_state)){
                                change |= tmp_reach.add_state(LeafStateID(leaf_state_successors[factor][id][o].second, factor));
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
                                    change |= tmp_reach.add_state(lid);
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
                    
                    
                    if (!g_factoring->has_successors(factor) && tmp_reach.goal_reached[factor]){
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
    
    if (curr_number_duplicates == -1){
        return 0;
    }
    
    return tmp_reach.check_duplicate(base_state, curr_number_duplicates, g_value, predecessor);
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
    if (leaf_state_id_map[factor][id] == -1){
        return false;
    }
    size_t leaf_id = leaf_state_id_map[factor][id];
    return leaf_id < reachable[factor].size() && reachable[factor][leaf_id];
}

int Reachable::get_cost_of_state(int id, int factor) const {
    assert((size_t) leaf_state_id_map[factor][id] < reachable[factor].size());
    return reachable[factor][leaf_state_id_map[factor][id]] ? g_min_action_cost_per_factor[factor] : INF;
}

size_t Reachable::get_number_states(int factor) const {
    return reachable[factor].count();
}

int Reachable::get_goal_cost(int factor) const {
    if (goal_reached[factor]){
        return g_min_action_cost_per_factor[factor];
    } else {
        return -1;
    }
}

void Reachable::store_new_graph(const State &state) {
    CPGStorage::reach_tags[state] = tmp_reach;
}

void Reachable::create_init_state_reachable() {
    Reachable reach(0);
        
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        reach.add_state(LeafStateID(0, factor));
    }
        
    reach.update_price_tags(-1, g_state_registry->get_initial_state(), 0, g_state_registry->get_initial_state(), 0);
}

