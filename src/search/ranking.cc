#include "ranking.h"

#include "globals.h"
#include "operator.h"
#include "option_parser.h"
#include "plugin.h"

using namespace std;

Ranking::Ranking() {}

VarInt Ranking::get_rank(const vector<set<int>> & factoring) {
    return rank(factoring);
}


void Ranking::print_all_rankings(const vector<set<int> > &factoring) {
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
    vector<int> factor_actions(factoring.size(), 0);
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);

    for (size_t i = 0; i < factoring.size(); i++){
        for (set<int>::iterator it = factoring[i].begin(); it != factoring[i].end(); ++it){
            belongs_to_factor[*it] = i;
        }
    }

    for (size_t i = 0; i < g_operators.size(); i++){
        const Operator *op = &g_operators[i];
        int affects_factor = -1;
        set<int> pre_factors;
        set<int> eff_factors;

        for (size_t prev = 0; prev < op->get_preconditions().size(); ++prev) {
            const Condition &prevail = op->get_preconditions()[prev];
            pre_factors.insert(belongs_to_factor[prevail.var]);
        }
        pre_factors.erase(-1);
        bool center = false;
        for (size_t post = 0; post < op->get_effects().size(); post++){
            const Effect &prepost = op->get_effects()[post];
            eff_factors.insert(belongs_to_factor[prepost.var]);
            affects_factor = belongs_to_factor[prepost.var];
            if (affects_factor == -1){
                center = true;
            }
        }
        if (!center && !(eff_factors.size() > 1) && !(pre_factors.size() > 1) &&
            (pre_factors.empty() || (*pre_factors.begin() == *eff_factors.begin()))) {
            ++factor_actions[affects_factor];
        }
    }
    VarInt mobility(1);
    bool any_actions = false;
    for (int i:factor_actions) {
        if (i > 0) {
            any_actions = true;
            VarInt f(i);
            mobility *= f;
        }
    }
    if (!any_actions) {
        mobility = 0;
    }
    return mobility;
}

