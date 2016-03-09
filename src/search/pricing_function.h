#ifndef PRICING_FUNCTION_H
#define PRICING_FUNCTION_H

#include "compliant_path_graph.h"
#include "globals.h"
#include "leaf_state.h"
#include "leaf_state_id.h"
#include "pruning/prunable.h"

#include <vector>


class Prices : public CompliantPathGraph, public Prunable {
    friend class CompliantPathGraph;
    
    static Prices tmp_price;
    
protected:  
    
    std::vector<boost::dynamic_bitset<> > frontier;

    std::vector<std::vector<int> > prices;
    
    std::vector<std::vector<int> > effective_prices;
    
    std::vector<size_t> number_states;
    
    std::vector<size_t> number_effective_states;

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
    
    bool add_effective_state(const LeafStateID &fact, int cost);
    
    // returns the new duplicate counter
    virtual int update_price_tags(int curr_number_duplicates,
                                  const State &base_state,
                                  int g_value,
                                  const State &predecessor,
                                  const Operator *op) const;
    
    void apply_center_op_to_leaves(const Prices &old_tag, const Operator &op);

    void compute_cost_frontier();

    void goal_price_propagation();

    void propagate_dominance_prices(); 
    
public:
    
    Prices(){};
    
    virtual bool has_leaf_state(int id, int factor) const;
    
    virtual bool has_effective_leaf_state(int id, int factor) const;
    
    virtual int get_cost_of_state(int id, int factor) const;
    
    virtual int get_effective_cost_of_state(int id, int factor) const;
    
    virtual size_t get_number_states(int factor) const;

    virtual size_t get_number_effective_states(int factor) const;
    
    virtual int get_goal_cost(int factor) const;
    
    virtual bool goal_reachable(int factor) const;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier();
    
    void dump() const;

    
    static void create_init_state_prices();

    static void store_new_graph(const State &state);
    
};



#endif
