#include "pruning_reachable.h"

#include "cpg_storage.h"
#include "../factoring.h"
#include "../operator.h"
#include "pruning_options.h"
#include "../state_registry.h"
#include "../utils/hash.h"

#include <limits>


using namespace std;


PruningReachableStorage* PruningReachable::cpg_storage;

void PruningReachable::register_cpg_storage(PruningReachableStorage *storage) {
    cpg_storage = storage;
}

PruningReachable::PruningReachable(const PruningReachable *tag) {
    if (tag == 0){
        reachable = vector<boost::dynamic_bitset<> >(g_leaves.size());
        goal_reached.resize(g_leaves.size(), false);
    } else {
        reachable = tag->reachable;
        goal_reached = tag->goal_reached;
    }
}

unique_ptr<CompliantPathGraph> PruningReachable::get_successor_via_center_action(const State &,
                                                                                 const Operator &) const {
    
    assert(g_factoring->get_profile() == FORK); // not supported, yet

    // TODO copy frontier from predecessor?

    return clone();
}

DOMINANCE PruningReachable::check_dominance(const State &other, uint64_t, int, DOMINANCE needed) {
    PruningReachable *other_cpg = &cpg_storage->pruning_reachable[other];
    
    assert(g_factoring->get_profile() == FORK);

    compute_frontier();
    other_cpg->compute_frontier();
        
    bool dominated = needed != DOMINANCE::DOMINATES;
    bool dominates = needed != DOMINANCE::DOMINATED;
    
    // TODO also add some counting here? i.e. if one frontier is larger than the number of reachable states..
    
    // compare prices
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        if (!g_goals_per_factor[factor].empty()){
            if (goal_reachable(factor) && !other_cpg->goal_reachable(factor)){
                dominated = false;
            } else if (!goal_reachable(factor) && other_cpg->goal_reachable(factor)){
                dominates = false;
            }
#ifndef NDEBUG
            else {
                assert(goal_reachable(factor) == other_cpg->goal_reachable(factor));
            }
#endif
        }

        for (LeafStateHash id(0); id < g_state_registry->size(factor); ++id){
            if (g_goals_per_factor[factor].empty() || !CompliantPathGraph::is_leaf_goal_state(id, factor)){
                if (dominates && id < other_cpg->frontier[factor].size() && other_cpg->frontier[factor][id]){
                    // reg. second condition: in satisficing planning (in contrast to optimal), the leaf state spaces
                    // are not necessarily precomputed. thus, the frontier of a decoupled state created early during
                    // search might be smaller than the current size of the leaf state space
                    assert(other_cpg->has_leaf_state(id, factor));
                    if (!has_leaf_state(id, factor)){
                        dominates = false;
                    }
                }
                if (dominated && id < frontier[factor].size() && frontier[factor][id]){
                    assert(has_leaf_state(id, factor));                    
                    if (!other_cpg->has_leaf_state(id, factor)){
                        dominated = false;
                    }
                }
            }

            if (!dominated && !dominates){
                return DOMINANCE::NONE;
            }
        }
    }
    if (dominated && dominates){
#ifdef DEBUG_PRUNING
        cout << "old tag is equal to new one" << endl;
#endif          
        return DOMINANCE::EQUAL;
    } else if (dominated){
#ifdef DEBUG_PRUNING
        cout << "new tag is dominated by old one" << endl;
#endif          
        return DOMINANCE::DOMINATED;
    } else if (dominates){
#ifdef DEBUG_PRUNING
        cout << "new tag dominates old one" << endl;
#endif          
        return DOMINANCE::DOMINATES;
    }
    return DOMINANCE::NONE;
}

void PruningReachable::compute_frontier() {
    assert(g_factoring->get_profile() == FORK);
    assert(pruning->use_frontier_pruning());
    
    if (!frontier.empty()){
        return;
    }
    
    frontier.resize(g_leaves.size());
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        boost::dynamic_bitset<>(g_state_registry->size(factor), false).swap(frontier[factor]);
        for (LeafStateHash id(0); id < leaf_state_successors[factor].size(); ++id){
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

void PruningReachable::store_new_cpg(const State &state) const {
    cpg_storage->store_cpg(state, *this);
}

unique_ptr<CompliantPathGraph> PruningReachable::get_init_state_cpg() {
    unique_ptr<PruningReachable> reach(new PruningReachable(0));
        
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        reach->add_state(LeafStateHash(0), factor);
    }
    reach->update(g_initial_state());

    return reach;
}

void PruningReachable::dump() const {
    if (frontier.empty()){
        cout << "WARNING: No frontier information available" << endl;
    }
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        for (LeafStateHash id(0); id < g_state_registry->size(factor); ++id){
            if (has_leaf_state(id, factor)){
                bool is_frontier = !frontier.empty() && frontier[factor][id];
                LeafState state = g_state_registry->lookup_leaf_state(id, factor);
                if (g_leaves[factor].size() == 1){
                    cout << g_fact_names[g_leaves[factor][0]][state[g_leaves[factor][0]]] << (is_frontier ? " - is frontier state" : "") << endl;
                } else {
                    state.dump_pddl();
                    if (is_frontier){
                        cout << "is frontier state" << endl;
                    }
                }
            }
        }
    }
}

unique_ptr<CompliantPathGraph> PruningReachable::clone() const {
    return unique_ptr<CompliantPathGraph>(new PruningReachable(this));
}

uint64_t PruningReachable::get_hash() const {
    utils::HashState hash_state;
    feed(hash_state, goal_reached);
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        feed(hash_state, reachable[factor]);
        feed(hash_state, frontier[factor]);
    }
    return hash_state.get_hash64();
}