VarInt LeavesRanking::rank(const vector<set<int>> &factoring) {
    if (factoring.size() == 0){
        return 0;
    }
    vector<bool> is_frozen_factor(factoring.size(), true);
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);
    size_t num_frozen = factoring.size();
    
    for (size_t i = 0; i < factoring.size(); i++){
        for (int var : factoring[i]){
            belongs_to_factor[var] = i;
        }
    }
    
    for (const Operator &op : g_operators){
        bool center = false;
        set<int> eff_factors;
        int affects_factor = -1;
        
        for (const Effect &eff : op.get_effects()){
            eff_factors.insert(belongs_to_factor[eff.var]);
            affects_factor = belongs_to_factor[eff.var];
            if (affects_factor == -1 || eff_factors.size() > 1){
                center = true;
                break;
            }
        }
        
        if (center){
            // op affects center variable or has effect on more than 1 factor
            continue;
        }
        if (!is_frozen_factor[affects_factor]){
            // factor is mobile, no need to continue
            continue;
        }
        
        set<int> pre_factors;
        for (const Condition &pre : op.get_preconditions()) {
            pre_factors.insert(belongs_to_factor[pre.var]);
        }
        pre_factors.erase(-1);
        
        if (pre_factors.size() <= 1  && (pre_factors.empty() || *pre_factors.begin() == *eff_factors.begin())){
            // op has only preconditions and effects on (at most) a single leaf factor
            if (is_frozen_factor[affects_factor]){
                --num_frozen;
                is_frozen_factor[affects_factor] = false;
                if (num_frozen == 0){
                    return VarInt(factoring.size());
                }
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
    if (factoring.size() == 0){
        return 0;
    }
    vector<int> affecting_center(factoring.size(),0);
    vector<int> affecting_leaf(factoring.size(),0);
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);

    for (size_t i = 0; i < factoring.size(); i++){
        for (set<int>::iterator it = factoring[i].begin(); it != factoring[i].end(); ++it){
            belongs_to_factor[*it] = i;
        }
    }

    for (size_t i = 0; i < g_operators.size(); i++){
        const Operator *op = &g_operators[i];
        int affects_factor = -1;
        set<int> pre_factors;
        set<int> eff_factors;

        for (size_t prev = 0; prev < op->get_preconditions().size(); ++prev) {
            const Condition &prevail = op->get_preconditions()[prev];
            pre_factors.insert(belongs_to_factor[prevail.var]);
        }
        pre_factors.erase(-1);
        bool center = false;
        for (size_t post = 0; post < op->get_effects().size(); post++){
            const Effect &prepost = op->get_effects()[post];
            eff_factors.insert(belongs_to_factor[prepost.var]);
            affects_factor = belongs_to_factor[prepost.var];
            if (affects_factor == -1){
                center = true;
            }
        }
        if (!center && !(eff_factors.size() > 1) && !(pre_factors.size() > 1) &&
            (pre_factors.empty() || (*pre_factors.begin() == *eff_factors.begin()))) {
            ++affecting_leaf[affects_factor];
        }
        else if (center || (eff_factors.size() > 1) || (pre_factors.size() > 1) ||
                 !(pre_factors.empty() || (*pre_factors.begin() == *eff_factors.begin()))) {
            for (int f:eff_factors) {
                if (f != -1) {
                    ++affecting_center[f];

                }
            }
        }
    }
    vector<int> affecting_ratio(factoring.size(),0);
    for (size_t i = 0; i < factoring.size(); ++i) {
        affecting_ratio[i] = (affecting_leaf[i] * 100) / (affecting_leaf[i] + affecting_center[i]);
    }
    int result = 0;
    for (size_t i = 0; i < factoring.size(); ++i) {
        result += affecting_ratio[i];
    }
    result = result / factoring.size();
    VarInt res(result);
    return res;
}


// Deprecated functions ----------------------------------------

uint64_t rank_leaf_size_avg(const vector<set<int>> &factoring, bool all_data) {
    int avg = 0;
    int max = 0;
    int min = 0;
    if (!factoring.empty()) {
        min = factoring[0].size();
        for (set<int> factor:factoring) {
            int s = (int) factor.size();
            avg += s;
            if (s > max) {
                max = s;
            }
            if (s < min) {
                min = s;
            }
        }
        avg = avg / factoring.size();
    }
    if (all_data) {
        cout << "Ranking " << "leaf_size_min" << ": " << min << endl;
        cout << "Ranking " << "leaf_size_max" << ": " << max << endl;
    }
    return (uint64_t)avg;
}

uint64_t rank_center_size(const vector<set<int>> &factoring, bool) {
    size_t size = g_variable_domain.size();
    for (set<int> f:factoring){
        size -= f.size();
    }
    return size;
}


uint64_t rank_aff_a(const vector<set<int>> &factoring, bool all_data, bool aff_leaf, bool leaf_a) {
    vector<int> factor_affecting_actions(factoring.size(), 0);
    int affecting_center = 0;
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);

    for (size_t i = 0; i < factoring.size(); i++){
        for (set<int>::iterator it = factoring[i].begin(); it != factoring[i].end(); ++it){
            belongs_to_factor[*it] = i;
        }
    }

    for (size_t i = 0; i < g_operators.size(); i++){
        const Operator *op = &g_operators[i];
        int affects_factor = -1;
        set<int> pre_factors;
        set<int> eff_factors;

        for (size_t prev = 0; prev < op->get_preconditions().size(); ++prev) {
            const Condition &prevail = op->get_preconditions()[prev];
            pre_factors.insert(belongs_to_factor[prevail.var]);
        }
        pre_factors.erase(-1);
        bool center = false;
        for (size_t post = 0; post < op->get_effects().size(); post++){
            const Effect &prepost = op->get_effects()[post];
            eff_factors.insert(belongs_to_factor[prepost.var]);
            affects_factor = belongs_to_factor[prepost.var];
            if (affects_factor == -1){
                center = true;
            }
        }
        if (aff_leaf && leaf_a && !center && !(eff_factors.size() > 1) && !(pre_factors.size() > 1) &&
            (pre_factors.empty() || (*pre_factors.begin() == *eff_factors.begin()))) {
            ++factor_affecting_actions[affects_factor];
        }
        else if (aff_leaf && !leaf_a && (center || (eff_factors.size() > 1) || (pre_factors.size() > 1) ||
                                         !(pre_factors.empty() || (*pre_factors.begin() == *eff_factors.begin())))) {
            for (int eff:eff_factors) {
                if (eff != -1) {
                    ++factor_affecting_actions[eff];
                }
            }
        }
        else if (!aff_leaf && center) {
            ++affecting_center;
        }

    }
    if (aff_leaf) {
        int avg = 0;
        int max = 0;
        int min = 0;
        if (!factor_affecting_actions.empty()) {
            min = factor_affecting_actions[0];
            for (int i:factor_affecting_actions) {
                avg += i;
                if (i > max) {
                    max = i;
                }
                if (i < min) {
                    min = i;
                }
            }
            avg = avg / factor_affecting_actions.size();
        }
        if (all_data) {
            if (!leaf_a) {
                cout << "Ranking " << "leaf_affecting_center_actions_min" << ": " << min << endl;
                cout << "Ranking " << "leaf_affecting_center_actions_max" << ": " << max << endl;
            } else {
                cout << "Ranking " << "leaf_affecting_leaf_actions_min" << ": " << min << endl;
                cout << "Ranking " << "leaf_affecting_leaf_actions_max" << ": " << max << endl;
            }
        }
        return (uint64_t) avg;
    } else {
        return (uint64_t) affecting_center;
    }

}

static Ranking *_parse_mobility(OptionParser &parser) {

    MobilityRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new MobilityRanking();
    }

    return ranking;
}

static Ranking *_parse_leaves(OptionParser &parser) {

    LeavesRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new LeavesRanking();
    }

    return ranking;
}

static Ranking *_parse_dom_size(OptionParser &parser) {

    DomSizeRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new DomSizeRanking();
    }

    return ranking;
}

static Ranking *_parse_vars(OptionParser &parser) {

    VariablesRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new VariablesRanking();
    }

    return ranking;
}

static Ranking *_parse_aff_actions(OptionParser &parser) {

    AffActionsRanking *ranking = 0;
    if (!parser.dry_run()) {
        ranking = new AffActionsRanking();
    }

    return ranking;
}

static Plugin<Ranking> _plugin_mobility("mobility", _parse_mobility);
static Plugin<Ranking> _plugin_leaves("leaves", _parse_leaves);
static Plugin<Ranking> _plugin_dom_size("dom_size", _parse_dom_size);
static Plugin<Ranking> _plugin_vars("vars", _parse_vars);
static Plugin<Ranking> _plugin_aff_actions("aff_actions", _parse_aff_actions);
