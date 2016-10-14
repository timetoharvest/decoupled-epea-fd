#ifndef COST_FRONTIER_PRUNING_H
#define COST_FRONTIER_PRUNING_H

#include "pruning.h"

#include <vector>


class CostFrontierPruning : public Pruning {

public:
    
    CostFrontierPruning(const Options &opts);
    
    virtual DOMINATION check_dominance(const State &center, CompliantPathGraph *check, 
                                       CompliantPathGraph *reference, DOMINATION needed = NONE) const override;
                                       
};

#endif
 
 
