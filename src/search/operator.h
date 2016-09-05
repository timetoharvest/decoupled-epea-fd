#ifndef OPERATOR_H
#define OPERATOR_H

#include "globals.h"
#include "leaf_state.h"
#include "state.h"
#include "utilities.h"

#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

template<class T>
class OpsLeafProps {
    
    const typename std::vector<T>::iterator begin;
    const typename std::vector<T>::iterator end;
    
public:
    OpsLeafProps(typename std::vector<T>::iterator begin, typename std::vector<T>::iterator end)
        : begin(begin), end(end) {}
    
    size_t size() const {
        return end - begin;
    }
    
    const T& operator[](size_t index) const {
        return *(begin + index);
    }
    
};

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
    
    // in decoupled search, the following two are sorted by factor, center first
    std::vector<Condition> preconditions;      // var, val
    std::vector<Effect> effects;     // var, val, effect conditions
    
    // 0 is center factors, leaves shifted by for center actions 
    // 1 is affects_factor for leaf actions
    std::vector<std::vector<Condition>::iterator > pre_by_factor;
    std::vector<std::vector<Effect>::iterator > eff_by_factor;
    
    std::string name;
    int cost;

    // the following two are only needed for decoupled search
    int affects_factor;

    mutable bool marked; // Used for short-term marking of preferred operators
    
    bool dead;

    void read_pre_post(std::istream &in);
    
public:
    explicit Operator(std::istream &in, bool is_axiom);
    
    void dump() const;
    
    std::string get_name() const {
        return name;
    }

    bool is_axiom() const {
        return is_an_axiom;
    }
    
    const std::vector<Condition> &get_preconditions() const {
        return preconditions;
    }
    
    const std::vector<Effect> &get_effects() const {
        return effects;
    }

    void set_dead() {
        dead = true;
    }

    bool is_dead() const {
        return dead;
    }

    const OpsLeafProps<Condition> get_preconditions(int factor) const;
    
    const OpsLeafProps<Effect> get_effects(int factor) const;
    
    bool is_applicable(const State &state) const;
    
    bool is_applicable(const LeafStateID &leaf_id) const;
    
    bool is_center_applicable(const State &state) const;

    bool is_marked() const {
        return marked;
    }
    
    void mark() const {
        marked = true;
    }
    
    void unmark() const {
        marked = false;
    }
    
    size_t get_index() const {
        int op_index = this - &*g_operators.begin();
        assert(op_index >= 0 && (size_t) op_index < g_operators.size());
        return op_index;
    }
    
    void set_affected_factor(int factor);
    
    int get_affected_factor() const {
        return affects_factor;
    }
    
    bool has_precondition_on(int factor) const {
        if (affects_factor == -1){
            return !pre_by_factor[factor + 1].empty();
        }
        return affects_factor == factor && !pre_by_factor[1].empty();
    }
    
    int get_number_pre_factors() const {
        if (affects_factor == -1){
            size_t num = 0;
            for (size_t i = 1; i < pre_by_factor.size(); ++i){
                if (!pre_by_factor[i].empty()){
                    ++num;
                }
            }
            return num;
        }
        return 1;
    }

    int get_cost() const {
        return cost;
    }
};

#endif