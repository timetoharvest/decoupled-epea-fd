#ifndef COST_FRONTIER_PRUNING_H
#define COST_FRONTIER_PRUNING_H

#include "pruning.h"

#include <vector>

class CostFrontierPruning : public Pruning {

    const bool compare_goal_prices;
public:
    
    CostFrontierPruning(const Options &opts);
    
    virtual DOMINATION check_dominance(const State &center, PriceTag &check, 
                                        PriceTag &reference, DOMINATION needed = NONE);
                                       
};

#endif
 
 
