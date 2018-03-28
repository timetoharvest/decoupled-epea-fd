#include "operator.h"

#include "compliant_paths/compliant_path_graph.h"
#include "compliant_paths/cpg_storage.h"
#include "factoring.h"
#include "globals.h"
#include "state_registry.h"

#include <iostream>

using namespace std;

Condition::Condition(istream &in) {
    in >> var >> val;
}

bool Condition::is_applicable(const State &state) const {
    assert(!g_factoring || g_belongs_to_factor[var] == LeafFactorID::CENTER);
    return state[var] == val;
}

bool Condition::is_applicable(const LeafState &state) const {
    assert(g_belongs_to_factor[var] == (int) state.get_id().get_factor());
    return state[var] == val;
}

void Condition::dump() const {
    cout << g_fact_names[var][val];
}

void Effect::dump() const {
    cout << " => " << g_fact_names[var][val];
    if (!conditions.empty()) {
        cout << " if";
        for (size_t i = 0; i < conditions.size(); ++i) {
            cout << " ";
            conditions[i].dump();
        }
    }
}


void Operator::read_pre_post(istream &in) {
    int cond_count, var, pre, post;
    in >> cond_count;
    vector<Condition> conditions;
    conditions.reserve(cond_count);
    for (int i = 0; i < cond_count; ++i)
        conditions.push_back(Condition(in));
    in >> var >> pre >> post;
    if (pre != -1)
        preconditions.push_back(Condition(var, pre));
    effects.push_back(Effect(var, post, conditions));
}

Operator::Operator(istream &in, bool axiom) {
    marked = false;
    dead = false;

    is_an_axiom = axiom;
    if (!is_an_axiom) {
        check_magic(in, "begin_operator");
        in >> ws;
        getline(in, name);
        int count;
        in >> count;
        for (int i = 0; i < count; ++i)
            preconditions.push_back(Condition(in));
        in >> count;
        for (int i = 0; i < count; ++i)
            read_pre_post(in);

        int op_cost;
        in >> op_cost;
        cost = g_use_metric ? op_cost : 1;

        g_min_action_cost = min(g_min_action_cost, cost);
        g_max_action_cost = max(g_max_action_cost, cost);

        check_magic(in, "end_operator");
    } else {
        name = "<axiom>";
        cost = 0;
        check_magic(in, "begin_rule");
        read_pre_post(in);
        check_magic(in, "end_rule");
    }
}

bool Operator::is_applicable(const State &state) const {
    if (g_factoring){    // not really efficient, but how to do it better?
        if (!is_center_applicable(state)){
            return false;
        }
        if (g_factoring->get_profile() == FORK && get_affected_factor() == LeafFactorID::CENTER){
            // in fork topology, center actions don't have leaf preconditions
            return true;
        }
        const CompliantPathGraph *price_tag = CPGStorage::storage->get_cpg(state);
        for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
            if (has_precondition_on(factor)){
                bool applicable = false;
                size_t number_states = price_tag->get_number_states(factor);
                LeafStateHash id(0);
                while (number_states > 0){
                    if (price_tag->has_leaf_state(id, factor)){
                        --number_states;
                        if (is_applicable(id, factor)){
                            applicable = true;
                            break;
                        }
                    }
                    ++id;
                }
                if (!applicable){
                    return false;
                }
            }
        }
    } else {
        for (size_t i = 0; i < preconditions.size(); ++i){
            if (!preconditions[i].is_applicable(state)){
                return false;
            }
        }
    }        
    return true;
}

bool Operator::is_applicable(LeafStateHash id, LeafFactorID factor) const {
    OpsLeafProps<Condition> preconditions = get_preconditions(factor);
    if (preconditions.empty()){
        return true;
    }
    LeafState state = g_state_registry->lookup_leaf_state(id, factor);
    for (const Condition &cond : preconditions){
        if (!cond.is_applicable(state)){
            return false;
        }
    }
    return true;
}

