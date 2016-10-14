#ifndef COST_FRONTIER_PRUNING_H
#define COST_FRONTIER_PRUNING_H

#include "pruning.h"


class CostFrontierPruning : public Pruning {

public:
    
    CostFrontierPruning(const Options &opts);
    
    virtual bool use_frontier() const override {
        return true;
    }
    
    virtual bool do_advanced_pruning() const  override {
        return true;
    }
    
    virtual DOMINATION check_dominance(CompliantPathGraph *check, 
                                       CompliantPathGraph *reference,
                                       DOMINATION needed = NONE) const override;
                                       
};

#endif
 
 
