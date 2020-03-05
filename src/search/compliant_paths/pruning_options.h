#ifndef PRUNING_OPTIONS_H
#define PRUNING_OPTIONS_H

class Options;
class OptionParser;

enum DOMINANCE {
    NONE,
    DOMINATED,
    DOMINATES,
    EQUAL
};

class PruningOptions;

static PruningOptions* _parse_dominance(OptionParser &parser);
static PruningOptions* _parse_duplicate(OptionParser &parser);
static PruningOptions* _parse_frontier(OptionParser &parser);

class PruningOptions {
    friend PruningOptions* _parse_dominance(OptionParser &parser); // to use PRUNING_TYPE
    friend PruningOptions* _parse_duplicate(OptionParser &parser);
    friend PruningOptions* _parse_frontier(OptionParser &parser);
    
    enum PRUNING_TYPE {
        DOMINANCE,
        DUPLICATE,
        FRONTIER
    };

    enum IRRELEVANCE {
        NO,
        STATES,
        TRANSITIONS
    };

    PRUNING_TYPE pruning_type;
    
    IRRELEVANCE irrelevance;

    bool do_simulation;
    
    bool goal_price_propagation;
    
    void verify_options() const;

    
    static bool ignore_current_search_state; // TODO: get rid of this hack
    
    static bool reopen_closed_nodes;
    
public:
    PruningOptions() : pruning_type(PRUNING_TYPE::DOMINANCE),
                       irrelevance(IRRELEVANCE::NO),
                       do_simulation(false), 
                       goal_price_propagation(false) {}

    PruningOptions(const Options &opts);

    ~PruningOptions() = default;
    
    void initialize() const;
    
    bool use_dominance_pruning() const {
        return pruning_type == PRUNING_TYPE::DOMINANCE;
    }

    bool use_exact_duplicate_checking() const {
        return pruning_type == PRUNING_TYPE::DUPLICATE;
    }

    bool use_frontier_pruning() const {
        return pruning_type == PRUNING_TYPE::FRONTIER;
    }
    
    bool do_advanced_pruning() const {
        return do_simulation || goal_price_propagation || pruning_type == PRUNING_TYPE::FRONTIER;
    }
    
    bool propagate_goal_prices() const {
        return goal_price_propagation;
    }
    
    bool propagate_simulation_prices() const {
        return do_simulation;
    }

    
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

#endif
 
