#ifndef PRICING_FUNCTION_H
#define PRICING_FUNCTION_H

#include "explicit_state_cpg.h"
#include "../leaf_state_id.h"

#include <vector>

template<class T>
class Storage;

class Prices : public ExplicitStateCPG {
    friend class ExplicitStateCPG;
    
    static Storage<Prices> *cpg_storage;
    

    // used when including part of the leaf cost into the g-value
    // of a search node
    void reduce_cost_by(LeafFactorID factor, int cost);
    
    void apply_center_op_to_leaves(const Prices &old_cpg, const Operator &op);


    static std::unique_ptr<CompliantPathGraph> get_init_state_cpg();

protected:  

    std::vector<std::vector<int> > prices;
    
    std::vector<size_t> number_states;

    // stores the current minimal goal cost per factor
    std::vector<int> goal_cost;

    void resize();

    // return true, if the fact was actually added
    bool add_state(LeafStateHash id, LeafFactorID factor, int cost);
    
    void update(const State &new_center_state);
    
    virtual std::unique_ptr<CompliantPathGraph> get_successor_via_center_action(const State &new_center_state,
                                                                                const Operator &op) const override;
                                                                          
    virtual DOMINANCE check_dominance(const State &other, uint64_t hash, int g_advantage, DOMINANCE needed) override;
    
    virtual void store_new_cpg(const State &state) override;
    
    virtual bool is_hypercube_covered(const State &base_state, int curr_number_duplicates) const override;

public:
    
    Prices(){};
    
    Prices(const Prices &tag);

    Prices(Prices &&other) = delete;

    Prices& operator=(const Prices &other) = delete;

    Prices& operator=(Prices &&other);


    virtual bool has_leaf_state(LeafStateHash id, LeafFactorID factor) const override;
    
    virtual int get_cost_of_state(LeafStateHash id, LeafFactorID factor) const override;
    
    virtual size_t get_number_states(LeafFactorID factor) const override;
    
    virtual void populate_reached_leaf_facts(std::function<void(int, int)> f) const override;

    virtual void populate_cost_of_leaf_facts(std::function<void(int, int, int)> f) const override;

    virtual int get_goal_cost(LeafFactorID factor) const override;
    
    virtual bool goal_reachable(LeafFactorID factor) const override;
    
    virtual void dump() const override;

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;

    virtual uint64_t get_hash() const override;

    virtual void apply_symmetry_permutation(const LeavesPermutation &permutation) override;

    virtual size_t get_memory_footprint() const override;


    static void register_cpg_storage(Storage<Prices> *storage);
    
};



#endif
