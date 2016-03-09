#ifndef COST_FRONTIER_PRUNING_H
#define COST_FRONTIER_PRUNING_H

#include "pruning.h"

#include <vector>


class Prices;

class CostFrontierPruning : public Pruning {

public:
    
    CostFrontierPruning(const Options &opts);
    
    virtual DOMINATION check_dominance(const State &center, Prunable *check, 
                                       Prunable *reference, DOMINATION needed = NONE) const;
                                       
};

#endif
 
 
