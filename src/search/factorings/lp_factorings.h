#ifndef FACTORINGS_LP_FACTORINGS_H
#define FACTORINGS_LP_FACTORINGS_H

#include "../factoring.h"

enum STRATEGY {
    MIS,
    MIS_MOBILE,
    MAX_MOBILE_LEAVES,
};


class LPFactoring : public Factoring {

protected:

    STRATEGY strategy;

    FactoredVars get_factoring() override;

    FactoredVars get_mip_factoring(bool mobile_leaves);

    FactoredVars get_max_mobile_leaves();

    std::vector<bool> get_mobile_vars();


public:

    LPFactoring(const Options &opts);

    static void add_options_to_parser(OptionParser &parser);

};


#endif
