#ifndef FACTORING_RANKING_H
#define FACTORING_RANKING_H

#include "../utils/variable_int.h"

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

namespace options {
class OptionParser;
class Options;
}


class FactoringRanking {

protected:

    bool maximize;

    FACTORING_FEATURE feature;

    virtual VarInt rank(const std::vector<std::set<int> > &factoring) = 0;

public:

    FactoringRanking() {
        maximize = true;
    }

    FactoringRanking(const options::Options &opts);

    virtual ~FactoringRanking() = default;

    FACTORING_FEATURE get_feature() const {
        return feature;
    }

    VarInt get_rank(const std::vector<std::set<int> > &factoring);

    bool maximizes() const {
        return maximize;
    }

    std::string print_feature() const;


    static void print_all_rankings(const std::vector<std::set<int> > &factoring);

    static void add_options_to_parser(options::OptionParser &parser);
};


class MobilityRanking : public FactoringRanking {

public:
    MobilityRanking() : FactoringRanking() {
        feature = MOBILITY;
    }

    MobilityRanking(const options::Options &opts) : FactoringRanking(opts) {
        feature = MOBILITY;
    };

    ~MobilityRanking() = default;

    VarInt rank(const std::vector<std::set<int> > &factoring) override;
};

class LeavesRanking : public FactoringRanking {

public:
    LeavesRanking() : FactoringRanking() {
        feature = LEAVES;
    }

    LeavesRanking(const options::Options &opts) : FactoringRanking(opts) {
        feature = LEAVES;
    };

    ~LeavesRanking() = default;

    VarInt rank(const std::vector<std::set<int> > &factoring) override;
};

class DomSizeRanking : public FactoringRanking {

public:
    DomSizeRanking() : FactoringRanking() {
        feature = DOM_SIZE;
    }

    DomSizeRanking(const options::Options &opts) : FactoringRanking(opts) {
        feature = DOM_SIZE;
    };

    ~DomSizeRanking() = default;

    VarInt rank(const std::vector<std::set<int> > &factoring) override;
};

class VariablesRanking : public FactoringRanking {

public:
    VariablesRanking() : FactoringRanking() {
        feature = VARS;
    }

    VariablesRanking(const options::Options &opts) : FactoringRanking(opts) {
        feature = VARS;
    };

    ~VariablesRanking() = default;

    VarInt rank(const std::vector<std::set<int> > &factoring) override;
};

class AffActionsRanking : public FactoringRanking {

public:
    AffActionsRanking() : FactoringRanking() {
        feature = AFF_ACTIONS;
    }

    AffActionsRanking(const options::Options &opts) : FactoringRanking(opts) {
        feature = AFF_ACTIONS;
    };

    ~AffActionsRanking() = default;

    VarInt rank(const std::vector<std::set<int> > &factoring) override;
};


#endif