bool Operator::is_center_applicable(const State &state) const {
    OpsLeafProps<Condition> center_preconds = get_preconditions(LeafFactorID::CENTER);
    for (const Condition &cond : center_preconds){
        if (!cond.is_applicable(state)){
            return false;
        }
    }
    return true;
}

void Operator::dump() const {
    cout << name << ":";
    for (size_t i = 0; i < preconditions.size(); ++i) {
        cout << " [";
        preconditions[i].dump();
        cout << "]";
    }
    for (size_t i = 0; i < effects.size(); ++i) {
        cout << " [";
        effects[i].dump();
        cout << "]";
    }
    cout << endl;
    if (g_factoring){
        factor_info->dump();
    }
}


class SingleFactorInfo: public FactorInfo {
    // operator is only preconditioned by and affects a single factor
    const vector<Condition> &preconditions;
    const vector<Effect> &effects;
public:
    SingleFactorInfo(LeafFactorID affected_factor,
                     const vector<Condition> &preconditions,
                     const vector<Effect> &effects) :
                        FactorInfo(affected_factor),
                        preconditions(preconditions),
                        effects(effects) {
    }

    size_t get_number_leaf_pre_factors() const override {
        if (affected_factor == LeafFactorID::CENTER || preconditions.empty()){
            return 0;
        }
        return 1;
    }

    OpsLeafProps<Condition> get_preconditions(LeafFactorID factor) const override {
        if (factor != affected_factor){
            return OpsLeafProps<Condition>(preconditions.begin(), preconditions.begin());
        }
        return OpsLeafProps<Condition>(preconditions.begin(), preconditions.end());
    }

    OpsLeafProps<Effect> get_effects(LeafFactorID factor) const override {
        if (factor != affected_factor){
            return OpsLeafProps<Effect>(effects.begin(), effects.begin());
        }
        return OpsLeafProps<Effect>(effects.begin(), effects.end());
    }

    bool has_precondition_on(LeafFactorID factor) const override {
        return factor == affected_factor && !preconditions.empty();
    }
    bool has_effect_on(LeafFactorID factor) const override {
        return factor == affected_factor && !effects.empty();
    }
    void dump() const override {
        cout << "SingleFactorInfo" << endl;
    }
};

class LeafOpCenterLeafPreFactorInfo: public FactorInfo {
    // leaf operator that is preconditioned by CENTER and (possibly) by affected_factor
    // effects only on affected_factor
    const vector<Condition>::const_iterator center_pre;
    const vector<Condition>::const_iterator leaf_pre;
    const vector<Condition>::const_iterator end_pre;
    const vector<Effect> &effects;
public:
    LeafOpCenterLeafPreFactorInfo(LeafFactorID affected_factor,
            const vector<Condition>::const_iterator center_pre,
            const vector<Condition>::const_iterator leaf_pre,
            const vector<Condition>::const_iterator end_pre,
            const vector<Effect> &effects) :
                        FactorInfo(affected_factor),
                        center_pre(center_pre),
                        leaf_pre(leaf_pre),
                        end_pre(end_pre),
                        effects(effects) {
        assert(affected_factor != LeafFactorID::CENTER);
    }

    size_t get_number_leaf_pre_factors() const override {
        if (leaf_pre == end_pre){
            return 0;
        }
        return 1;
    }

    OpsLeafProps<Condition> get_preconditions(LeafFactorID factor) const override {
        if (factor == affected_factor){
            return OpsLeafProps<Condition>(leaf_pre, end_pre);
        }
        if (factor == LeafFactorID::CENTER){
            return OpsLeafProps<Condition>(center_pre, leaf_pre);
        }
        return OpsLeafProps<Condition>(center_pre, center_pre);
    }

    OpsLeafProps<Effect> get_effects(LeafFactorID factor) const override {
        if (factor != affected_factor){
            return OpsLeafProps<Effect>(effects.begin(), effects.begin());
        }
        return OpsLeafProps<Effect>(effects.begin(), effects.end());
    }

