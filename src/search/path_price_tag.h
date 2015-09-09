#ifndef PATH_PRICE_TAG_H
#define PATH_PRICE_TAG_H

#include "price_tag.h"


struct PathPriceTagInfo {
    const Operator *best_supporter;
    LeafStateID predecessor;
    bool is_new;
    
    PathPriceTagInfo() : predecessor(LeafStateID::no_state),
                         is_new(false) {}
    
    PathPriceTagInfo(const Operator* bs, LeafStateID predecessor, bool is_new = false)
            : best_supporter(bs), predecessor(predecessor), is_new(is_new) {}
        
    void dump() const;    
};


class PathPriceTag : public PriceTag {
    
    friend class SearchSpace;
    
    // path information
    std::vector<std::vector<PathPriceTagInfo> > paths;
    
    // return true, if the fact was actually added
    // in satisficing planning, return true only if the leaf state was
    // not already reached, before
    virtual bool add_fact(const LeafStateID &fact, int cost, const Operator *bs, 
                  const LeafStateID &predecessor);
    
public:
    
    PathPriceTag();
    
    PathPriceTag(const PathPriceTag& tag, bool copy_facts);
      
    const PathPriceTagInfo &get_path_info_for_fact(int id, int factor) const;
    
    virtual void update_price_tags(const State &base_state);
    
    virtual void apply_center_op_to_leaves(const PriceTag &old_tag, const Operator &op);
    
};

#endif