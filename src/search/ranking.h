#ifndef RANKING_H
#define RANKING_H

#include "utilities.h"
#include "variable_int.h"

#include <set>
#include <vector>

enum FEATURE {
    MOBILITY,
    LEAVES,
    L_VS_C_VARS,
    L_VS_C_DOM_SIZE,
    L_VS_C_AFF_ACTIONS,
    CENTER_AFF_A,
    LEAF_AFF_CENTER_A_AVG,
    LEAF_AFF_LEAF_A_AVG,
    LEAF_SIZE_AVG,
    CENTER_SIZE,
    NO
};

class Options;

class Ranking {

protected:

    FEATURE feature;

    virtual VarInt rank(const std::vector<std::set<int>> &) = 0;

    Ranking(const Options &ops);

public:

    FEATURE get_feature() const {
        return feature;
    }

    VarInt get_rank(const std::vector<std::set<int>> &);

    void print_all_rankings(const std::vector<std::set<int>> &);
};

#endif