    bool has_precondition_on(LeafFactorID factor) const override {
        return factor == LeafFactorID::CENTER || (factor == affected_factor && leaf_pre != end_pre);
    }
    bool has_effect_on(LeafFactorID factor) const override {
        return factor == affected_factor && !effects.empty();
    }
    void dump() const override {
        cout << "LeafOpCenterLeafPreFactorInfo, factor " << affected_factor << endl;
    }
};

class CenterOpLeafPreFactorInfo: public FactorInfo {
    // center operator that is preconditioned by pre_factor and (possibly) by CENTER
    // effects only on CENTER
    LeafFactorID pre_factor;
    const vector<Condition>::const_iterator center_pre;
    const vector<Condition>::const_iterator leaf_pre;
    const vector<Condition>::const_iterator end_pre;
    const vector<Effect> &effects;
public:
    CenterOpLeafPreFactorInfo(LeafFactorID pre_factor,
            const vector<Condition>::const_iterator center_pre,
            const vector<Condition>::const_iterator leaf_pre,
            const vector<Condition>::const_iterator end_pre,
            const vector<Effect> &effects) :
                        FactorInfo(LeafFactorID::CENTER),
                        pre_factor(pre_factor),
                        center_pre(center_pre),
                        leaf_pre(leaf_pre),
                        end_pre(end_pre),
                        effects(effects) {
            assert(affected_factor == LeafFactorID::CENTER);
            assert(pre_factor != LeafFactorID::CENTER);
            assert(leaf_pre != end_pre);
    }

    size_t get_number_leaf_pre_factors() const override {
        return 1;
    }

    OpsLeafProps<Condition> get_preconditions(LeafFactorID factor) const override {
        if (factor == pre_factor){
            return OpsLeafProps<Condition>(leaf_pre, end_pre);
        }
        if (factor == LeafFactorID::CENTER){
            return OpsLeafProps<Condition>(center_pre, leaf_pre);
        }
        return OpsLeafProps<Condition>(center_pre, center_pre);
    }

    OpsLeafProps<Effect> get_effects(LeafFactorID factor) const override {
        if (factor != LeafFactorID::CENTER){
            return OpsLeafProps<Effect>(effects.begin(), effects.begin());
        }
        return OpsLeafProps<Effect>(effects.begin(), effects.end());
    }

    bool has_precondition_on(LeafFactorID factor) const override {
        return factor == pre_factor || (factor == LeafFactorID::CENTER && center_pre != leaf_pre);
    }
    bool has_effect_on(LeafFactorID factor) const override {
        return factor == LeafFactorID::CENTER && !effects.empty();
    }
    void dump() const override {
        cout << "CenterOpLeafPreFactorInfo, pre-factor " << pre_factor << endl;
    }
};

class GeneralFactorInfo: public FactorInfo {
    // operator does not fall into any other special case

    // index 0 is center factor, leaves shifted by one;
    // last is end iterator of preconditions/effects of Operator
    vector<vector<Condition>::const_iterator> pre_by_factor;
    vector<vector<Effect>::const_iterator> eff_by_factor;
public:
    GeneralFactorInfo(LeafFactorID affected_factor,
                      vector<vector<Condition>::const_iterator> pre_by_factor,
                      vector<vector<Effect>::const_iterator> eff_by_factor) :
                        FactorInfo(affected_factor),
                        pre_by_factor(pre_by_factor),
                        eff_by_factor(eff_by_factor) {
    }

    size_t get_number_leaf_pre_factors() const override {
        size_t pre_factors = 0;
        for (size_t i = 1; i < pre_by_factor.size() - 1; ++i){
            // if pre_by_factor[i] == pre_by_factor[i+1] => no preconditions on factor i-1
            if (pre_by_factor[i] != pre_by_factor[i+1]){
                ++pre_factors;
            }
        }
        return pre_factors;
    }

