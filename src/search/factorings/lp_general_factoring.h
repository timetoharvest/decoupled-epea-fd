#ifndef FACTORINGS_LP_GENERAL_FACTORING_H
#define FACTORINGS_LP_GENERAL_FACTORING_H

#include "lp_factoring.h"


namespace lp_general_factoring {
class LPGeneralFactoring : public lp_factoring::LPFactoring {

    enum STRATEGY {
        MML, // maximize mobile leaves
        MMAS, // maximize mobile action schemas
        MM_OPT, // maximize mobility
        MM_APPROX,
        MFA, // maximize mobile facts
        MM, // maximize mobility (sum)
        MCL // maximize number of conclulsive leaves
    };

    struct PotentialLeaf {
        size_t id;
        int num_actions; // number of actions with the effect schema
        double max_flexibility;
        std::vector<int> vars; // sorted
        std::vector<size_t> action_schemes;
        std::vector<size_t> self_mobile_as; // subset of leaf_only_scheme whose pre_vars are in vars
        std::vector<double> as_flexibility;

        PotentialLeaf(size_t id, const std::vector<int> &vars)
        : id(id), num_actions(0), max_flexibility(0.0), vars(vars) {}

        void add_leaf_only_schema(size_t action_schema);
    };

    STRATEGY strategy;

    int min_mobility;

    double min_flexibility;

    double min_fact_flexibility;

    bool add_cg_sccs_;

    int max_merge_steps;

    bool merge_overlapping;

    bool merge_dependent;

    std::vector<std::set<size_t> > var_to_p_leaves; // maps variables to potential leaf ids

    std::vector<PotentialLeaf> potential_leaves;

    FactoredVars get_factoring() override;

    bool is_as_leaf_irrelevant(const ActionSchema &as, const PotentialLeaf &leaf) const;

    bool is_as_leaf_conclusive(const ActionSchema &as, const PotentialLeaf &leaf) const;

    bool has_as_pre_or_eff_on_leaf(const ActionSchema &as, const PotentialLeaf &leaf) const;

    void construct_lp_conclusive(std::vector<lp::LPVariable> &variables,
                                 std::vector<lp::LPConstraint> &constraints,
                                 std::vector<bool> &can_be_leaf_var,
                                 std::vector<std::vector<size_t>> &mob_as_var_ids);

    FactoredVars compute_factoring();

    void filter_potential_leaves();

    void compute_leaf_flexibility();

    std::vector<PotentialLeaf> compute_potential_leaves();

    void add_cg_sccs();

    void merge_potential_leaves();

    void recompute_var_to_p_leaves();

public:

    LPGeneralFactoring(const options::Options &opts);

    static void add_options_to_parser(options::OptionParser &parser);

};
}

#endif
