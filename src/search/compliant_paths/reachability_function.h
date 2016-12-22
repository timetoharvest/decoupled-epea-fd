#ifndef REACHABILITY_FUNCTION_H
#define REACHABILITY_FUNCTION_H

#include "compliant_path_graph.h"

#include <vector>

class ReachableStorage;

class Reachable : public CompliantPathGraph {
    friend class CompliantPathGraph; // to get init state cpg

    static ReachableStorage *cpg_storage;
    
    static std::unique_ptr<CompliantPathGraph> get_init_state_reachable();
    
    void handle_new_leaf_state_from_center_op(LeafStateHash id, LeafFactorID factor);
    
protected:
    
    std::vector<boost::dynamic_bitset<> > reachable;
    
    boost::dynamic_bitset<> goal_reached;
    
        
    Reachable(const Reachable *tag);
    
    
    // return true, if the fact was actually added
    // in satisficing planning, return true only if the leaf state was
    // not already reached, before
    bool add_state(LeafStateHash id, LeafFactorID factor);
    
    // returns the new duplicate counter
    virtual std::unique_ptr<CompliantPathGraph> get_updated_successor_cpg(const State &base_state,
                                                                          const Operator *op,
                                                                          const State &predecessor) const override;
    
    void apply_center_op_to_leaves(const Reachable &old_tag, const Operator &op);
    
public:
    
    Reachable(){};

    virtual void store_new_graph(const State &state) const override;
    
    virtual bool has_leaf_state(LeafStateHash id, LeafFactorID factor) const override;
    
    virtual bool has_effective_leaf_state(LeafStateHash id, LeafFactorID factor) const override;
    
    virtual int get_cost_of_state(LeafStateHash id, LeafFactorID factor) const override;
    
    virtual int get_effective_cost_of_state(LeafStateHash id, LeafFactorID factor) const override;
    
    virtual size_t get_number_states(LeafFactorID factor) const override;

    virtual size_t get_number_effective_states(LeafFactorID factor) const override;
    
    virtual int get_goal_cost(LeafFactorID factor) const override;
    
    virtual bool goal_reachable(LeafFactorID factor) const override;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier() override;
    
    virtual void dump() const override;

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;

    
    static void register_cpg_storage(ReachableStorage *storage);
 
};



#endif
