#ifndef PRUNING_REACHABLE_H
#define PRUNING_REACHABLE_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "../globals.h"
#include "../leaf_state_id.h"
#include "reachability_function.h"

#include <vector>

class PruningReachableStorage;

class PruningReachable : public Reachable {
    friend class Reachable; // to get init state cpg
    
    static PruningReachableStorage *cpg_storage;
    
    static std::unique_ptr<CompliantPathGraph> get_init_state_reachable();   
    
    void compute_frontier();
    
protected:
    
    std::vector<boost::dynamic_bitset<> > frontier;
    
    
    PruningReachable(const PruningReachable *tag);
    
    // returns the duplicate counter for this base state and price tag 
    // i.e. the counter of the subsuming that or the current max + 1
    int check_duplicate(const State &base_state, int curr_number_duplicates, 
                        int g_value, const State &predecessor);

    // returns the new duplicate counter
    virtual std::unique_ptr<CompliantPathGraph> get_updated_successor_cpg(const State &base_state,
                                                                          const Operator *op,
                                                                          const State &predecessor) const override;
    
public:
    
    PruningReachable(){};

    virtual void store_new_graph(const State &state) const;

    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier() override;
    
    virtual void dump() const override;

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;
    
    
    static void register_cpg_storage(PruningReachableStorage *storage);
 
};



#endif
