#ifndef PRICE_TAG_H
#define PRICE_TAG_H

#include "globals.h"
#include "leaf_state.h"
#include "leaf_state_id.h"

#include <vector>


class StateRegistry;

const int INF = -1;

class PriceTag {
    friend class StateRegistry;
    
protected:
    
    PriceTag(const PriceTag *tag);
    
    static std::vector<std::vector<StateID> > leaf_price_cache;
    
    // first is id, second is leaf factor
    static std::vector<std::pair<int, int> > leaf_tags_to_cache;
    
    std::vector<size_t> number_facts;
    
    std::vector<std::vector<int> > costs;
    
    std::vector<std::vector<bool> > reachable;
    
    // stores the current minimal goal cost per factor
    std::vector<int> goal_cost;
    
    bool contains_goal(const LeafStateID &fact) const;
    
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
                                  const State &predecessor);
    
    virtual void apply_center_op_to_leaves(const PriceTag &old_tag, const Operator &op);
    
    void cache_leaf_price_tags(const State &state);
    
    void discard_leaf_price_cache();
    
public:
    
    PriceTag(){};
    
    bool has_leaf_state(int id, int factor) const;
    
    int get_cost_of_state(int id, int factor) const;
    
    size_t get_number_states(int factor) const;
    
    int get_goal_cost(int factor) const;
    
    static void initialize_leaf_cache();
    
};



#endif
