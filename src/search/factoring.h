#ifndef FACTORING_H
#define FACTORING_H

#include "globals.h"
#include "leaf_state_id.h"
#include "utilities.h"

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
    STAR,
    SOKOBAN,    // domain dependant factoring
    OPENSTACKS, // domain dependant factoring
    TPP_CONSTRAINED, // domain dependant factoring
    INV_TPP_CONSTRAINED, // domain dependant factoring
    DOC_TRANSFER, // domain dependant factoring
    PEGSOL // domain dependant factoring
};

enum SEARCH_TYPE { // TODO rename this + add a type that starts standard search if no factoring was found
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
    std::vector<std::vector<LeafFactorID> > successors;
    std::vector<std::vector<LeafFactorID> > predecessors;
    
    void add_dependency(LeafFactorID from, LeafFactorID to);
    
    InteractionGraph(){};
public:
    InteractionGraph(size_t num_factors) {
        successors.resize(num_factors+1);
        predecessors.resize(num_factors+1);
    }
    
    ~InteractionGraph(){}

    const std::vector<LeafFactorID> &get_successors(LeafFactorID factor) const {
        if (factor == LeafFactorID::CENTER){
            return successors[0];
        }
        return successors[factor+1];
    }

    const std::vector<LeafFactorID> &get_predecessors(LeafFactorID factor) const {
        if (factor == LeafFactorID::CENTER){
            return predecessors[0];
        }
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
    
    std::vector<bool> fork_leaf;
    
    std::vector<bool> ifork_leaf;


    static std::vector<std::set<int>> var_to_affecting_op;

    
    virtual FactoredVars get_factoring() = 0;
    
    FactoredVars get_sccs(std::vector<int> variables = std::vector<int>()) const;
    
    FactoredVars get_connected_components(const std::vector<int> &variables) const;
    
    std::vector<bool> get_frozen_leaves(const FactoredVars &components) const;
    
    void apply_factoring(const FactoredVars &factoring);
    
    void print_factoring() const;
    
    void exit_error(std::string message, ExitCode exit_code) const;
    
    Factoring(const Options &ops);


    static void init_var_to_ops_map();
    
public:
    
    virtual ~Factoring() = default;
    
    void do_factoring_or_abstain();
    
    PROFILE get_profile() const {
        return profile;
    }
    
    bool is_fork_leaf(LeafFactorID factor) const {
        return fork_leaf[factor];
    }
    
    bool is_ifork_leaf(LeafFactorID factor) const {
        return ifork_leaf[factor];
    }
    
    SEARCH_TYPE get_search_type() const {
        return search_type;
    }
    

    static void add_options_to_parser(OptionParser &parser);
    
};

#endif
 
