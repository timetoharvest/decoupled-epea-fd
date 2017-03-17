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

enum SEARCH_FEATURE {
    EXP_STATES,
    DEC_STATE_SPACE,
    HEUR_VAL_IMP,
    MAX_DUP,
    NO_SEARCH_FEATURE
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

class SearchRanking : public Ranking {

protected:

    SEARCH_FEATURE feature;

    virtual size_t rank(SearchEngine*) = 0;
public:
    SearchRanking() : Ranking() {};

    SearchRanking(const Options &ops) : Ranking(ops) {};

    std::string print_feature() const;

    size_t get_rank(SearchEngine*);
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

class ExpStatesRanking : public SearchRanking {

public:
    ExpStatesRanking() : SearchRanking() {
        maximize = true;
        feature = EXP_STATES;
    }
    ExpStatesRanking(const Options &opts) : SearchRanking(opts) {
        feature = EXP_STATES;
    };

    size_t rank(SearchEngine*);
};

class HeurValImpRanking : public SearchRanking {

public:
    HeurValImpRanking() : SearchRanking() {
        maximize = true;
        feature = HEUR_VAL_IMP;
    }
    HeurValImpRanking(const Options &opts) : SearchRanking(opts) {
        feature = HEUR_VAL_IMP;
    };

    size_t rank(SearchEngine*);
};

class DecStateSpaceRanking : public SearchRanking {

public:
    DecStateSpaceRanking() : SearchRanking() {
        maximize = true;
        feature = DEC_STATE_SPACE;
    }

    DecStateSpaceRanking(const Options &opts) : SearchRanking(opts) {
        feature = DEC_STATE_SPACE;
    };

    size_t rank(SearchEngine*);
};

class MaxDupRanking : public SearchRanking {

public:
    MaxDupRanking() : SearchRanking() {
        maximize = false;
        feature = MAX_DUP;
    }
    MaxDupRanking(const Options &opts) : SearchRanking(opts) {
        feature = MAX_DUP;
    };

    size_t rank(SearchEngine*);
};

#endif
