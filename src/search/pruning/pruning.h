#ifndef PRUNING_H
#define PRUNING_H

class Options;
class PriceTag;
class State;
class OptionParser;

enum DOMINATION {
    NONE,
    DOMINATED,
    DOMINATES,
    EQUAL
};

class Pruning {
    
    enum IRRELEVANCE {
        NO,
        STATES,
        TRANSITIONS
    };
    
    IRRELEVANCE irrelevance;
    
protected:    
    bool do_simulation;
public:
    Pruning() : irrelevance(NO), do_simulation (false) {}

    Pruning(const Options &opts);

    virtual ~Pruning() = default;
    
    virtual void init(); 

    virtual DOMINATION check_dominance(const State &center, PriceTag &check, 
                                       PriceTag &reference, DOMINATION needed = NONE) = 0;

    static void add_options_to_parser(OptionParser &parser);
    
};

class BasicPruning : public Pruning {
    
public:
   
    BasicPruning() : Pruning() {} 

    BasicPruning(const Options &opts);
      
    virtual DOMINATION check_dominance(const State &center, PriceTag &check, 
                                       PriceTag &reference, DOMINATION needed = NONE);
    
};

#endif
 
