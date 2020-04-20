#ifndef PATH_PRICE_TAG_H
#define PATH_PRICE_TAG_H

#include "pricing_function.h"


struct PathPriceInfo {
    OperatorID generating_op;
    LeafStateHash predecessor;
    bool is_new;
    
    PathPriceInfo() : generating_op(OperatorID::no_operator),
                      predecessor(LeafStateHash::MAX),
                      is_new(false) {}
    
    PathPriceInfo(OperatorID op, LeafStateHash predecessor, bool is_new = false)
            : generating_op(op), predecessor(predecessor), is_new(is_new) {}
            
    void reset_generating_op() {
        is_new = false;
        generating_op = OperatorID::no_operator;
    }
    
    void dump() const;    
};


class PathPriceTag : public Prices { // TODO rename to PathPrices?
    friend class SearchSpace;
    
    // path information
    std::vector<std::vector<PathPriceInfo> > paths;
    
    // return true, if the fact was actually added
    // in satisficing planning, return true only if the leaf state was
    // not already reached, before
    bool add_state(LeafStateHash id, LeafFactorID factor, int cost,
                   OperatorID generating_op, LeafStateHash predecessor);
    
    PathPriceTag();

    PathPriceTag(const PathPriceTag &cpg);

    PathPriceTag(const CompliantPathGraph &cpg);

    const PathPriceInfo &get_path_info(LeafStateHash id, LeafFactorID factor) const;
    
    void update(const State &new_center_state);
    
    void apply_center_op_to_leaves(const PathPriceTag &old_cpg, const Operator &op);

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;

    virtual bool apply_symmetry_permutation(const LeavesPermutation &permutation) override;

};

#endif
