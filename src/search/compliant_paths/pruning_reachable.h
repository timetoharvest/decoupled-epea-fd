#ifndef PRUNING_REACHABLE_H
#define PRUNING_REACHABLE_H

#include "reachability_function.h"

#include <vector>

class PruningReachableStorage;

class PruningReachable : public Reachable {
    friend class Reachable; // to get init state cpg
    
    static PruningReachableStorage *cpg_storage;

    
    void compute_frontier();
    
    PruningReachable(const PruningReachable *tag);
    

    static std::unique_ptr<CompliantPathGraph> get_init_state_cpg();

protected:
    
    std::vector<boost::dynamic_bitset<> > frontier;    

    virtual std::unique_ptr<CompliantPathGraph> get_successor_via_center_action(const State &predecessor,
                                                                                const Operator &op) const override;
                                                                                
// no own implementation so far. the only use would be to skip non-frontier states in the update
// virtual void update(const State &base_state) = 0;
                                                                          
    virtual DOMINANCE check_dominance(const State &other, uint64_t hash, int g_advantage, DOMINANCE needed) override;
    
    virtual void store_new_cpg(const State &state) const override;
    
public:
    
    PruningReachable(){};
    
    virtual void dump() const override;

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;
    
    virtual uint64_t get_hash() const override;
    

    static void register_cpg_storage(PruningReachableStorage *storage);
 
};

#endif
