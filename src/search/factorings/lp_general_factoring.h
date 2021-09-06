#ifndef FACTORINGS_LP_GENERAL_FACTORINGS_H
#define FACTORINGS_LP_GENERAL_FACTORINGS_H

#include "../factoring.h"


namespace lp {
class LPConstraint;
enum class LPObjectiveSense;
enum class LPSolverType;
class LPVariable;
}


namespace lp_general_factoring {
class LPGeneralFactoring : public Factoring {

    enum STRATEGY {
        MML,
        MM_OPT,
        MM_APPROX,
//        MF,
    };

    struct PotentialLeaf {
        size_t id;
        int num_actions; // number of actions with the effect schema
        std::vector<int> vars; // sorted
        std::vector<size_t> action_schemes;
        std::vector<size_t> self_mobile_as; // subset of leaf_only_scheme whose pre_vars are in vars

        PotentialLeaf(size_t id, const std::vector<int> &vars)
        : id(id), num_actions(0), vars(vars) {}

        void add_leaf_only_schema(size_t action_schema);
    };

    STRATEGY strategy;

    int min_mobility;

    float min_flexibility;

    bool add_cg_sccs_;

    int max_merge_steps;

    bool merge_overlapping;

    bool merge_dependent;

    int cplex_time_limit;

    int memory_limit;

    lp::LPSolverType lp_solver_type;

    mutable utils::Timer timer;

    double infty;

    mutable std::vector<std::set<size_t> > var_to_p_leaves; // maps variables to potential leaf ids

    bool check_num_constraints_and_time(const std::vector<lp::LPConstraint> &constraints) const;

    // TODO move these 2 to a common base class
    std::vector<double> solve_lp(lp::LPObjectiveSense sense,
            std::vector<lp::LPVariable> &vars,
            std::vector<lp::LPConstraint> &constrs,
            bool fail_on_out_of_memory = true,
            bool solve_lp_relaxation = false,
            int time_limit = -1) const;

    void dump_lp(lp::LPObjectiveSense sense, const std::vector<lp::LPVariable> &vars, const std::vector<lp::LPConstraint> &constrs) const;

    FactoredVars get_factoring() override;

    FactoredVars compute_factoring() const;

    void filter_action_schemas() const;

    std::vector<PotentialLeaf> get_potential_leaves() const;

    void add_cg_sccs(std::vector<PotentialLeaf> &potential_leaves) const;

    void merge_potential_leaves(std::vector<PotentialLeaf> &potential_leaves) const;

public:

    LPGeneralFactoring(const Options &opts);

    static void add_options_to_parser(OptionParser &parser);

};
}

#endif
