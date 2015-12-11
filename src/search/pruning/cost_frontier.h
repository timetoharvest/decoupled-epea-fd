#ifndef COST_FRONTIER_PRUNING_H
#define COST_FRONTIER_PRUNING_H

#include "pruning.h"

#include "../per_state_information.h"
#include "../ext/boost/dynamic_bitset.hpp"

#include <vector>

class CostFrontierPruning : public Pruning {
    
    PerStateInformation<std::vector<boost::dynamic_bitset<> > > is_frontier_state;
    
    boost::dynamic_bitset<> last_checked;
    
public:
    
    CostFrontierPruning() {}
    
    CostFrontierPruning(const Options &opts);
    
    virtual DOMINATION check_dominance(const State &center, const PriceTag &check, 
                                       const PriceTag &reference, DOMINATION needed = NONE) const;
    
};

#endif
 
 
