#ifndef FACTORINGS_LP_FACTORINGS_H
#define FACTORINGS_LP_FACTORINGS_H

#include "../factoring.h"

enum STRATEGY {
    MIS,
    MIS_MOBILE,
    MIS_LP_RELAX,
    MML,
    MML_MODULAR,
    MML_MODULAR_NO_SCHEMES,
    MM,
    MF,
    MMF,
};

enum FACTORING_TYPE {
    GENERAL,
    STRICT,
    F,
    IF,
};

struct PotentialLeaf {
    int id;
    int num_actions; // number of actions with the effect schema
    int num_leaf_only_actions; // number of leaf-only actions if the effect schema forms a leaf
    int num_leaf_actions; // number of leaf actions if the effect schema forms a leaf
    std::vector<int> vars; // sorted
    std::set<int> leaf_only_schemes;

    PotentialLeaf(int id, int num_actions, const std::vector<int> &vars)
    : id(id), num_actions(num_actions), num_leaf_only_actions(0), num_leaf_actions(0), vars(vars) {}

    float get_flexibility() const {
        return ((float) num_leaf_only_actions / (float) num_leaf_actions);
    }

};

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

class LPFactoring : public Factoring {

protected:

    STRATEGY strategy;

    FACTORING_TYPE factoring_type;

    int min_mobility;

    float min_flexibility;

    int max_merge_steps;

    int cplex_time_limit;

    std::vector<ActionSchema> act_schemes;

    std::vector<PotentialLeaf> potential_leaves;

    std::vector<std::set<size_t>> var_to_p_leaves;

    FactoredVars get_factoring() override;

    FactoredVars get_mip_factoring();

    FactoredVars get_max_mobile_leaves();

    FactoredVars get_max_mobile_leaves_modular();

    FactoredVars get_max_mobile_leaves_modular_no_schemes();

    std::vector<bool> get_mobile_vars();

    void set_action_schemes(int min_num_actions = 1);

    void set_potential_leaves(int min_mobility = 1, float min_flexibility = 0);

    void merge_potential_leaves(int max_iterations = 2);

public:

    LPFactoring(const Options &opts);

    static void add_options_to_parser(OptionParser &parser);

};


#endif
