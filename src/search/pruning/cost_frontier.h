#ifndef COST_FRONTIER_PRUNING_H
#define COST_FRONTIER_PRUNING_H

#include "pruning.h"


class CostFrontierPruning : public Pruning {
    
public:
    
    CostFrontierPruning() {}
    
    CostFrontierPruning(const Options&);
    
    virtual DOMINATION check_dominance(const PriceTag& check, const PriceTag& reference, DOMINATION needed = NONE) const;
    
};

#endif
 
 
