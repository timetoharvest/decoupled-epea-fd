#include "cost_frontier.h"

#include "../plugin.h"
#include "../price_tag.h"
#include "../state_registry.h"


using namespace std;

CostFrontierPruning::CostFrontierPruning(const Options&) {
    cout << "using cost frontier pruning" << endl;
}

DOMINATION CostFrontierPruning::check_dominance(const PriceTag &, const PriceTag &, DOMINATION ) const {
    // TODO implement
    
    return NONE;
}


static Pruning *_parse_cost_frontier(OptionParser &parser) {
    Options opts = parser.parse();

    CostFrontierPruning *pruning = 0;
    if (!parser.dry_run()) {
        pruning = new CostFrontierPruning(opts);
    }

    return pruning;
}

static Plugin<Pruning> _plugin("cost_frontier", _parse_cost_frontier);

