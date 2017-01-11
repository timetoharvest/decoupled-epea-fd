#ifndef COMPLIANT_PATH_GRAPH_H
#define COMPLIANT_PATH_GRAPH_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "../factoring.h"
#include "../globals.h"
#include "../por/simple_stubborn_sets.h"
#include "../pruning/prunable.h"

#include <vector>

class LeafState;
class LeafStateID;
class LeavesPermutation;
class State;
class StateID;

class CompliantPathGraph : public Prunable {
    friend class POR::SimpleStubbornSets; // access to leaf_state_successors
    friend class SimulationRelation; // access to leaf_state_pre/successors
    friend class StateRegistry; // build leaf state spaces + init state cpg
    friend class SuccessorGeneratorSwitch; // access to center_successors
    friend class SearchSpace; // to update CPGs when reconstructing symmetry path

    
    /*
      this builds all entire leaf state spaces and stores them
    */
    static void build_leaf_state_spaces();
    
    static CompliantPathGraph& get_init_state_cpg();

    static CompliantPathGraph& get_real_init_state_cpg();
    
protected:
    
    static const size_t MAX_LEAF_SPACE_STORE;
    
    static bool compute_leaf_backwards_graph;
    
    static std::vector<bool> precompute_leaf_state_spaces;
    
    static std::vector<std::vector<StateID> > leaf_price_cache;
    
    // first is id, second is leaf factor
    static std::vector<std::pair<int, int> > leaf_tags_to_cache;
    
    static std::vector<boost::dynamic_bitset<> > is_a_leaf_goal_state;
    
    // these are only computed when performing goal price propagation
    static std::vector<std::vector<size_t> > leaf_goal_states;
    
    static std::vector<std::vector<std::vector<size_t> > > center_successors;
    
    static std::vector<std::vector<std::vector<std::pair<const Operator*, size_t> > > > leaf_state_successors;
    
    static std::vector<std::vector<std::vector<std::pair<const Operator*, size_t> > > > leaf_state_predecessors;
    
    static std::vector<std::vector<int> > leaf_state_id_map;
    
    static std::vector<size_t> curr_leaf_state_max_id;
    
    static void cache_leaf_price_tags(const State &state);
    
    static void discard_leaf_price_cache();
    
    // returns the new duplicate counter
    virtual CompliantPathGraph& update_price_tags(const State &base_state,
                                                  const Operator *op,
                                                  const State &predecessor) const = 0;
    
public:
    
    static const int INF;
    
    virtual ~CompliantPathGraph() = default;
    
    // TODO has_leaf_state and get_number_states are mainly used to iterate 
    // over leaf state IDs, what about having some kind of iterator providing
    // this functionality?
    
    virtual void store_new_graph(const State &state) const = 0;

    virtual void discard_new_graph() const;

    virtual bool has_leaf_state(int id, int factor) const = 0;
    
    virtual int get_cost_of_state(int id, int factor) const = 0;
    
    virtual size_t get_number_states(int factor) const = 0;

    virtual bool goal_reachable(int factor) const = 0;

    virtual bool apply_symmetry_permutation(const LeavesPermutation &permutation) = 0;  
    
    virtual void dump() const = 0;
 
    virtual std::unique_ptr<CompliantPathGraph> clone() const = 0;
    
    virtual bool is_empty() const = 0;
       
    static void initialize();
    
    static bool is_leaf_goal_state(const LeafStateID &state);
    
    static void store_is_leaf_goal_state(const LeafState &state);
    
    static int get_leaf_state_id(const std::vector<int> &new_leaf_state, int factor);
    
    static void set_compute_leaf_backwards_graph() {
        compute_leaf_backwards_graph = true;
    }
    
    static void set_precompute_leaf_state_spaces(const std::vector<bool> &precompute) {
        precompute_leaf_state_spaces = precompute;
    }
    
    static bool precompute_leaf_state_space(int factor) {
        return precompute_leaf_state_spaces[factor];
    }
    
    virtual std::unique_ptr<CompliantPathGraph> get_permuted_cpg(const LeavesPermutation &permutation) const = 0;  

    bool operator<(const CompliantPathGraph & cpg2) const;
};


#endif
