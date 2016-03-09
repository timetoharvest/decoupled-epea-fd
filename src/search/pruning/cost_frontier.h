#ifndef COST_FRONTIER_PRUNING_H
#define COST_FRONTIER_PRUNING_H

#include "pruning.h"

#include <vector>


class Prices;

class CostFrontierPruning : public Pruning {

public:
    
    CostFrontierPruning(const Options &opts);
    
    virtual DOMINATION check_dominance(const State &center, const CompliantPathGraph *check, 
                                       const CompliantPathGraph *reference, DOMINATION needed = NONE) const;
    
    DOMINATION check_dominance(const State &center, Prices *check, 
                               Prices *reference, DOMINATION needed = NONE);
                                       
};

#endif
 
 
