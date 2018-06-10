#ifndef FACTORINGS_LP_FACTORINGS_H
#define FACTORINGS_LP_FACTORINGS_H

#include "../factoring.h"

enum STRATEGY {
    MIS,
};


class LPFactoring : public Factoring {

protected:

    STRATEGY strategy;

    FactoredVars get_factoring() override;

    FactoredVars get_mip_factoring();

public:

    LPFactoring(const Options &opts);

    static void add_options_to_parser(OptionParser &parser);

};


#endif
