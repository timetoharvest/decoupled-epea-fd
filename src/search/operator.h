#ifndef OPERATOR_H
#define OPERATOR_H

#include "globals.h"
#include "leaf_state.h"
#include "state.h"
#include "utilities.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

struct Condition {
    int var;
    int val;
    explicit Condition(std::istream &in);
    Condition(int v, int p) : var(v), val(p) {
        assert(in_bounds(var, g_variable_name));
        assert(val >= 0 && val < g_variable_domain[var]);
    }

    bool is_applicable(const State &state) const;
    
    bool is_applicable(const LeafState &state) const;

    bool operator==(const Condition &other) const {
        return var == other.var && val == other.val;
    }

    bool operator!=(const Condition &other) const {
        return !(*this == other);
    }

    void dump() const;
};

struct Effect {
    int var;
    int val;
    std::vector<Condition> conditions;
    Effect(int var, int val, const std::vector<Condition> &co)
        : var(var), val(val), conditions(co) {}

    bool does_fire(const State &state) const {
        for (size_t i = 0; i < conditions.size(); ++i){
            if (!conditions[i].is_applicable(state)){
                return false;
            }
        }
        return true;
    }

    void dump() const;
};

class Operator {
    bool is_an_axiom;
    std::vector<Condition> preconditions;      // var, val
    std::vector<Effect> effects;     // var, val, effect conditions
    std::vector<std::vector<const Condition*> > pre_by_factor;
    std::vector<std::vector<const Effect*> > eff_by_factor;
    std::string name;
    int cost;

    // the following three are only needed for decoupled search
    int affects_factor;
    // number of leaf factors that this center (!) op is preconditioned by
    size_t number_pre_factors;
    size_t index;

    mutable bool marked; // Used for short-term marking of preferred operators
    
    void read_pre_post(std::istream &in);
public:
    explicit Operator(std::istream &in, bool is_axiom, int index);
    void dump() const;
    std::string get_name() const {return name; }

    bool is_axiom() const {return is_an_axiom; }
    
    const std::vector<Condition> &get_preconditions() const {
        return preconditions;
    }
    
    const std::vector<Effect> &get_effects() const {
        return effects;
    }

    const std::vector<const Condition*> &get_preconditions(int factor) const {
        return pre_by_factor[factor + 1];
    }
    const std::vector<const Effect*> &get_effects(int factor) const {
        return eff_by_factor[factor + 1];
    }

    bool is_applicable(const State &state) const;
    
    bool is_applicable(const LeafStateID &leaf_id) const;
    
    bool is_center_applicable(const State &state) const {
        for (size_t i = 0; i < pre_by_factor[0].size(); i++){
            if (!pre_by_factor[0][i]->is_applicable(state)){
                return false;
            }
        }
        return true;
    }

    bool is_marked() const {
        return marked;
    }
    void mark() const {
        marked = true;
    }
    void unmark() const {
        marked = false;
    }
    
    int get_index() const {
        return index;
    }
    
    void set_affected_factor(int factor);
    
    int get_affected_factor() const {
        return affects_factor;
    }
    
    bool has_preconditions_on_factor(int factor) const {
        return !pre_by_factor[factor + 1].empty();
    }
    
    int get_number_pre_factors() const {
        return number_pre_factors;
    }

    int get_cost() const {return cost; }
};

#endif
