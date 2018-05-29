#include "ranking.h"

#include "factoring.h"
#include "operator.h"
#include "option_parser.h"
#include "plugin.h"
#include "search_engine.h"
#include "state_registry.h"

using namespace std;

Ranking::Ranking(const Options &opts) :
            maximize(opts.get<bool>("maximize")) {
}


VarInt FactoringRanking::get_rank(const vector<set<int>> & factoring) {
    return rank(factoring);
}

string FactoringRanking::print_feature() const{
    switch (feature) {
        case LEAVES:
            return "L";
            break;
        case DOM_SIZE:
            return "D";
            break;
        case AFF_ACTIONS:
            return "A";
            break;
        case VARS:
            return "V";
            break;
        case MOBILITY:
            return "M";
            break;
        default: case NO_FACTORING_FEATURE:
            return "";
    }
}

void print_all_rankings(const vector<set<int> > &factoring) {
    MobilityRanking *mobility = new MobilityRanking();
    cout << "mis_ranking_mobility: ";
    mobility->get_rank(factoring).dump_number();
    LeavesRanking *leaves = new LeavesRanking();
    cout << "mis_ranking_leaves: ";
    leaves->get_rank(factoring).dump_number();
    DomSizeRanking *dom_size = new DomSizeRanking();
    cout << "mis_ranking_dom_size: ";
    dom_size->get_rank(factoring).dump_number();
    VariablesRanking *vars = new VariablesRanking();
    cout << "mis_ranking_variables: ";
    vars->get_rank(factoring).dump_number();
    AffActionsRanking *aff_actions = new AffActionsRanking();
    cout << "mis_ranking_aff_actions: ";
    aff_actions->get_rank(factoring).dump_number();
}

VarInt MobilityRanking::rank(const vector<set<int> > &factoring) {
    if (factoring.size() == 0){
        return 0;
    }
    int factor_actions = 0;
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);
    vector<bool> operator_seen(g_operators.size(), false);

    for (size_t i = 0; i < factoring.size(); i++){
        for (int var : factoring[i]){
            belongs_to_factor[var] = i;
        }
    }

    for (size_t f = 0; f < factoring.size(); ++f) {
        for (int v: factoring[f]){

            for (int op : Factoring::get_var_to_affecting_op()[v]) {
                if (!operator_seen[op]) {
                    operator_seen[op] = true;

                    bool center = false;
                    set<int> eff_factors;
                    for (const Effect &eff : g_operators[op].get_effects()) {
                        eff_factors.insert(belongs_to_factor[eff.var]);
                        if (belongs_to_factor[eff.var] == -1 || eff_factors.size() > 1) {
                            center = true;
                            break;
                        }
                    }
                    if (center) {
                        continue;
                    }
                    set<int> pre_factors;
                    for (const Condition &pre : g_operators[op].get_preconditions()) {
                        pre_factors.insert(belongs_to_factor[pre.var]);
                    }
                    pre_factors.erase(-1);

                    if (pre_factors.size() <= 1 &&
                        (pre_factors.empty() || *pre_factors.begin() == *eff_factors.begin())) {
                        // op has only preconditions and effects on (at most) a single leaf factor
                        ++factor_actions;
                    }
                }
            }
        }
    }

    VarInt mobility(0);
    mobility = factor_actions*100 / max(1,(int)g_operators.size());
    return mobility;
}

VarInt LeavesRanking::rank(const vector<set<int>> &factoring) {
    if (factoring.size() == 0){
        return 0;
    }
    vector<bool> is_frozen_factor(factoring.size(), true);
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);
    size_t num_frozen = factoring.size();
    vector<bool> operator_seen(g_operators.size(), false);
    
    for (size_t i = 0; i < factoring.size(); ++i){
        for (int var : factoring[i]){
            belongs_to_factor[var] = i;
        }
    }

    for (size_t f = 0; f < factoring.size(); ++f) {
        for (int v : factoring[f]){
            for (int op : Factoring::get_var_to_affecting_op()[v]) {
                if (!operator_seen[op]) {
                    // if an operator has been handled by another leaf factor, it has to be a center action
                    operator_seen[op] = true;

                    bool center = false;
                    int eff_factor = -1;
                    for (const Effect &eff : g_operators[op].get_effects()) {
                        if (belongs_to_factor[eff.var] == -1){
                            center = true;
                            break;
                        } else if (eff_factor == -1){
                            eff_factor = belongs_to_factor[eff.var];
                        }  else if (eff_factor != belongs_to_factor[eff.var]){
                            center = true;
                            break;
                        }
                    }
                    if (center) {
                        continue;
                    }
                    int pre_factor = -1;
                    bool single_pre_factor = true;
                    for (const Condition &pre : g_operators[op].get_preconditions()) {
                        if (belongs_to_factor[pre.var] != -1){
                            if (pre_factor == -1){
                                pre_factor = belongs_to_factor[pre.var];
                            } else if (pre_factor != belongs_to_factor[pre.var]) {
                                single_pre_factor = false;
                                break;
                            }
                        }
                    }

                    if (single_pre_factor && (pre_factor == -1 || pre_factor == eff_factor)) {
                        // op has preconditions and effects on (at most) a single leaf factor
                        if (is_frozen_factor[f]) {
                            --num_frozen;
                            is_frozen_factor[f] = false;
                            if (num_frozen == 0) {
                                return VarInt(factoring.size());
                            }
                        }
                        break;
                    }
                }
            }
            if (!is_frozen_factor[f]) {
                break;
            }
        }
    }

    return VarInt(factoring.size() - num_frozen);
}