    OpsLeafProps<Condition> get_preconditions(LeafFactorID factor) const override {
        if (factor == LeafFactorID::CENTER){
            return OpsLeafProps<Condition>(pre_by_factor[0], pre_by_factor[1]);
        }
        if ((size_t) factor + 1 >= pre_by_factor.size() - 1){ // +1 for center shift; -1 for end() iterator
            return OpsLeafProps<Condition>(pre_by_factor[0], pre_by_factor[0]);
        }
        return OpsLeafProps<Condition>(pre_by_factor[factor + 1], pre_by_factor[factor + 2]);
    }

    OpsLeafProps<Effect> get_effects(LeafFactorID factor) const override {
        if (factor == LeafFactorID::CENTER){
            return OpsLeafProps<Effect>(eff_by_factor[0], eff_by_factor[1]);
        }
        if ((size_t) factor + 1 >= eff_by_factor.size() - 1){ // +1 for center shift; -1 for end() iterator
            return OpsLeafProps<Effect>(eff_by_factor[0], eff_by_factor[0]);
        }
        return OpsLeafProps<Effect>(eff_by_factor[factor + 1], eff_by_factor[factor + 2]);
    }

    bool has_precondition_on(LeafFactorID factor) const override {
        if (factor == LeafFactorID::CENTER){
            return pre_by_factor[0] != pre_by_factor[1];
        }
        if ((size_t) factor + 1 < pre_by_factor.size() - 1){ // +1 for center shift; -1 for end() iterator
            return pre_by_factor[factor + 1] != pre_by_factor[factor + 2];
        }
        return false;
    }
    bool has_effect_on(LeafFactorID factor) const override {
        if (factor == LeafFactorID::CENTER){
            return eff_by_factor[0] != eff_by_factor[1];
        }
        if ((size_t) factor + 1 < eff_by_factor.size() - 1){ // +1 for center shift; -1 for end() iterator
            return eff_by_factor[factor + 1] != eff_by_factor[factor + 2];
        }
        return false;
    }
    void dump() const override {
        cout << "GeneralFactorInfo" << endl;
        for (vector<Condition>::const_iterator it : pre_by_factor){
            cout << (int)(it - *pre_by_factor.cbegin()) << endl;
        }
    }
};

