#ifndef COST_FRONTIER_PRUNING_H
#define COST_FRONTIER_PRUNING_H

#include "pruning.h"

#include "../decoupled_data_store.h"
#include "../per_state_information.h"
#include "../ext/boost/dynamic_bitset.hpp"

#include <vector>

class CostFrontierPruning : public Pruning, public DecoupledDataStore {
    
    PerStateInformation<std::vector<boost::dynamic_bitset<> > > is_frontier_state;
    
    std::vector<boost::dynamic_bitset<> > last_checked;
    
    bool registered;
    
    bool get_new_frontier;
    
#ifndef NDEBUG
    StateID last_checked_state;
#endif
    
    CostFrontierPruning();
    
public:
    
    CostFrontierPruning(const Options &opts);
    
    virtual void reset();
    
    virtual DOMINATION check_dominance(const State &center, const PriceTag &check, 
                                       const PriceTag &reference, DOMINATION needed = NONE);
                                       
    virtual void store_cached_results(const State &state);
    
};

#endif
 
 
