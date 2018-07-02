#ifndef FACTORINGS_LP_FACTORINGS_H
#define FACTORINGS_LP_FACTORINGS_H

#include "../factoring.h"

enum STRATEGY {
    MIS,
    MIS_MOBILE,
    MAX_MOBILE_LEAVES,
};

enum FACTORING_TYPE {
    STRICT_STAR,
    GENERAL_STAR,
};

struct ActionSchema {
    int num_actions;
    std::vector<int> pre_var;
    std::vector<int> eff_var;

    ActionSchema(int num_actions, const std::vector<int> pre_var, const std::vector<int> eff_var)
        : num_actions(num_actions), pre_var(pre_var), eff_var(eff_var) {}

    void incr_num_action() {
        num_actions++;
    }

    void dump() const;
};

class LPFactoring : public Factoring {

protected:

    STRATEGY strategy;

    FACTORING_TYPE factoring_type;

    FactoredVars get_factoring() override;

    FactoredVars get_mip_factoring(bool mobile_leaves);

    FactoredVars get_max_mobile_leaves();

    std::vector<bool> get_mobile_vars();

    std::vector<ActionSchema> get_action_schemes();

public:

    LPFactoring(const Options &opts);

    static void add_options_to_parser(OptionParser &parser);

};


#endif
