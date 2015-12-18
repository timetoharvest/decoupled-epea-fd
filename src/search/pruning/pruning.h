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

    bool do_simulation, do_irrelevance;
public:
    Pruning() : do_simulation (false), do_irrelevance(false){}

    Pruning(const Options &opts);

    virtual ~Pruning();
    
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
 
