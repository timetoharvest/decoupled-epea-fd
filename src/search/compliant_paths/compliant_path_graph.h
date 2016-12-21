#ifndef COMPLIANT_PATH_GRAPH_H
#define COMPLIANT_PATH_GRAPH_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "../factoring.h"
#include "../globals.h"
#include "../operator.h"
#include "../operator_cost.h"
#include "../pruning/pruning.h"
#include "../pruning/simulation_relation.h"

#include <limits>
#include <vector>


class LeafState;
class LeafStateID;
class SearchSpace;
class State;
class StateID;

namespace stubborn_sets_decoupled {
class StubbornSetsDecoupled;
}

class CompliantPathGraph {
    friend class stubborn_sets_decoupled::StubbornSetsDecoupled; // access to leaf_state_pre/successors
    friend class SimulationRelation; // access to leaf_state_pre/successors
    friend class StateRegistry; // build leaf state spaces + init state cpg
    friend class SuccessorGeneratorSwitch; // access to center_successors
    
    /*
      this builds all entire leaf state spaces and stores them
    */
    static void build_leaf_state_spaces();
    
    static std::unique_ptr<CompliantPathGraph> get_init_state_cpg();
    
protected:
    
    static SearchSpace *search_space;
    
    static OperatorCost cost_type;
    
    static std::unique_ptr<Pruning> pruning;
    
    static const size_t MAX_LEAF_SPACE_STORE;
    
    static bool compute_leaf_backwards_graph;
    
    static std::vector<bool> precompute_leaf_state_spaces;
    
    static std::vector<std::vector<StateID> > leaf_price_cache;
    
    // first is id, second is leaf factor
    static std::vector<LeafStateID> leaf_tags_to_cache;
    
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
    
    // TODO refactor this into two separate functions, one that creates the succ CPG, and calls
    // a new update_cpg(..) function that does the actual update
    // returns the new duplicate counter
    virtual std::unique_ptr<CompliantPathGraph> get_updated_successor_cpg(const State &base_state,
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

    virtual bool has_leaf_state(const LeafStateID &id) const = 0;
    
    virtual bool has_effective_leaf_state(const LeafStateID &id) const = 0;
    
    virtual int get_cost_of_state(const LeafStateID &id) const = 0;
    
    virtual int get_effective_cost_of_state(const LeafStateID &id) const = 0;
    
    virtual size_t get_number_states(size_t factor) const = 0;
    
    virtual size_t get_number_effective_states(size_t factor) const = 0;

    virtual bool goal_reachable(size_t factor) const = 0;
    
    virtual int get_goal_cost(size_t factor) const = 0;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier() = 0;
    
    virtual void dump() const = 0;

    virtual std::unique_ptr<CompliantPathGraph> clone() const = 0;
    
    // returns the duplicate counter for this base state and compliant path graph 
    // i.e. the counter of the state subsuming that or the current max + 1
    virtual int check_dominance(const State &base_state,
                                int curr_number_duplicates,
                                const State &predecessor,
                                const Operator &op);
    

    static void initialize();
    
    static const Pruning& get_pruning() {
        return *pruning;
    }
    
    static void set_pruning(Pruning *p) {
        pruning.reset(p);
    }
    
    static void set_search_space(SearchSpace *sp) {
        search_space = sp;
    }
    
    static void set_cost_type(OperatorCost ct) {
        cost_type = ct;
        g_min_action_cost_per_factor.resize(g_factors.size(), std::numeric_limits<int>::max());
        for (const auto &op : g_operators){
            int factor = op.get_affected_factor();
            if (factor != -1){
                g_min_action_cost_per_factor[factor] = std::min(g_min_action_cost_per_factor[factor], get_adjusted_action_cost(op, cost_type));
            }
        }
    }
    
    static OperatorCost get_cost_type() {
        return cost_type;
    }
    
    static bool is_leaf_goal_state(const LeafStateID &state);
    
    static void store_is_leaf_goal_state(const LeafState &state);
    
    static void set_compute_leaf_backwards_graph() {
        compute_leaf_backwards_graph = true;
    }
    
    static void set_precompute_leaf_state_spaces(const std::vector<bool> &precompute) {
        precompute_leaf_state_spaces = precompute;
    }
    
    static bool precompute_leaf_state_space(size_t factor) {
        return precompute_leaf_state_spaces[factor];
    }
    
};

#endif
