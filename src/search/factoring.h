#ifndef FACTORING_H
#define FACTORING_H

#include "globals.h"
#include "utilities.h"
#include "mis.h"

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
    DOC_TRANSFER, // domain dependant factoring
    PEGSOL, // domain dependant factoring
    INC_ARCS,
    MAX_IND_SET
};

enum SEARCH_TYPE {
    FRA,        // stop if leaf plan not optimal
    ASDA,       // anytime search
    SDA,        // fork-decoupled search
    SAT,        // satisficing search
    UNSAT       // for proving unsolvability (SAT + optimizations)
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

class Factoring {
    
    SEARCH_TYPE search_type;
    
    size_t max_precompute_state_space_size;
    
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
    
    void exit_error(std::string message, ExitCode exit_code) const;
    
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
    
    SEARCH_TYPE get_search_type() const {
        return search_type;
    }
    

    static void add_options_to_parser(OptionParser &parser);
    
};

#endif
 
