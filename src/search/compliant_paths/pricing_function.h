#ifndef PRICING_FUNCTION_H
#define PRICING_FUNCTION_H

#include "compliant_path_graph.h"
#include "../globals.h"
#include "../leaf_state.h"
#include "../leaf_state_id.h"
#include "../pruning/prunable.h"

#include <vector>

class PriceStorage;

class Prices : public CompliantPathGraph {
    friend class CompliantPathGraph;
    
    // TODO get rid of this in all CPG classes! 
    // might be needed for init state, then make this an exception
    static Prices tmp_price;
    
    static PriceStorage *cpg_storage;
    
    static CompliantPathGraph& get_init_state_prices();
    
protected:  

    std::vector<std::vector<int> > prices;
    
    std::vector<size_t> number_states;

    // stores the current minimal goal cost per factor
    std::vector<int> goal_cost;
        
    Prices(const Prices *tag);
      
    // used when including part of the leaf cost into the g-value
    // of a search node
    void reduce_cost_by(int factor, int cost);

    // return true, if the fact was actually added
    // in satisficing planning, return true only if the leaf state was
    // not already reached, before
    bool add_state(const LeafStateID &fact, int cost);
    
    // returns the new duplicate counter
    virtual CompliantPathGraph& update_price_tags(const State &base_state,
                                        const Operator *op,
                                        const State &predecessor) const;
    
    void apply_center_op_to_leaves(const Prices &old_tag, const Operator &op);
    
public:
    
    Prices(){};
    
    virtual void store_new_graph(const State &state) const;
    
    virtual bool has_leaf_state(int id, int factor) const;
    
    virtual bool has_effective_leaf_state(int id, int factor) const;
    
    virtual int get_cost_of_state(int id, int factor) const;
    
    virtual int get_effective_cost_of_state(int id, int factor) const;
    
    virtual size_t get_number_states(int factor) const;

    virtual size_t get_number_effective_states(int factor) const;

    virtual bool goal_reachable(int factor) const;
    
    virtual int get_goal_cost(int factor) const;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier();
    
    virtual bool is_empty() const override {
        return goal_cost.empty();
    }

    // TODO add the override everywhere
    virtual bool apply_symmetry_permutation(const LeavesPermutation &permutation) override;
    
    virtual void dump() const;

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;

    virtual std::unique_ptr<CompliantPathGraph> get_permuted_cpg(const LeavesPermutation &permutation) const override;  
   
    static void register_cpg_storage(PriceStorage *storage);
    
};



#endif
