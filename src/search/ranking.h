#ifndef RANKING_H
#define RANKING_H

#include "globals.h"
#include "utilities.h"
#include "variable_int.h"

#include <set>
#include <vector>


enum FACTORING_FEATURE {
    MOBILITY,
    LEAVES,
    VARS,
    DOM_SIZE,
    AFF_ACTIONS,
    NO_FACTORING_FEATURE
};

class OptionParser;
class Options;
class SearchEngine;

class Ranking {

protected:
    bool maximize;

    Ranking() {
        maximize = true;
    };

    Ranking(const Options &ops);

public:
    bool maximizes() const {
        return maximize;
    }

    static void add_options_to_parser(OptionParser &parser);

    virtual std::string print_feature() const = 0;

};

class FactoringRanking : public Ranking {

protected:

    FACTORING_FEATURE feature;

    virtual VarInt rank(const std::vector<std::set<int> > &) = 0;

public:

    FactoringRanking() : Ranking() {};
    FactoringRanking(const Options &ops) : Ranking(ops) {};
    FACTORING_FEATURE get_feature() const {
        return feature;
    }
    std::string print_feature() const;
    VarInt get_rank(const std::vector<std::set<int> > &);
};

void print_all_rankings(const std::vector<std::set<int> > &);

class MobilityRanking : public FactoringRanking {

public:
    MobilityRanking() : FactoringRanking() {
        maximize = true;
        feature = MOBILITY;
    }
    MobilityRanking(const Options &opts) : FactoringRanking(opts) {
        feature = MOBILITY;
    };

    VarInt rank(const std::vector<std::set<int> > &);
};

class LeavesRanking : public FactoringRanking {

public:
    LeavesRanking() : FactoringRanking() {
        maximize = true;
        feature = LEAVES;
    }
    LeavesRanking(const Options &opts) : FactoringRanking(opts) {
        feature = LEAVES;
    };

    VarInt rank(const std::vector<std::set<int> > &);
};

class DomSizeRanking : public FactoringRanking {

public:
    DomSizeRanking() : FactoringRanking() {
        maximize = true;
        feature = DOM_SIZE;
    }
    DomSizeRanking(const Options &opts) : FactoringRanking(opts) {
        feature = DOM_SIZE;
    };

    VarInt rank(const std::vector<std::set<int> > &);
};

class VariablesRanking : public FactoringRanking {

public:
    VariablesRanking() : FactoringRanking() {
        maximize = true;
        feature = VARS;
    }
    VariablesRanking(const Options &opts) : FactoringRanking(opts) {
        feature = VARS;
    };

    VarInt rank(const std::vector<std::set<int> > &);
};

class AffActionsRanking : public FactoringRanking {

public:
    AffActionsRanking() : FactoringRanking() {
        maximize = true;
        feature = AFF_ACTIONS;
    }
    AffActionsRanking(const Options &opts) : FactoringRanking(opts) {
        feature = AFF_ACTIONS;
    };

    VarInt rank(const std::vector<std::set<int> > &);
};


#endif
