#ifndef REACHABILITY_FUNCTION_H
#define REACHABILITY_FUNCTION_H

#include "compliant_path_graph.h"
#include "ext/boost/dynamic_bitset.hpp"
#include "globals.h"
#include "leaf_state.h"
#include "leaf_state_id.h"
#include "pruning/prunable.h"

#include <vector>

class Reachable : public CompliantPathGraph, public Prunable {
    
    static Reachable tmp_reach;
    
protected:
    
    std::vector<boost::dynamic_bitset<> > reachable;
    
    boost::dynamic_bitset<> goal_reached;
        
    Reachable(const Reachable *tag);
    
    // returns the duplicate counter for this base state and price tag 
    // i.e. the counter of the subsuming that or the current max + 1
    int check_duplicate(const State &base_state, int curr_number_duplicates, 
                        int g_value, const State &predecessor);
    
    // return true, if the fact was actually added
    // in satisficing planning, return true only if the leaf state was
    // not already reached, before
    bool add_state(const LeafStateID &fact);
    
    // returns the new duplicate counter
    virtual int update_price_tags(int curr_number_duplicates,
                                  const State &base_state,
                                  int g_value,
                                  const State &predecessor,
                                  const Operator *op) const;
    
    void apply_center_op_to_leaves(const Reachable &old_tag, const Operator &op);
    
public:
    
    Reachable(){};
    
    virtual bool has_leaf_state(int id, int factor) const;
    
    virtual bool has_effective_leaf_state(int id, int factor) const;
    
    virtual int get_cost_of_state(int id, int factor) const;
    
    virtual int get_effective_cost_of_state(int id, int factor) const;
    
    virtual size_t get_number_states(int factor) const;

    virtual size_t get_number_effective_states(int factor) const;
    
    virtual int get_goal_cost(int factor) const;
    
    virtual bool goal_reachable(int factor) const;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier();
    
    
    static void store_new_graph(const State &state);
    
    static void create_init_state_reachable();
 
};



#endif
