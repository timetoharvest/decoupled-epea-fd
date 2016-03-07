#ifndef PRUNING_H
#define PRUNING_H

class CompliantPathGraph;
class Options;

enum DOMINATION {
    NONE,
    DOMINATED,
    DOMINATES,
    EQUAL
};

class Pruning {
    
public:
    
    virtual DOMINATION check_dominance(const CompliantPathGraph *check, const CompliantPathGraph *reference, DOMINATION needed = NONE) const = 0;
    
};

class BasicPruning : public Pruning {
    
public:
    
    BasicPruning() {}
    
    BasicPruning(const Options&);
    
    virtual DOMINATION check_dominance(const CompliantPathGraph *check, const CompliantPathGraph *reference, DOMINATION needed = NONE) const;
    
};

#endif
 
