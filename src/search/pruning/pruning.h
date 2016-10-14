#ifndef PRUNING_H
#define PRUNING_H

class CompliantPathGraph;
class Options;
class OptionParser;
class State;

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
    
    bool do_simulation;
    
    bool goal_price_propagation;
    
    static bool ignore_current_search_state;
    
    static bool reopen_closed_nodes;
    
public:
    Pruning() : irrelevance(NO), do_simulation (false), goal_price_propagation(false) {}

    Pruning(const Options &opts);

    virtual ~Pruning() = default;
    
    virtual void init();
    
    // TODO make this part of CompliantPathGraph, so that each subclass can have its own specific implementation
    // then, have a PruningOptions class that encapsulates options like frontier/irrelevance and a class for effective price 
    // like simulation and goal price propagation, that is used within PruningPrices/Reachable
    virtual DOMINATION check_dominance(const State &center, CompliantPathGraph *check, CompliantPathGraph *reference, DOMINATION needed = NONE) const = 0;
    
    
    static void set_ignore_current_state() {
        ignore_current_search_state = true;
    }
    
    static void reset_ignore_current_state() {
        ignore_current_search_state = false;
    }
    
    static bool ignore_current_state() {
        return ignore_current_search_state;
    }
    
    static void set_reopen_closed_nodes() {
        reopen_closed_nodes = true;
    }
    
    static bool are_closed_nodes_reopened() {
        return reopen_closed_nodes;
    }
    
    static void add_options_to_parser(OptionParser &parser);
    
};

class BasicPruning : public Pruning {
    
public:
    
    BasicPruning() : Pruning() {}
    
    BasicPruning(const Options &opts);
    
    virtual DOMINATION check_dominance(const State &center, CompliantPathGraph *check, CompliantPathGraph *reference, DOMINATION needed = NONE) const override;
    
};

#endif
 
