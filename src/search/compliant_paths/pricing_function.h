#ifndef PRICING_FUNCTION_H
#define PRICING_FUNCTION_H

#include "compliant_path_graph.h"
#include "../globals.h"
#include "../leaf_state.h"
#include "../leaf_state_id.h"
#include "../pruning/prunable.h"

#include <vector>


class Prices : public CompliantPathGraph, public Prunable {
    friend class CompliantPathGraph;
    
    static Prices tmp_price;
    
protected:  

    std::vector<std::vector<int> > prices;
    
    std::vector<size_t> number_states;

    // stores the current minimal goal cost per factor
    std::vector<int> goal_cost;
        
    Prices(const Prices *tag);
      
    // used when including part of the leaf cost into the g-value
    // of a search node
    void reduce_cost_by(int factor, int cost);
    
    // returns the duplicate counter for this base state and price tag 
    // i.e. the counter of the subsuming that or the current max + 1
    int check_duplicate(const State &base_state, int curr_number_duplicates, 
                        int g_value, const State &predecessor);
    
    // return true, if the fact was actually added
    // in satisficing planning, return true only if the leaf state was
    // not already reached, before
    bool add_state(const LeafStateID &fact, int cost);
    
    // returns the new duplicate counter
    virtual int update_price_tags(int curr_number_duplicates,
                                  const State &base_state,
                                  int g_value,
                                  const State &predecessor,
                                  const Operator *op) const override;
    
    virtual void apply_center_op_to_leaves(const Prices &old_tag, const Operator &op);
    
public:
    
    Prices(){};
    
    virtual bool has_leaf_state(const LeafStateID &id) const override;
    
    virtual bool has_effective_leaf_state(const LeafStateID &id) const override;
    
    virtual int get_cost_of_state(const LeafStateID &id) const override;
    
    virtual int get_effective_cost_of_state(const LeafStateID &id) const override;
    
    virtual size_t get_number_states(size_t factor) const override;

    virtual size_t get_number_effective_states(size_t factor) const override;
    
    virtual int get_goal_cost(size_t factor) const override;
    
    virtual bool goal_reachable(size_t factor) const override;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier() override;
    
    virtual void dump() const override;

    
    static void create_init_state_prices();

    static void store_new_graph(const State &state);
    
};



#endif
