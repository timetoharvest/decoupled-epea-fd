#ifndef PRUNING_H
#define PRUNING_H

class Options;
class PriceTag;
class State;

enum DOMINATION {
    NONE,
    DOMINATED,
    DOMINATES,
    EQUAL
};

class Pruning {
    
public:
    
    virtual DOMINATION check_dominance(const State &center, const PriceTag &check, 
                                       const PriceTag &reference, DOMINATION needed = NONE) = 0;
    
};

class BasicPruning : public Pruning {
    
public:
    
    BasicPruning() {}
    
    BasicPruning(const Options &opts);
    
    virtual DOMINATION check_dominance(const State &center, const PriceTag &check, 
                                       const PriceTag &reference, DOMINATION needed = NONE);
    
};

#endif
 
