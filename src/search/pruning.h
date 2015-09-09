#ifndef PRUNING_H
#define PRUNING_H

class PriceTag;

enum DOMINATION {
    NONE,
    DOMINATED,
    DOMINATES,
    EQUAL
};

class Pruning {
    
public:
    
    virtual DOMINATION check_dominance(const PriceTag &check, const PriceTag &reference, DOMINATION needed = NONE) const = 0;
    
};

class BasicPruning : public Pruning {
    
public:
    
    BasicPruning() {}
    
    virtual DOMINATION check_dominance(const PriceTag& check, const PriceTag& reference, DOMINATION needed = NONE) const;
    
};

#endif
 
