#ifndef CPG_STORAGE
#define CPG_STORAGE

#include "../per_state_information.h"
#include "pricing_function.h"
#include "pruning_prices.h"
#include "pruning_reachable.h"
#include "reachability_function.h"


class CPGStorage {
    friend class CompliantPathGraph;
    friend class PathPriceTag;
    friend class Prices;
    friend class PruningPrices;
    friend class PruningReachable;
    friend class Reachable;

    static PerStateInformation<Prices> price_tags;
    static PerStateInformation<PruningPrices> pruning_prices;
    static PerStateInformation<PruningReachable> pruning_reachable;
    static PerStateInformation<Reachable> reach_tags;

public:

    static const CompliantPathGraph* get_cpg(const State &state) {
        if (g_factoring->get_search_type() == SAT){
            if (Prunable::do_pruning()){
                return &CPGStorage::pruning_reachable[state];
            } else {
                return &CPGStorage::reach_tags[state];
            }
        } else {
            if (Prunable::do_pruning()){
                return &CPGStorage::pruning_prices[state];
            } else {
                return &CPGStorage::price_tags[state];
            }
        }
    }

};

#endif