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


class PruningOptions {
    
    enum PRUNING_TYPE {
        // TODO FRONTIER and DUPLICATE are actually orthogonal and can be combined
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
    
    bool hypercube_pruning;

    // use g_inc_g_by to increase the g-value of decoupled states
    // by the sum of minimum prices
    bool min_price_in_g;

    // incorporate the difference in g-values of decoupled states
    // into the dominance checking
    bool g_in_dominance;

    // compare prices across leaves, so higher prices in one
    // can be traded against lower prices in another leaf
    bool prices_in_dominance;

    IRRELEVANCE irrelevance;

    bool do_simulation;
    
    bool goal_price_propagation;
    
    void verify_options() const;

    
    static bool ignore_current_search_state; // TODO: get rid of this hack

    static bool reopen_closed_nodes;
    
public:
    PruningOptions() : pruning_type(PRUNING_TYPE::DOMINANCE),
                       hypercube_pruning(false),
                       min_price_in_g(true),
                       g_in_dominance(true),
                       prices_in_dominance(true),
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
    
    bool use_hypercube_pruning() const {
        return hypercube_pruning;
    }

    bool include_min_price_in_g() const {
        return min_price_in_g;
    }

    bool include_g_in_dominance() const {
        return g_in_dominance;
    }

    bool trade_prices_in_dominance() const {
        return prices_in_dominance;
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
    
    static PruningOptions* _parse_dominance(OptionParser &parser);
    static PruningOptions* _parse_duplicate(OptionParser &parser);
    static PruningOptions* _parse_frontier(OptionParser &parser);

};

#endif
 
