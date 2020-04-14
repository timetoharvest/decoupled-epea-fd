#ifndef FACTORING_H
#define FACTORING_H

#include "globals.h"
#include "leaf_state_id.h"
#include "utils/system.h"

#include <set>
#include <string>
#include <vector>


// TODO put all global variables related to factoring into this class?!

// TODO think about weaker factoring constraints than those of the CG.
// e.g. in ParcPrinter, there is a perfect inverted fork, that is not present
// in the CG because of a single action that has to (and can only) be executed once
// to "initialize the printer"

enum FACTORING_PROFILE {
    FORK,
    IFORK,
    XSHAPE,
    STAR,
    SOKOBAN,    // domain dependent factoring
    OPENSTACKS, // domain dependent factoring
    TPP_CONSTRAINED, // domain dependent factoring
    INV_TPP_CONSTRAINED, // domain dependent factoring
    DOC_TRANSFER, // domain dependent factoring
    PEGSOL, // domain dependent factoring
};

enum DECOUPLED_SEARCH_TYPE { // TODO rename this + add a type that starts standard search if no factoring was found
    FRA,        // optimal fork-decoupled search; stop if leaf plan not optimal
    ASDA,       // optimal anytime search with center heuristic
    SDA,        // optimal decoupled search with star heuristic
    SAT,        // satisficing search
    UNSAT,      // for proving unsolvability (SAT + optimizations)
    NO_SEARCH   // stop after doing the factoring
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
    
    DECOUPLED_SEARCH_TYPE search_type;
    
    size_t max_precompute_state_space_size;
    
protected:
    
    typedef std::vector<std::set<int> > FactoredVars;
    
    static FactoredVars var_to_affecting_op;

    static void init_var_to_ops_map();


    size_t max_leaf_size;
    
    size_t min_number_leaves;
    
    FACTORING_PROFILE profile;
    
    std::vector<bool> fork_leaf;
    
    std::vector<bool> ifork_leaf;
    
    virtual FactoredVars get_factoring() = 0;
    
    FactoredVars get_sccs(const std::vector<int> &variables = std::vector<int>()) const;
    
    FactoredVars get_connected_components(const std::vector<int> &variables) const;
    
    std::vector<bool> get_frozen_leaves(const FactoredVars &components) const;
    
    void apply_factoring(const FactoredVars &factoring);
    
    void print_factoring() const;
    
    Factoring(const Options &ops);
    
public:
    
    virtual ~Factoring() = default;
    
    void do_factoring_or_abstain();
    
    FACTORING_PROFILE get_profile() const {
        return profile;
    }
    
    bool is_fork_leaf(LeafFactorID factor) const {
        return fork_leaf[factor];
    }
    
    bool is_ifork_leaf(LeafFactorID factor) const {
        return ifork_leaf[factor];
    }
    
    DECOUPLED_SEARCH_TYPE get_search_type() const {
        return search_type;
    }

    
    static const FactoredVars& get_var_to_affecting_op() {
        init_var_to_ops_map();
        return var_to_affecting_op;
    }

    static void add_options_to_parser(OptionParser &parser);
    
};

#endif
 
