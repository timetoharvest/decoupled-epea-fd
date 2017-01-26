#ifndef RANKING_H
#define RANKING_H

#include "globals.h"
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

class OptionParser;
class Options;

class Ranking {

protected:

    FEATURE feature;

    bool maximize;

    virtual VarInt rank(const std::vector<std::set<int> > &) = 0;

    Ranking() {
        maximize = true;
    };

    Ranking(const Options &ops);

public:

    FEATURE get_feature() const {
        return feature;
    }

    bool maximizes() const {
        return maximize;
    }

    VarInt get_rank(const std::vector<std::set<int> > &);

    static void add_options_to_parser(OptionParser &parser);

};

void print_all_rankings(const std::vector<std::set<int> > &);


class MobilityRanking : public Ranking {

public:
    MobilityRanking() : Ranking() {
        maximize = true;
        feature = MOBILITY;
    }
    MobilityRanking(const Options &opts) : Ranking(opts) {
        feature = MOBILITY;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

class LeavesRanking : public Ranking {

public:
    LeavesRanking() : Ranking() {
        maximize = true;
        feature = LEAVES;
    }
    LeavesRanking(const Options &opts) : Ranking(opts) {
        feature = LEAVES;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

class DomSizeRanking : public Ranking {

public:
    DomSizeRanking() : Ranking() {
        maximize = true;
        feature = L_VS_C_DOM_SIZE;
    }
    DomSizeRanking(const Options &opts) : Ranking(opts) {
        feature = L_VS_C_DOM_SIZE;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

class VariablesRanking : public Ranking {

public:
    VariablesRanking() : Ranking() {
        maximize = true;
        feature = L_VS_C_VARS;
    }
    VariablesRanking(const Options &opts) : Ranking(opts) {
        feature = L_VS_C_VARS;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

class AffActionsRanking : public Ranking {

public:
    AffActionsRanking() : Ranking() {
        maximize = true;
        feature = L_VS_C_AFF_ACTIONS;
    }
    AffActionsRanking(const Options &opts) : Ranking(opts) {
        feature = L_VS_C_AFF_ACTIONS;
    };

    virtual VarInt rank(const std::vector<std::set<int> > &);
};

#endif
