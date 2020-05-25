#ifndef FACTORINGS_MIS_FACTORINGS_H
#define FACTORINGS_MIS_FACTORINGS_H

#include "../algorithms/mis.h"
#include "../factoring.h"
#include "factoring_ranking.h"

#include <vector>

namespace mis_factoring {
class MaxIndependentSetFactoring : public Factoring {

    int arg_minimize_center;

    bool arg_minimize_frozen;

    std::vector<FactoringRanking* > arg_rankings;

    int arg_xth_factoring = 0;

    int arg_merge_leaves = 0;

    int arg_find_all;

    bool arg_all_rankings;

    int arg_multiple_mis;


    std::vector<FactoredVars> get_factorings(const std::vector<std::vector<FactoringRanking*> > &rankings);

    void optimize_feature(FactoredVars &, const NGraph &,int &, int &, int &) const;

    NGraph generate_ngraph() const;

    bool check_factoring(const FactoredVars &factoring) const;

protected:

    FactoredVars get_factoring() override;

public:

    MaxIndependentSetFactoring(const Options &opts);


    static void add_options_to_parser(OptionParser &parser);

};
}

#endif
