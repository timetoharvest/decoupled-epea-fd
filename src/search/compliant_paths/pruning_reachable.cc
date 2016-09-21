#include "pruning_reachable.h"

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


PruningReachable PruningReachable::tmp_p_reach;


PruningReachable::PruningReachable(const PruningReachable *tag) {
    if (tag == 0){
        reachable = vector<boost::dynamic_bitset<> >(g_factors.size());
        goal_reached.resize(g_factors.size(), false);
    } else {
        reachable = tag->reachable;
        goal_reached = tag->goal_reached;
    }
}

int PruningReachable::check_duplicate(const State &base_state, int curr_number_duplicates, int g_value, const State &predecessor) {
    
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
            curr_id = search_space->get_node(g_state_registry->lookup_state(curr_id)).get_parent_state_id();
        }
        predecessors.push_back(0);
    }
    
    int replaced_state_dup = -1;

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

            DOMINATION res = g_factoring->get_pruning()->check_dominance(base_state, this, &CPGStorage::pruning_reachable[s]); 
            
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
                    // this is only needed for eager GBFS search, where for duplicate states
                    // reached on a cheaper path, the predecessor is updated
                    g_factoring->set_ignore_current_state(true);
                }
            }       
            
        } else {
            
            DOMINATION res = g_factoring->get_pruning()->check_dominance(base_state, this, &CPGStorage::pruning_reachable[s]);
            
            if (res == EQUAL || res == DOMINATED){
                int old_g = search_space->get_node(s).get_g();
                if (g_value + g_inc_g_by < old_g){
                    // otherwise, that existing state would be reopened
                    g_factoring->set_ignore_current_state(true);
                }
                return dup;
            } else if (res == DOMINATES){
                // set dominated nodes to closed
                SearchNode node = search_space->get_node(s);
                if (node.is_open()){
                    node.close();
                }
            }
        }
    }
    if (replaced_state_dup != -1){
        return replaced_state_dup;
    } else {
        return curr_number_duplicates + 1;
    }
}

int PruningReachable::update_price_tags(int curr_number_duplicates,
                                 const State &base_state,
                                 int g_value,
                                 const State &predecessor,
                                 const Operator *op) const {
    
    if (g_factoring->get_profile() != FORK && op != 0){
        tmp_p_reach = PruningReachable(0);               // for initial state => op == 0
        tmp_p_reach.apply_center_op_to_leaves(*this, *op);
    } else {                
        tmp_p_reach = PruningReachable(this);
    }
    // the rest of this function works on tmp_p_reach!
    
#ifdef DEBUG_SEARCH
    cout << "+++++++++++++++ STARTING RPG" << endl;
#endif
    
    assert(leaf_tags_to_cache.empty());
    
    for (size_t factor = 0; factor < g_factors.size(); factor++){
        
        if (!g_factoring->has_successors(factor)){
            // skip fork leaves that don't have a goal or whose goal is already achieved
            if (g_goals_per_factor[factor].empty() || 
                (g_factoring->get_search_type() == SAT && tmp_p_reach.goal_reached[factor])){
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
            if (tmp_p_reach.reachable[factor].count() == 1){
                size_t leaf_id = 0;
                for (;;){
                    // TODO there should be a better way to find the state
                    if (tmp_p_reach.has_leaf_state(leaf_id, factor)){
                        break;
                    }
                    ++leaf_id;
                }
                if (leaf_price_cache[factor].size() > leaf_id && leaf_price_cache[factor][leaf_id] != StateID::no_state){                    
                    const PruningReachable &tag_to_copy = CPGStorage::pruning_reachable[g_state_registry->lookup_state(leaf_price_cache[factor][leaf_id])];
                    
                    int cost_delta = tmp_p_reach.get_cost_of_state(leaf_id, factor);
                    cost_delta -= tag_to_copy.get_cost_of_state(leaf_id, factor);
                    
                    size_t number_states = tag_to_copy.get_number_states(factor) - 1;
                    for (size_t fact = 0 ; fact < number_leaf_states; fact++){
                        if (fact != leaf_id && tag_to_copy.has_leaf_state(fact, factor)){
                            tmp_p_reach.add_state(fact, factor);
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
        
        size_t curr_number_states = tmp_p_reach.reachable[factor].count();
        
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
                
                if (tmp_p_reach.has_leaf_state(id, factor)){
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
                                if(tmp_p_reach.add_state(leaf_state_successors[factor][id][o].second, factor)) {
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

                                size_t lid = g_state_registry->get_id_of_successor_leaf_state(predecessor, *op);
                                
                                if (lid >= handled.size()){
                                    handled.resize(lid + 1, false);
                                }
                                
                                if (lid >= leaf_state_id_map[factor].size()){
                                    leaf_state_id_map[factor].resize(lid + 1, -1);
                                }
                                
                                if(!fits_store || op->is_center_applicable(base_state)){
                                    if (tmp_p_reach.add_state(lid, factor)){
                                        ++added;
                                    }
                                }
                                
                                if (op->get_affected_factor() != -1 && fits_store){
                                    leaf_state_successors[factor][id].push_back(make_pair(op, lid));
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
                    
                    if (g_factoring->get_search_type() == SAT && !g_factoring->has_successors(factor) && tmp_p_reach.goal_reached[factor]){
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
    
    return tmp_p_reach.check_duplicate(base_state, curr_number_duplicates, g_value, predecessor);
}

const vector<boost::dynamic_bitset<> >& PruningReachable::get_frontier() {
    assert(store_frontier);
    if (frontier.empty()){
        compute_frontier();
    }
    return frontier;
}

void PruningReachable::compute_frontier() {
    assert(g_factoring->get_profile() == FORK);
    
    frontier.resize(g_factors.size());
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        boost::dynamic_bitset<>(g_state_registry->size(factor), false).swap(frontier[factor]);
        for (size_t id = 0; id < leaf_state_successors[factor].size(); ++id){
            if (!has_leaf_state(id, factor)){
                continue;
            }
            for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
                if (!has_leaf_state(leaf_state_successors[factor][id][o].second, factor)){
                    frontier[factor][id] = true;
                }
            }
        }
    }
}

void PruningReachable::store_new_graph(const State &state) {
    CPGStorage::pruning_reachable[state] = tmp_p_reach;
}

void PruningReachable::create_init_state_reachable() {
    PruningReachable reach(0);
        
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        reach.add_state(0, factor);
    }
        
    reach.update_price_tags(-1, g_state_registry->get_initial_state(), 0, g_state_registry->get_initial_state(), 0);
}

void PruningReachable::dump() const {
    // TODO include frontier information
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

