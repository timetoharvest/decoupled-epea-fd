#include "cost_frontier.h"

#include "../operator.h"
#include "../plugin.h"
#include "../price_tag.h"
#include "../state_registry.h"


using namespace std;

CostFrontierPruning::CostFrontierPruning(const Options&) {
    cout << "using cost frontier pruning" << endl;
    assert(g_factoring->get_profile() == FORK);
}

DOMINATION CostFrontierPruning::check_dominance(const State &center, const PriceTag &check, 
                                                const PriceTag &reference, DOMINATION needed) const {
    // TODO implement
    
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        bool is_frontier = false;
        for (size_t id = 0; id < g_leaf_state_successors[factor].size(); ++id){
            for (size_t o = 0; o < g_leaf_state_successors[factor][id].size(); ++o){
                const Operator *op = g_leaf_state_successors[factor][id][o].first;
                if (op->is_center_applicable(center)){
                    continue;
                }
                int new_cost = check.get_cost_of_state(id, factor) + get_adjusted_action_cost(*op, g_factoring->get_cost_type());
                if (check.get_cost_of_state(g_leaf_state_successors[factor][id][o].second, factor) > new_cost){
                    is_frontier = true;
                    break;
                }
            }
            if (is_frontier){
                break;
            }
        }
    }
    
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

