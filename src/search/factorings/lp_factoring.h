#ifndef FACTORINGS_LP_FACTORINGS_H
#define FACTORINGS_LP_FACTORINGS_H

#include "../factoring.h"


namespace lp {
class LPConstraint;
enum class LPObjectiveSense;
enum class LPSolverType;
class LPVariable;
}


namespace lp_factoring {
class LPFactoring : public Factoring {

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
        std::vector<int> pre_vars; // sorted
        std::vector<int> eff_vars; // sorted

        ActionSchema(int num_actions, const std::vector<int> &pre_vars, const std::vector<int> &eff_vars)
            : num_actions(num_actions), pre_vars(pre_vars), eff_vars(eff_vars) {
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

    int cplex_time_limit;

    int memory_limit;

    lp::LPSolverType lp_solver_type;

    mutable utils::Timer timer;

    std::vector<double> solve_lp(lp::LPObjectiveSense sense,
            std::vector<lp::LPVariable> &vars,
            std::vector<lp::LPConstraint> &constrs,
            bool fail_on_out_of_memory = true,
            bool solve_lp_relaxation = false,
            int time_limit = -1) const;

    FactoredVars get_factoring() override;

    FactoredVars get_mip_factoring() const;

    FactoredVars get_max_mobile_leaves() const;

    FactoredVars get_max_mobile_leaves_modular() const;

    FactoredVars get_max_mobile_leaves_modular_no_schemes() const;

    std::vector<bool> get_mobile_vars() const;

    std::vector<ActionSchema> get_action_schemas() const;

    std::vector<PotentialLeaf> get_potential_leaves(const std::vector<ActionSchema> &action_schemas) const;

    void merge_potential_leaves(std::vector<PotentialLeaf> &potential_leaves, std::vector<std::set<size_t> > &var_to_p_leaves) const;

    void dump_lp(lp::LPObjectiveSense sense, const std::vector<lp::LPVariable> &vars, const std::vector<lp::LPConstraint> &constrs) const;

public:

    LPFactoring(const Options &opts);

    static void add_options_to_parser(OptionParser &parser);

};
}

#endif
