#ifndef CPG_STORAGE
#define CPG_STORAGE

#include "../per_state_information.h"
#include "pricing_function.h"
#include "reachability_function.h"


class CPGStorage {
    friend class CompliantPathGraph;
    friend class PathPriceTag;
    friend class Prices;
    friend class Reachable;

    static PerStateInformation<Prices> price_tags;
    static PerStateInformation<Reachable> reach_tags;

public:

    static const CompliantPathGraph* get_cpg(const State &state) {
        if (g_factoring->get_search_type() == SAT){
            return &CPGStorage::reach_tags[state];
        } else {
            return &CPGStorage::price_tags[state];
        }
    }

};

#endif