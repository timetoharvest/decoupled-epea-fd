#ifndef REACHABILITY_FUNCTION_H
#define REACHABILITY_FUNCTION_H

#include "compliant_path_graph.h"
#include "../ext/boost/dynamic_bitset.hpp"
#include "../globals.h"
#include "../leaf_state.h"
#include "../leaf_state_id.h"
#include "../pruning/prunable.h"

#include <vector>

class ReachableStorage;

class Reachable : public CompliantPathGraph {
    
    static Reachable tmp_reach;
    
    static ReachableStorage *cpg_storage;
    
protected:
    
    std::vector<boost::dynamic_bitset<> > reachable;
    
    boost::dynamic_bitset<> goal_reached;
        
    Reachable(const Reachable *tag);
    
    // return true, if the fact was actually added
    // in satisficing planning, return true only if the leaf state was
    // not already reached, before
    bool add_state(const LeafStateID &fact);
    
    // returns the new duplicate counter
    virtual Prunable& update_price_tags(const State &base_state,
                                        const Operator *op,
                                        const State &predecessor) const;
    
    void apply_center_op_to_leaves(const Reachable &old_tag, const Operator &op);
    
public:
    
    Reachable(){};
    
    virtual void store_new_graph(const State &state) const;
    
    virtual bool has_leaf_state(int id, int factor) const;
    
    virtual bool has_effective_leaf_state(int id, int factor) const;
    
    virtual int get_cost_of_state(int id, int factor) const;
    
    virtual int get_effective_cost_of_state(int id, int factor) const;
    
    virtual size_t get_number_states(int factor) const;

    virtual size_t get_number_effective_states(int factor) const;
    
    virtual int get_goal_cost(int factor) const;
    
    virtual bool goal_reachable(int factor) const;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier();
    
    virtual void dump() const;
    
    
    static void register_cpg_storage(ReachableStorage *storage);
    
    static void create_init_state_reachable(const State &init_state);
 
};



#endif
