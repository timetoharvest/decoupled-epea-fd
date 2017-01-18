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


class Ranking {

protected:

    FEATURE feature;

    virtual VarInt rank(const std::vector<std::set<int> > &) = 0;

    Ranking() {};

public:

    FEATURE get_feature() const {
        return feature;
    }

    VarInt get_rank(const std::vector<std::set<int> > &);

    void print_all_rankings(const std::vector<std::set<int> > &);
};




class MobilityRanking : public Ranking {

public:
    MobilityRanking() : Ranking() {
        feature = MOBILITY;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

class LeavesRanking : public Ranking {

public:
    LeavesRanking() : Ranking() {
        feature = LEAVES;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

class DomSizeRanking : public Ranking {

public:
    DomSizeRanking() : Ranking() {
        feature = L_VS_C_DOM_SIZE;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

class VariablesRanking : public Ranking {

public:
    VariablesRanking() : Ranking() {
        feature = L_VS_C_VARS;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

class AffActionsRanking : public Ranking {

public:
    AffActionsRanking() : Ranking() {
        feature = L_VS_C_AFF_ACTIONS;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

#endif
