#ifndef COMPLIANT_PATH_GRAPH_H
#define COMPLIANT_PATH_GRAPH_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "../factoring.h"
#include "../globals.h"
#include "../operator_cost.h"
#include "../operator_id.h"
#include "pruning_options.h"

#include <vector>


class LeafState;
class LeafStateID;
class LeavesPermutation;
class Prices;
class SearchSpace;
class State;
class StateID;

namespace stubborn_sets_decoupled {
class StubbornSetsDecoupled;
}

class CompliantPathGraph {
    friend class stubborn_sets_decoupled::StubbornSetsDecoupled; // access to leaf_state_pre/successors
    friend class PruningOptions; // access to leaf_state_pre/successors
    friend class SimulationRelation; // access to leaf_goal_states
    friend class StateRegistry; // build leaf state spaces + init state cpg
    friend class successor_generator::SuccessorGenerator; // access to center_successors
    friend class SearchSpace; // init state cpg + get successors
    

    static void build_leaf_state_space(Prices &prices,
                                       LeafStateHash first_state_id,
                                       LeafFactorID factor,
                                       bool set_leaf_invertibility);

    /*
      this builds all entire leaf state spaces and stores them
    */
    static void build_leaf_state_spaces();
    
    static std::unique_ptr<CompliantPathGraph> get_init_state_cpg();
    
protected:
    
    // contains center actions only, keeping their leaf preconditions
    static std::unique_ptr<successor_generator::SuccessorGenerator> center_action_successor_generator;

    // contains only leaf actions, branching only over the leaf preconditions
    static std::vector<std::unique_ptr<successor_generator::SuccessorGenerator> > leaf_successor_generators;

    static SearchSpace *search_space;
    
    static OperatorCost cost_type;
    
    static std::unique_ptr<PruningOptions> pruning;
    
    static const size_t MAX_LEAF_SPACE_STORE;
    
    static bool compute_leaf_backwards_graph;
    
    static std::vector<bool> precompute_leaf_state_spaces;
    
    // Stores for every leaf state the StateID of the decoupled state
    // from which the CompliantPathGraph can be copied.
    // In inverted-fork factorings, if the leaf state space is rebuilt
    // from a single leaf state, we can save the reconstruction effort
    // and simply copy the final CompliantPathGraph for that factor
    // from a decoupled state that started reconstruction in the same
    // leaf state
    static std::vector<std::vector<StateID> > cpg_cache;
    
    // temporary memory that keeps the leaf state IDs from which the
    // current CompliantPathGraph started reconstruction of the leaf
    // state spaces for inverted-fork leaves
    static std::vector<LeafStateID> leaf_states_to_cache_for_current_cpg;
    
    static std::vector<boost::dynamic_bitset<> > is_a_leaf_goal_state;
    
    // these are only computed if compute_leaf_backwards_graph is true
    static std::vector<std::vector<LeafStateHash> > leaf_goal_states;
    
    static std::vector<std::vector<std::vector<OperatorID> > > center_successors;
    
    static std::vector<std::vector<std::vector<std::pair<OperatorID, LeafStateHash> > > > leaf_state_successors;
    
    static std::vector<std::vector<std::vector<std::pair<OperatorID, LeafStateHash> > > > leaf_state_predecessors;
    
    static std::vector<bool> is_leaf_state_space_scc;

    static std::vector<std::vector<int> > leaf_state_id_map;
    
    static std::vector<size_t> curr_leaf_state_max_id;
    
    static void cache_cpg(const State &state);
    
    // when overwriting the CompliantPathGraph of state, because the new
    // decoupled state dominates the old one, we need to clear the cache
    // for all leaf states that point to state, since it can happen that
    // not the same leaf state is the starting point of construction of
    // the leaf state space in the new CompliantPathGraph
    static void clear_cpg_cache(const State &state);

    static void discard_cpg_cache();

    
    virtual std::unique_ptr<CompliantPathGraph> get_successor_via_center_action(const State &new_center_state, const Operator &op) const = 0;
                                                                          
    virtual DOMINANCE check_dominance(const State &other, uint64_t hash, int g_advantage = 0, DOMINANCE needed = DOMINANCE::NONE) = 0;
    
    virtual void store_new_cpg(const State &state) = 0;

    virtual void discard_new_cpg() const;
    
    virtual bool is_hypercube_covered(const State &base_state, int curr_number_duplicates) const = 0;

public:
    
    static const int INF;
    

    virtual ~CompliantPathGraph() = default;
    
    // TODO has_leaf_state and get_number_states are mainly used to iterate 
    // over leaf state IDs, what about having some kind of iterator providing
    // this functionality?

    virtual bool has_leaf_state(LeafStateHash id, LeafFactorID factor) const = 0;
    
    virtual int get_cost_of_state(LeafStateHash id, LeafFactorID factor) const = 0;
    
    virtual size_t get_number_states(LeafFactorID factor) const = 0;

    virtual bool goal_reachable(LeafFactorID factor) const = 0;
    
    virtual int get_goal_cost(LeafFactorID factor) const = 0;

    virtual void apply_symmetry_permutation(const LeavesPermutation &permutation) = 0;
    
    virtual void dump() const = 0;

    virtual std::unique_ptr<CompliantPathGraph> clone() const = 0;
    
    // returns an approximation of the number of bytes stored in memory of this CPG for factor
    virtual size_t get_memory_footprint() const = 0;

    virtual uint64_t get_hash() const = 0;

    // returns the duplicate counter for this base state and compliant path graph 
    // i.e. the counter of the state subsuming that or the current max + 1
    std::pair<int, bool> check_dominance(const State &base_state,
                                         int curr_number_duplicates,
                                         const State &predecessor,
                                         const Operator &op);
    

    static void initialize();
    
    static void build_leaf_successor_generators();

    static const PruningOptions& get_pruning_options() {
        return *pruning;
    }
    
    static void set_pruning_options(PruningOptions *p) {
        pruning.reset(p);
    }
    
    static void set_search_space(SearchSpace *sp) {
        search_space = sp;
    }
    
    static void set_cost_type(OperatorCost ct);
    
    static OperatorCost get_cost_type() {
        return cost_type;
    }
    
    static bool is_leaf_goal_state(LeafStateHash id, LeafFactorID factor);
    
    static void store_is_leaf_goal_state(const LeafState &state);

    static LeafStateHash get_leaf_state_hash(const std::vector<int> &new_leaf_state, LeafFactorID factor);
    
    static void set_compute_leaf_backwards_graph() {
        compute_leaf_backwards_graph = true;
    }
    
    static void set_precompute_leaf_state_spaces(const std::vector<bool> &precompute) {
        precompute_leaf_state_spaces = precompute;
    }
    
    static bool precompute_leaf_state_space(LeafFactorID factor) {
        return precompute_leaf_state_spaces[factor];
    }

    static bool all_leaves_invertible() {
        for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
            if (!is_leaf_state_space_scc[factor]){
                return false;
            }
        }
        return true;
    }

};

#endif
