#ifndef FACTORING_H
#define FACTORING_H

#include "globals.h"
#include "operator_cost.h"

#include <set>
#include <string>
#include <vector>


// TODO put all global variables related to factoring into this class?!

// TODO think about weaker factoring constraints than those of the CG.
// e.g. in ParcPrinter, there is a perfect inverted fork, that is not present
// in the CG because of a single action that has to (and can only) be executed once
// to "initialize the printer"

enum PROFILE {
    FORK,
    I_FORK,
    X_SHAPE,
    CENTER,
    SOKOBAN,    // domain dependant factoring
    OPENSTACKS, // domain dependant factoring
    TPP_CONSTRAINED, // domain dependant factoring
    INV_TPP_CONSTRAINED, // domain dependant factoring
    INC_ARCS
};

enum SEARCH_TYPE {
    FRA,        // stop if leaf plan not optimal
    ASDA,       // anytime search
    SDA,        // fork-decoupled search
    SAT         // satisficing search
};

enum PRUNING_TYPE {
    BASIC//,
//     COST_FRONTIER,   // TODO integrate Patrick's code
//     COST_FRONTIER_PRECOMPUTE,
//     GOAL_COST_PROPAGATION
};

struct InteractionGraph {
    friend class Factoring;
    
    // to handle the center factor, increase indexes by 1
    
private:
    std::vector<std::vector<int> > successors;
    std::vector<std::vector<int> > predecessors;
    
    void add_dependency(int from, int to);
    
    InteractionGraph(){};
public:
    InteractionGraph(int num_factors) {
        successors.resize(num_factors+1);
        predecessors.resize(num_factors+1);
    }
    
    ~InteractionGraph(){}

    const std::vector<int> &get_successors(int factor) const {
        return successors[factor+1];
    }

    const std::vector<int> &get_predecessors(int factor) const {
        return predecessors[factor+1];
    }
};

class OptionParser;
class Options;
class Pruning;
class SearchSpace;

class Factoring {
    
    mutable bool ignore_current_state;
    
    bool reopen_closed_nodes;
    
    mutable OperatorCost cost_type;
    
    mutable SearchSpace *search_space;
    
    Pruning *pruning;
    
    SEARCH_TYPE search_type;
    
    size_t max_precompute_state_space_size;
    
    mutable std::vector<bool> precompute_leaf_state_space;
    
protected:
    
    typedef std::vector<std::set<int> > FactoredVars;
    
    size_t max_leaf_size;
    
    size_t min_number_leaves;
    
    PROFILE profile;
    
    mutable InteractionGraph interaction_graph;
    
    virtual FactoredVars get_factoring() = 0;
    
    FactoredVars get_sccs(std::vector<int> variables = std::vector<int>()) const;
    
    FactoredVars get_connected_components(std::vector<int> variables) const;
    
    std::vector<bool> get_frozen_leaves(FactoredVars components) const;
    
    void apply_factoring(FactoredVars factoring);
    
    void print_factoring() const;
    
    void exit_error(std::string message, int exit_code) const;
    
    Factoring(const Options &ops);
    
public:
    
    void do_factoring_or_abstain();
    
    PROFILE get_profile() const {
        return profile;
    }
    
    bool has_predecessors(int factor) const {
        return !interaction_graph.get_predecessors(factor).empty();
    }
    
    bool has_successors(int factor) const {
        return !interaction_graph.get_successors(factor).empty();
    }
    
    const InteractionGraph &get_interaction_graph() const {
        return interaction_graph;
    }
    
    void set_cost_type(OperatorCost ct) {
        cost_type = ct;
    }
    
    OperatorCost get_cost_type() const {
        return cost_type;
    }
    
    SEARCH_TYPE get_search_type() const {
        return search_type;
    }
    
    void set_search_type(SEARCH_TYPE type) {
        search_type = type;
    }
    
    void set_ignore_current_state(bool ignore) {
        ignore_current_state = ignore;
    }
    
    bool ignore_current_search_state() const {
        return ignore_current_state;
    }
    
    void set_search_space(SearchSpace *sp) {
        search_space = sp;
    }
    
    SearchSpace* get_search_space() const {
        return search_space;
    }
    
    const Pruning* get_pruning() const {
        return pruning;
    }
    
    bool are_closed_nodes_reopened() const {
        return reopen_closed_nodes;
    }
    
    void set_reopen_closed_nodes() {
        reopen_closed_nodes = true;
    }
    
    /**
     * only precompute leaf state spaces in optimal search or for small leaves
     */
    bool precompute_state_space(int factor) const {
        return precompute_leaf_state_space[factor];
    }
    
    static void add_options_to_parser(OptionParser &parser);
    
};

#endif
 