VarInt DomSizeRanking::rank(const vector<set<int>> & factoring) {
    if (factoring.size() == 0){
        return 0;
    }
    VarInt leaf(1);
    VarInt center(1);
    vector<bool> in_center(g_variable_domain.size(), true);
    for (size_t i = 0; i < factoring.size(); i++) {
        for (set<int>::iterator it = factoring[i].begin(); it != factoring[i].end(); ++it) {
            in_center[*it] = false;
        }
    }
    for (size_t i = 0; i < g_variable_domain.size(); ++i) {
        if (in_center[i]) {
            if (g_variable_domain[i] == 0) {
                VarInt f(g_variable_domain[i]);
                center *= f;
            }
        } else {
            VarInt f(g_variable_domain[i]);
            leaf *= f;
        }
    }
    if (center.count() == 0) {
        return leaf;
    } else {
        return leaf / center;
    }
}

VarInt VariablesRanking::rank(const vector<set<int>> & factoring) {
    if (factoring.size() == 0){
        return 0;
    }
    size_t leaf = 0;
    size_t center = 0;
    size_t size = g_variable_domain.size();
    center = size;
    for (set<int> f:factoring){
        center -= f.size();
    }
    if (center == 0) {
        VarInt res(size);
        return res;
    } else {
        leaf = size - center;
        leaf = leaf * 100 / center;
        VarInt res(leaf);
        return res;
    }
}

VarInt AffActionsRanking::rank(const vector<set<int>> &factoring) {
    if (factoring.size() == 0 || g_operators.size() == 0){
        return 0;
    }
    vector<int> affecting_center(factoring.size(),0);
    vector<int> affecting_leaf(factoring.size(),0);
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);
    vector<bool> operator_seen(g_operators.size(), false);

    for (size_t i = 0; i < factoring.size(); i++){
        for (int var : factoring[i]){
            belongs_to_factor[var] = i;
        }
    }

    for (size_t f = 0; f < factoring.size(); ++f) {
        for (int v: factoring[f]){

            for (int op : Factoring::get_var_to_affecting_op()[v]) {
                if (!operator_seen[op]) {
                    operator_seen[op] = true;

                    bool center = false;
                    set<int> eff_factors;
                    for (const Effect &eff : g_operators[op].get_effects()) {
                        eff_factors.insert(belongs_to_factor[eff.var]);
                        if (belongs_to_factor[eff.var] == -1 || eff_factors.size() > 1) {
                            center = true;
                        }
                    }
                    if (center) {
                        if (eff_factors.size() > 1) {
                            for (int f:eff_factors) {
                                if (f != -1) {
                                    ++affecting_center[f];
                                }
                            }
                        }
                        continue;
                    }
                    set<int> pre_factors;
                    for (const Condition &pre : g_operators[op].get_preconditions()) {
                        pre_factors.insert(belongs_to_factor[pre.var]);
                    }
                    pre_factors.erase(-1);

                    if (pre_factors.size() <= 1 &&
                        (pre_factors.empty() || *pre_factors.begin() == *eff_factors.begin())) {
                        // op has only preconditions and effects on (at most) a single leaf factor
                        ++affecting_leaf[belongs_to_factor[v]];
                    }
                    else {
                        for (int f:eff_factors) {
                            if (f != -1) {
                                ++affecting_center[f];

                            }
                        }
                    }
                }
            }
        }
    }
    vector<int> affecting_ratio(factoring.size(),0);
    for (size_t i = 0; i < factoring.size(); ++i) {
        affecting_ratio[i] = (affecting_leaf[i] * 100) / (max(1,affecting_leaf[i] + affecting_center[i]));
    }
    int result = 0;
    for (size_t i = 0; i < factoring.size(); ++i) {
        result += affecting_ratio[i];
    }
    result = result / factoring.size();
    VarInt res(result);
    return res;
}


void Ranking::add_options_to_parser(OptionParser &parser) {
    parser.add_option<bool>(
            "maximize",
            "TODO",
            "true");
}

static FactoringRanking *_parse_mobility(OptionParser &parser) {

    FactoringRanking::add_options_to_parser(parser);
    Options opts = parser.parse();
    MobilityRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new MobilityRanking(opts);
    }

    return ranking;
}

static FactoringRanking *_parse_leaves(OptionParser &parser) {

    FactoringRanking::add_options_to_parser(parser);
    Options opts = parser.parse();
    LeavesRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new LeavesRanking(opts);
    }

    return ranking;
}

static FactoringRanking *_parse_dom_size(OptionParser &parser) {

    FactoringRanking::add_options_to_parser(parser);
    Options opts = parser.parse();
    DomSizeRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new DomSizeRanking(opts);
    }

    return ranking;
}

static FactoringRanking *_parse_vars(OptionParser &parser) {

    FactoringRanking::add_options_to_parser(parser);
    Options opts = parser.parse();
    VariablesRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new VariablesRanking(opts);
    }

    return ranking;
}

static FactoringRanking *_parse_aff_actions(OptionParser &parser) {

    FactoringRanking::add_options_to_parser(parser);
    Options opts = parser.parse();
    AffActionsRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new AffActionsRanking(opts);
    }

    return ranking;
}


static Plugin<FactoringRanking> _plugin_mobility("mobility", _parse_mobility);
static Plugin<FactoringRanking> _plugin_leaves("leaves", _parse_leaves);
static Plugin<FactoringRanking> _plugin_dom_size("dom_size", _parse_dom_size);
static Plugin<FactoringRanking> _plugin_vars("vars", _parse_vars);
static Plugin<FactoringRanking> _plugin_aff_actions("aff_actions", _parse_aff_actions);