void Operator::set_affected_factor(LeafFactorID affected_factor) {
    std::sort(preconditions.begin(), preconditions.end(),
            [](Condition &a, Condition &b) {
        if (g_belongs_to_factor[a.var] == LeafFactorID::CENTER){
            if (g_belongs_to_factor[b.var] == LeafFactorID::CENTER){
                return a.var < b.var;
            }
            return true;
        } else if (g_belongs_to_factor[b.var] == LeafFactorID::CENTER){
            return false;
        } else if (g_belongs_to_factor[a.var] != g_belongs_to_factor[b.var]){
            return g_belongs_to_factor[a.var] < g_belongs_to_factor[b.var];
        }
        return a.var < b.var;
    });
    std::sort(effects.begin(), effects.end(),
            [](Effect &a, Effect &b) {
        if (g_belongs_to_factor[a.var] == LeafFactorID::CENTER){
            if (g_belongs_to_factor[b.var] == LeafFactorID::CENTER){
                return a.var < b.var;
            }
            return true;
        } else if (g_belongs_to_factor[b.var] == LeafFactorID::CENTER){
            return false;
        } else if (g_belongs_to_factor[a.var] != g_belongs_to_factor[b.var]){
            return g_belongs_to_factor[a.var] < g_belongs_to_factor[b.var];
        }
        return a.var < b.var;
    });

    // TODO implement more special cases
    // e.g. center actions with 1/2/more leaf preconditions
    // center actions with 1/2/more leaf effects

    bool center_pre = !preconditions.empty();
    vector<std::vector<Condition>::const_iterator> pre_by_factor;
    if (affected_factor == LeafFactorID::CENTER && g_factoring->get_profile() != FORK){
        pre_by_factor.reserve(min(preconditions.size() + 1, g_leaves.size() + 2));
    }
    pre_by_factor.push_back(preconditions.cbegin());
    LeafFactorID curr_factor(LeafFactorID::CENTER);
    for (vector<Condition>::const_iterator it = preconditions.cbegin(); it != preconditions.cend(); ++it){
        if (g_belongs_to_factor[it->var] != curr_factor){
            if (curr_factor == LeafFactorID::CENTER){
                pre_by_factor.push_back(it);
                curr_factor = LeafFactorID(0);
                if (it == preconditions.cbegin()){
                    center_pre = false;
                }
            }
            while (curr_factor < g_belongs_to_factor[it->var]){
                pre_by_factor.push_back(it);
                ++curr_factor;
            }
        }
    }
    pre_by_factor.push_back(preconditions.cend());
    pre_by_factor.shrink_to_fit();

    vector<std::vector<Effect>::const_iterator> eff_by_factor;
    if (affected_factor == LeafFactorID::CENTER){
        if (affected_factor == LeafFactorID::CENTER && g_factoring->get_profile() != FORK && g_factoring->get_profile() != IFORK){
            eff_by_factor.reserve(min(effects.size() + 1, g_leaves.size() + 2));
        }
        eff_by_factor.push_back(effects.cbegin());
        curr_factor = LeafFactorID::CENTER;
        for (vector<Effect>::const_iterator it = effects.cbegin(); it != effects.cend(); ++it){
            if (g_belongs_to_factor[it->var] != curr_factor){
                if (curr_factor == LeafFactorID::CENTER){
                    eff_by_factor.push_back(it);
                    curr_factor = LeafFactorID(0);
                }
                while (curr_factor < g_belongs_to_factor[it->var]){
                    eff_by_factor.push_back(it);
                    ++curr_factor;
                }
            }
        }
        eff_by_factor.push_back(effects.cend());
        eff_by_factor.shrink_to_fit();
    }

    if (affected_factor == LeafFactorID::CENTER){
        if (pre_by_factor.size() == 2 && eff_by_factor.size() == 2){
            // not preconditioned by or affects any leaf factor
            factor_info = new SingleFactorInfo(affected_factor, preconditions, effects);
        } else if (pre_by_factor.size() == 3 && eff_by_factor.size() == 2){
            // effects only on center, preconditions on one leaf + possibly center
            vector<Condition>::const_iterator start_leaf_pre = pre_by_factor[0]; // in case there is no center precondition
            for (size_t i = 1; i < pre_by_factor.size() - 1; ++i){
                if (pre_by_factor[i] != pre_by_factor[i+1]){
                    start_leaf_pre = pre_by_factor[i];
                    break;
                }
            }
            // we have to find an iterator in the loop unless there are no center preconditions
            assert(start_leaf_pre != pre_by_factor[0] || pre_by_factor[0] == pre_by_factor[1]);
            factor_info = new CenterOpLeafPreFactorInfo(g_belongs_to_factor[start_leaf_pre->var], pre_by_factor[0], start_leaf_pre, pre_by_factor.back(), effects);
        } else {
            factor_info = new GeneralFactorInfo(affected_factor, pre_by_factor, eff_by_factor);
        }
    } else {
        if (center_pre){
            vector<Condition>::const_iterator start_leaf_pre = pre_by_factor.back(); // in case there is no leaf precondition
            for (size_t i = 1; i < pre_by_factor.size() - 1; ++i){
                if (pre_by_factor[i] != pre_by_factor[i+1]){
                    start_leaf_pre = pre_by_factor[i];
                    break;
                }
            }
            factor_info = new LeafOpCenterLeafPreFactorInfo(affected_factor, pre_by_factor[0], start_leaf_pre, pre_by_factor.back(), effects);
        } else {
            // not preconditioned by or affects center factor
            factor_info = new SingleFactorInfo(affected_factor, preconditions, effects);
        }
    }
}
