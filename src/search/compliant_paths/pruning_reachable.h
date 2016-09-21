#ifndef PRUNING_REACHABLE_H
#define PRUNING_REACHABLE_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "../globals.h"
#include "../leaf_state.h"
#include "../leaf_state_id.h"
#include "../pruning/prunable.h"
#include "reachability_function.h"

#include <vector>

class PruningReachableStorage;

class PruningReachable : public Reachable {
    friend class Reachable; // to get init state cpg
    
    static PruningReachable tmp_p_reach;
    
    static PruningReachableStorage *cpg_storage;
    
    static CompliantPathGraph& get_init_state_reachable();
    
    
    void compute_frontier();
    
protected:
    
    std::vector<boost::dynamic_bitset<> > frontier;
    
    
    PruningReachable(const PruningReachable *tag);
    
    // returns the duplicate counter for this base state and price tag 
    // i.e. the counter of the subsuming that or the current max + 1
    int check_duplicate(const State &base_state, int curr_number_duplicates, 
                        int g_value, const State &predecessor);

    // returns the new duplicate counter
    virtual CompliantPathGraph& update_price_tags(const State &base_state,
                                        const Operator *op,
                                        const State &predecessor) const;
    
public:
    
    PruningReachable(){};

    virtual void store_new_graph(const State &state) const;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier();
    
    virtual void dump() const;
    
    virtual std::unique_ptr<CompliantPathGraph> clone() const override;  

    virtual std::unique_ptr<Prunable> get_permuted_cpg(const LeavesPermutation &permutation) const override;  

    
    static void register_cpg_storage(PruningReachableStorage *storage);
 
};



#endif
