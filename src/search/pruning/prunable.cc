#include "prunable.h"

#include "../compliant_paths/cpg_storage.h"
#include "pruning.h"
#include "../search_space.h"


using namespace std;


bool Prunable::store_frontier = false;

bool Prunable::propagate_goal_prices = false;



int Prunable::check_dominance(const State &base_state, int curr_number_duplicates, int g_value, const State &predecessor) {
    
#ifdef DEBUG_PRUNING
    cout << "start checking duplicates for state " << base_state.get_id() << ", g = " << g_value << endl;
    dump();
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
        const State s = g_state_registry->get_center_state(base_state, dup);
        
#ifdef DEBUG_PRUNING
        cout << "compare to state " << s.get_id() << ", g = " << search_space->get_node(s).get_g() << endl;
        g_price_tags[s].dump();
#endif
        
        if (!g_factoring->are_closed_nodes_reopened()){
            // in greedy search, close nodes that are dominated by this one as cannot
            // reach anything new from those that cannot be reached from this node
            // if dominated => do not consider this state
            // in this case, all dominated nodes are also dominated by the state
            // that dominates this one

            DOMINATION res = g_factoring->get_pruning()->check_dominance(base_state, this, CPGStorage::storage->get_mutable_cpg(s)); 
            
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
            
            if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
                
                DOMINATION res = g_factoring->get_pruning()->check_dominance(base_state, this, CPGStorage::storage->get_mutable_cpg(s));
            
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
                
            } else {
            
                // pruning for optimal search
                
                int old_g = search_space->get_node(s).get_g();
            
                if (g_value + g_inc_g_by < old_g){
#ifdef DEBUG_PRUNING
                    cout << "new g is lower: " << (g_value + g_inc_g_by) << " < " << old_g << endl;
#endif
                    // new state has a lower g-value and may only replace an
                    // existing state if it dominates the existing one (nodes are closed instead of replaced)
                    // => if the new one does not dominate the old state, need
                    // to make it a new state, no duplicate
                    
                    DOMINATION res = g_factoring->get_pruning()->check_dominance(base_state, this, CPGStorage::storage->get_mutable_cpg(s), DOMINATES);
                    
                    if (res == EQUAL){
#ifdef DEBUG_PRUNING
                        cout << "new state is equal to old one" << endl;
#endif
                        return dup;
                    } else if (res == DOMINATES){
#ifdef DEBUG_PRUNING
                        cout << "new state dominates old one" << endl;
#endif
                        if (find(predecessors.begin(), predecessors.end(), s.get_id().hash()) == predecessors.end()){
                            // must not close a dominated state that is on path to this state
                            // it might have been reopened, so its successors may get lower g
                            SearchNode node = search_space->get_node(s);
                            if (node.is_open()){
                                node.close();
                            }
                        }
                    }
                    
                } else {
#ifdef DEBUG_PRUNING
                    cout << "old g is lower: " << (g_value + g_inc_g_by) << " > " << old_g << endl;
#endif
                    // new state has a higher/equal g-value than the existing one and can
                    // be marked as duplicate if it is subsumed.
                    // if it is not subsumed, it must be considered a new state

                    DOMINATION res = g_factoring->get_pruning()->check_dominance(base_state, this, CPGStorage::storage->get_mutable_cpg(s), DOMINATED);
                    
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
    }
    if (replaced_state_dup != -1){
        return replaced_state_dup;
    } else {
        return curr_number_duplicates + 1;
    }
}