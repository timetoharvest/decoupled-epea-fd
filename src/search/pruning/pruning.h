#ifndef PRUNING_H
#define PRUNING_H

class Options;
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
    
    BasicPruning(const Options&);
    
    virtual DOMINATION check_dominance(const PriceTag& check, const PriceTag& reference, DOMINATION needed = NONE) const;
    
};

#endif
 
