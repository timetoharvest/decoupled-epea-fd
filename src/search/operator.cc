#include "operator.h"

#include "globals.h"
#include "factoring.h"
#include "per_state_information.h"
#include "price_tag.h"

#include <iostream>

using namespace std;


Condition::Condition(istream &in) {
    in >> var >> val;
}

bool Condition::is_applicable(const State &state) const {
    assert(var >= 0 && var < g_variable_name.size());
    assert(val >= 0 && val < g_variable_domain[var]);
    assert(!g_use_decoupled_search || g_belongs_to_factor[var] == -1);
    return state[var] == val;
}

bool Condition::is_applicable(const LeafState &state) const {
    assert(var >= 0 && var < g_variable_name.size());
    assert(val >= 0 && val < g_variable_domain[var]);
    assert(g_belongs_to_factor[var] == state.get_id().get_factor());
    return state[var] == val;
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

Operator::Operator(istream &in, bool axiom, int index) {
    marked = false;

    is_an_axiom = axiom;
    if (!is_an_axiom) {
        check_magic(in, "begin_operator");
        in >> ws;
        getline(in, name);
        int count;
        in >> count;
        for (int i = 0; i < count; i++)
            preconditions.push_back(Condition(in));
        in >> count;
        for (int i = 0; i < count; i++)
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
    
    affects_factor = -1;	// -1 => center factor
    number_pre_factors = 0;
    
    this->index = index;
}

void Condition::dump() const {
    cout << g_fact_names[var][val];
}

void Effect::dump() const {
    cout << " => " << g_fact_names[var][val];
    if (!cond.empty()) {
        cout << " if";
        for (int i = 0; i < cond.size(); i++) {
            cout << " ";
            cond[i].dump();
        }
    }
}


bool Operator::is_applicable(const State& state) const {
    if (g_use_decoupled_search){    // not really efficient, but how to do it better?
        if (!is_center_applicable(state)){
            return false;
        }
        if (g_factoring->get_profile() == FORK && affects_factor == -1){
            // in fork topology, center actions don't have leaf preconditions
            return true;
        }
        const PriceTag &price_tag = g_price_tags[state];
        for (int factor = 0; factor < g_factors.size(); factor++){
            if (!pre_by_factor[factor + 1].empty()){
                bool applicable = false;
                int number_facts = price_tag.get_number_facts(factor);
                int id = 0;
                while (number_facts > 0){
                    if (price_tag.has_leaf_state(id, factor)){
                        number_facts--;
                        if (is_applicable(LeafStateID(id, factor))){
                            applicable = true;
                            break;
                        }
                    }
                    id++;
                }
                if (!applicable){
                    return false;
                }
            }
        }
    } else {
        for (int i = 0; i < preconditions.size(); i++){
            if (!preconditions[i].is_applicable(state)){
                return false;
            }
        }
    }        
    return true;
}

bool Operator::is_applicable(const LeafStateID &leaf_id) const {
    int factor = leaf_id.get_factor() + 1;
    if (pre_by_factor[factor].empty()){
        return true;
    }
    LeafState state = g_state_registry->lookup_leaf_state(leaf_id);
    for (int i = 0; i < pre_by_factor[factor].size(); i++){
        if (!pre_by_factor[factor][i]->is_applicable(state)){
            return false;
        }
    }
    return true;
}
    
void Operator::set_affected_factor(int factor) {
    affects_factor = factor;

    pre_by_factor.resize(g_factors.size() + 1);    
    for (int i = 0; i < preconditions.size(); i++){
        int f = g_belongs_to_factor[preconditions[i].var];
        pre_by_factor[f + 1].push_back(&preconditions[i]);
    }
    
    eff_by_factor.resize(g_factors.size() + 1);
    for (int i = 0; i < effects.size(); i++){
        int f = g_belongs_to_factor[effects[i].var];
        eff_by_factor[f + 1].push_back(&effects[i]);
    }
    
    for (int i = 1; i < pre_by_factor.size(); i++){
        if (!pre_by_factor[i].empty()){
            number_pre_factors++;
        }
    }
}

void Operator::dump() const {
    cout << name << ":";
    for (int i = 0; i < preconditions.size(); i++) {
        cout << " [";
        preconditions[i].dump();
        cout << "]";
    }
    for (int i = 0; i < effects.size(); i++) {
        cout << " [";
        effects[i].dump();
        cout << "]";
    }
    cout << endl;
}
