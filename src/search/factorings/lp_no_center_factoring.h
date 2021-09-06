#ifndef FACTORINGS_LP_NO_CENTER_FACTORINGS_H
#define FACTORINGS_LP_NO_CENTER_FACTORINGS_H

#include "lp_factoring.h"


namespace lp_no_center_factoring {
class LPNoCenterFactoring : public lp_factoring::LPFactoring {

    enum STRATEGY {
        MML,
        MM,
        MF,
        MMF,
    };

    enum FACTORING_TYPE {
        STAR,
        F,
        IF,
    };

    struct PotentialLeaf {
        size_t id;
        int num_actions; // number of actions with the effect schema
        int num_leaf_only_actions; // number of leaf-only actions if the effect schema forms a leaf
        int num_leaf_actions; // number of leaf actions if the effect schema forms a leaf
        std::vector<int> vars; // sorted
        std::set<size_t> leaf_only_schemes;

        PotentialLeaf(size_t id, int num_actions, const std::vector<int> &vars)
        : id(id), num_actions(num_actions), num_leaf_only_actions(0), num_leaf_actions(0), vars(vars) {}

        float get_flexibility() const {
            return ((float) num_leaf_only_actions / (float) num_leaf_actions);
        }

    };

    struct ActionSchema {

        int num_actions; // number of actions with the action schema
        std::vector<int> vars; // sorted

        ActionSchema(int num_actions, const std::vector<int> &vars)
            : num_actions(num_actions), vars(vars) {
        }

        void incr_num_action() {
            num_actions++;
        }

    };

    STRATEGY strategy;

    FACTORING_TYPE factoring_type;

    int min_mobility;

    float min_flexibility;

    int max_merge_steps;

    FactoredVars get_factoring() override;

    FactoredVars get_max_mobile_leaves() const;

    std::vector<ActionSchema> get_action_schemas() const;

    std::vector<PotentialLeaf> get_potential_leaves(const std::vector<ActionSchema> &action_schemas) const;

    void merge_potential_leaves(std::vector<PotentialLeaf> &potential_leaves, std::vector<std::set<size_t> > &var_to_p_leaves) const;

public:

    LPNoCenterFactoring(const Options &opts);

    static void add_options_to_parser(OptionParser &parser);

};
}

#endif
