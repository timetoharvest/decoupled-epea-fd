#ifndef FACTORING_H
#define FACTORING_H

#include "globals.h"
#include "leaf_state_id.h"
#include "utils/countdown_timer.h"

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
    STAR,
};

enum DECOUPLED_SEARCH_TYPE {
    FRA,        // optimal fork-decoupled search; stop if leaf plan not optimal
    ASDA,       // optimal anytime search with center heuristic
    SDA,        // optimal decoupled search with star heuristic
    SAT,        // satisficing search
    UNSAT       // for proving unsolvability (SAT + optimizations)
};

enum LEAF_REPRESENTATION_TYPE {
    EXPLICIT,
    SYMBOLIC
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
    
    enum ABSTAIN_TYPE {
        ABSTAIN_ON_FAILURE, // default, if no factoring is found, the planner stops
        STANDARD_ON_FAILURE, // run standard search if no factoring is found
        NO_SEARCH   // stop after doing the factoring
    };

    DECOUPLED_SEARCH_TYPE search_type;

    ABSTAIN_TYPE abstain_type;
    
    LEAF_REPRESENTATION_TYPE leaf_type;

    size_t max_precompute_state_space_size;
    
    int factoring_time_limit;

    std::vector<bool> fork_leaf;

    std::vector<bool> ifork_leaf;

    void apply_factoring(const std::vector<std::set<int> > &factoring);

    void print_factoring_statistics() const;

    bool is_factoring_possible() const;

    bool is_two_leaf_factoring_possible() const;

protected:
    
    struct ActionSchema {

        int num_actions; // number of actions with the action schema
        std::vector<int> pre_vars; // sorted
        std::vector<int> eff_vars; // sorted

        ActionSchema(int num_actions, const std::vector<int> &pre_vars, const std::vector<int> &eff_vars)
        : num_actions(num_actions), pre_vars(pre_vars), eff_vars(eff_vars) {
        }

        void incr_num_action() {
            num_actions++;
        }

    };

    using FactoredVars = std::vector<std::set<int> >;

    static std::vector<std::set<int> > var_to_affecting_op;

    static std::vector<ActionSchema> action_schemas;

    static void init_var_to_ops_map();

    static void init_action_schemas();


    size_t max_leaf_size;
    
    size_t min_number_leaves;
    
    // TODO so-far, this is only considered by LP-based, incident arcs, and MIS factorings; all other factorings ignore the limit
    utils::CountdownTimer factoring_timer;

    FACTORING_PROFILE profile;

    virtual FactoredVars get_factoring() = 0;
    
    FactoredVars get_sccs(const std::vector<int> &variables = std::vector<int>()) const;
    
    FactoredVars get_connected_components(const std::vector<int> &variables) const;
    
    std::vector<bool> get_frozen_leaves(const FactoredVars &components) const;
    

    Factoring(const Options &opts);
    
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

    LEAF_REPRESENTATION_TYPE get_leaf_representation_type() const {
        return leaf_type;
    }

    bool run_standard_search_because_factoring_failed() const {
        // TODO hacky, fix this
        return g_leaves.empty() && abstain_type == ABSTAIN_TYPE::STANDARD_ON_FAILURE;
    }

    
    static const FactoredVars& get_var_to_affecting_op() {
        init_var_to_ops_map();
        return var_to_affecting_op;
    }

    static void add_options_to_parser(OptionParser &parser);
    
};

#endif
 
