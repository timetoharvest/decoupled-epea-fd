#ifndef PRICE_TAG_H
#define PRICE_TAG_H

#include "globals.h"
#include "leaf_state.h"
#include "leaf_state_id.h"

#include <vector>


const int INF = -1;

// TODO create a new struct for satisficing => 0/1
struct PriceTagInfo {
    int cost;
    
    PriceTagInfo() : cost(INF) {}
    
    PriceTagInfo(int cost)
            : cost(cost) {}
        
    void dump() const;    
};


class StateRegistry;

class PriceTag {
    friend class StateRegistry;
    
protected:
    
    std::vector<size_t> number_facts;
    
    std::vector<std::vector<PriceTagInfo> > costs;
    
    // stores the current minimal goal cost per factor
    std::vector<int> goal_cost;
    
    // first is id, second is leaf factor
    mutable std::vector<std::pair<int, int> > leaf_tags_to_cache;
    
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
    virtual bool add_fact(const LeafStateID &fact, int cost);
    
    // returns the new duplicate counter
    virtual int update_price_tags(int curr_number_duplicates,
                                  const State &base_state,
                                  int g_value,
                                  const State &predecessor);
    
    virtual void apply_center_op_to_leaves(const PriceTag &old_tag, const Operator &op);
    
    void cache_leaf_price_tags(const State &state);
    
    void discard_leaf_price_cache();
    
public:
    
    PriceTag();

    PriceTag(const PriceTag &tag, bool copy_facts = true);
    
    bool has_leaf_state(int id, int factor) const;
    
    virtual const PriceTagInfo &get_info_for_fact(int id, int factor) const;
    
    size_t get_number_facts(int factor) const;
    
    int get_goal_cost(int factor) const;
    
};



#endif
