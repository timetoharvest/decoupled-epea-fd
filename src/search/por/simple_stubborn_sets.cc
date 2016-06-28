#include "simple_stubborn_sets.h"

#include "../compliant_paths/compliant_path_graph.h"
#include "../compliant_paths/cpg_storage.h"
#include "../globals.h"
#include "../operator.h"
#include "../option_parser.h"
#include "../rng.h"
#include "../state_registry.h"
#include "../successor_generator.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <list>
#include <algorithm>
using namespace std;

namespace POR {
std::vector<std::vector<Fact> > op_preconds;
std::vector<std::vector<Fact> > op_effects;

// Implementation of simple instantiation of strong stubborn sets.
// Disjunctive action landmarks are computed trivially.
//
//
// TODO: get_op_index belongs to a central place.
// We currently have copies of it in different parts of the code.
//
//
// Internal representation of operator preconditions.
// Only used during computation of interference relation.
// op_preconds[i] contains precondition facts of the i-th
// operator.
//
//
// Internal representation of operator effects.
// Only used during computation of interference relation.
// op_effects[i] contains effect facts of the i-th
// operator.

static inline int get_op_index(const Operator *op) {
    int op_index = op - &*g_operators.begin();
    assert(op_index >= 0 && (size_t) op_index < g_operators.size());
    return op_index;
}

inline bool operator<(const Operator &lhs, const Operator &rhs) {
    return get_op_index(&lhs) < get_op_index(&rhs);
}

static inline bool can_disable(int op1_no, int op2_no) {
    size_t i = 0;
    size_t j = 0;
    while (i < op_preconds[op2_no].size() && j < op_effects[op1_no].size()) {
        int read_var = op_preconds[op2_no][i].var;
        int write_var = op_effects[op1_no][j].var;
        if (read_var < write_var){
            i++;
        } else if (read_var == write_var) {
            int read_value = op_preconds[op2_no][i].val;
            int write_value = op_effects[op1_no][j].val;
            if (read_value != write_value)
                return true;
            i++;
            j++;
        } else {
            // read_var > write_var
            j++;
        }
    }
    if (g_use_decoupled_search && g_factoring->get_profile() != FORK){
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            // preconditions are not guaranteed to be sorted by variable index
            const vector<const Condition*> &op1_pre = g_operators[op1_no].get_preconditions(factor);
            const vector<const Condition*> &op2_pre = g_operators[op2_no].get_preconditions(factor);
            if (!op1_pre.empty() && !op2_pre.empty()){
                vector<bool> handled(op2_pre.size(), false);
                for (const auto &pre1 : op1_pre){
                    bool found = false;
                    for (size_t var2 = 0; var2 < op2_pre.size(); ++var2){
                        const Condition *pre2 = op2_pre[var2];
                        if (pre1->var == pre2->var){
                            handled[var2] = true;
                            if (pre1->val != pre2->val){
                                return true;
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found){
                        return true;
                    }
                }
                for (size_t var2 = 0; var2 < op2_pre.size(); ++var2){
                    if (handled[var2]){
                        continue;
                    }
                    const Condition *pre2 = op2_pre[var2];
                    bool found = false;
                    for (const auto &pre1 : op1_pre){
                        if (pre1->var == pre2->var){
                            if (pre1->val != pre2->val){
                                return true;
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found){
                        return true;
                    }
                }
            }
        }
    }
    return false;
}


static inline bool can_conflict(int op1_no, int op2_no) {
    size_t i = 0;
    size_t j = 0;
    while (i < op_effects[op1_no].size() && j < op_effects[op2_no].size()) {
        int var1 = op_effects[op1_no][i].var;
        int var2 = op_effects[op2_no][j].var;
        if (var1 < var2)
            i++;
        else if (var1 == var2) {
            int value1 = op_effects[op1_no][i].val;
            int value2 = op_effects[op2_no][j].val;
            if (value1 != value2)
                return true;
            i++;
            j++;
        } else {
            // var1 > var2
            j++;
        }
    }
    return false;
}

static inline bool mutex_precond(int op1_no, int op2_no) {

    for (size_t i = 0; i < op_preconds[op1_no].size(); i++) {

	 int var1 = op_preconds[op1_no][i].var;
	 int val1 = op_preconds[op1_no][i].val;
	
	for (size_t j = 0; j < op_preconds[op2_no].size(); j++) {
	    
	    int var2 = op_preconds[op2_no][j].var;
	    int val2 = op_preconds[op2_no][j].val;
	    
	    if (are_mutex(make_pair(var1,val1), make_pair(var2, val2)))
		return true;
	}
    }
    
    return false;

}

static inline bool preserved(pair<pair<int, int>, pair<int, int> >& disj, pair<pair<int, int>, pair<int, int> >& C, int op_no) {

    // check if op_no is applicable in a state satisfying C
    for (size_t i = 0; i < op_preconds[op_no].size(); i++) {
	int var = op_preconds[op_no][i].var;
	int val = op_preconds[op_no][i].val;
	
	if ((C.first.first == var && C.first.second != val) || (C.second.first == var && C.second.second != val)) {
	    return true; // o is not applicable in C, so o cannot possibly negate disj
	}
    }

    // o applicable in C; check if o can possibly negate disj
    for (size_t i = 0; i < op_effects[op_no].size(); i++) {
	int eff_var = op_effects[op_no][i].var;
	int eff_val = op_effects[op_no][i].val;
	
	bool OK = false;
	if (disj.first.first == eff_var && disj.first.second != eff_val) {
	    // o negates first literal => second literal must become true or be true and not become false
	    for (size_t j = 0; j < op_effects[op_no].size() && !OK; j++) {
		int eff_var2 = op_effects[op_no][j].var;
		int eff_val2 = op_effects[op_no][j].val;
		
		if (eff_var2 == disj.second.first && eff_val2 == disj.second.second) {
		    OK = true;
		}
	    }
	    
	    if (!OK) {
		if ((disj.second.first == C.first.first && disj.second.second == C.first.second) || 
		    (disj.second.first == C.second.first && disj.second.second == C.second.second)) {

		    //second literal true in C => check that o does not falsify it
		    bool falsifies = false;
		    for (size_t j = 0; j < op_effects.size() && !falsifies; j++) {
			int eff_var2 = op_effects[op_no][j].var;
			int eff_val2 = op_effects[op_no][j].val;
			if (eff_var2 == disj.second.first && eff_val2 != disj.second.second) {
			    falsifies = true;
			}
		    }
		    if (!falsifies) {
			OK = true;
		    }
		}
	    }
	}
	if (!OK) {
	    return false;
	}
	
	if (disj.second.first == eff_var && disj.second.second != eff_val) {
	    // o negates second literal => first literal must become true or be true and not become false
	    for (size_t j = 0; j < op_effects[op_no].size() && !OK; j++) {
		int eff_var2 = op_effects[op_no][j].var;
		int eff_val2 = op_effects[op_no][j].val;
		
		if (eff_var2 == disj.first.first && eff_val2 == disj.first.second) {
		    OK = true;
		}
	    }
	    
	    if (!OK) {
		if ((disj.first.first == C.first.first && disj.first.second == C.first.second) || 
		    (disj.first.first == C.second.first && disj.first.second == C.second.second)) {

		    //first literal true in C => check that o does not falsify it
		    bool falsifies = false;
		    for (size_t j = 0; j < op_effects.size() && !falsifies; j++) {
			int eff_var2 = op_effects[op_no][j].var;
			int eff_val2 = op_effects[op_no][j].val;
			if (eff_var2 == disj.first.first && eff_val2 != disj.first.second) {
			    falsifies = true;
			}
		    }
		    if (!falsifies) {
			OK = true;
		    }
		}
	    }
	}
	if (!OK) {
	    return false;
	}
    }

    return true;
} 


inline bool SimpleStubbornSets::interfere(int op1_no, int op2_no) {
    
    switch (mutex_option) {
    case 0:
	return (can_disable(op1_no, op2_no) || can_conflict(op1_no, op2_no) || can_disable(op2_no, op1_no));
    case 1:
	return !mutex_precond(op1_no, op2_no) && (can_disable(op1_no, op2_no) || can_conflict(op1_no, op2_no) || can_disable(op2_no, op1_no));
    default:
	assert(false);
	exit(2);
    }
}


pair<int, int> SimpleStubbornSets::find_unsatisfied_goal_small_dynamic(const State &state) { 

    int best_goal_var = -1;
    int best_goal_val = -1;
    int number = -1;
    
    for (size_t i = 0; i < g_goal.size(); ++i) {
        int goal_var = g_goal[i].first;
        int goal_value = g_goal[i].second;
        if (state[goal_var] != goal_value) {
	    const vector<int> &op_nos = achievers[goal_var][goal_value];
	    if (number == -1 || (int) op_nos.size() < number) {// TODO fix this
		number = op_nos.size();
		best_goal_var = goal_var;
		best_goal_val = goal_value;
	    }
	}
    }
    
    return make_pair(best_goal_var, best_goal_val);
    
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_goal_small_static(const State &state) { 

    // return first unsatisfied goal fact according to the precomputed
    // goal ordering
    for (size_t i = 0; i < goal_ordering.size(); ++i) {
        int var = goal_ordering[i].first;
        int value = goal_ordering[i].second;
        if (state[var] != value)
	    return make_pair(var, value);
    }

    // this should not be reachable
    assert(false);
    return make_pair(-1, -1);

}

pair<int, int> SimpleStubbornSets::find_unsatisfied_goal_random_dynamic(const State &state) { 

    int best_goal_var = -1;
    int best_goal_val = -1;
    double number = -1;
    
    for (size_t i = 0; i < g_goal.size(); ++i) {
        int goal_var = g_goal[i].first;
        int goal_value = g_goal[i].second;
        if (state[goal_var] != goal_value) {
            double random_value = g_rng();
	    if (number == -1 || random_value < number) {
		number = random_value;
		best_goal_var = goal_var;
		best_goal_val = goal_value;
	    }
	}
    }
    
    return make_pair(best_goal_var, best_goal_val);
    
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_goal_decoupled(const State& state) {
    const CompliantPathGraph *tag = CPGStorage::get_cpg(state);
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        // factor has a goal that is not reachable
        if (!g_goals_per_factor[factor].empty() && !tag->goal_reachable(factor)){
            int number_open_goals = g_goals_per_factor[factor].size();
            vector<bool> is_open_goal(number_open_goals, true); // false => is fulfilled by some leaf state
            int number_unsatisfied_goals = 0;
            vector<bool> is_unsatisfied_goal(number_open_goals, false); // true => is not fulfilled by some leaf state
        
            size_t number_states = tag->get_number_states(factor);
            size_t id = 0;
            while (number_states > 0){
                if (tag->has_leaf_state(id, factor)){
                    --number_states;
                    LeafState s = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
                    for (size_t i = 0; i < g_goals_per_factor[factor].size(); ++i){
                        int var = g_goals_per_factor[factor][i].first;
                        int val = g_goals_per_factor[factor][i].second;
                        if (s[var] == val){
                            if (is_open_goal[i]){
                                --number_open_goals;
                            }
                            is_open_goal[i] = false;
                        } else {
                            if (!is_unsatisfied_goal[i]){
                                ++number_unsatisfied_goals;
                            }
                            is_unsatisfied_goal[i] = true;
                        }
                    }
                }
                ++id;
            }
            if (number_open_goals > 0){
                for (size_t i = 0; i < is_open_goal.size(); ++i){
                    if (is_open_goal[i]){
                        return g_goals_per_factor[factor][i];
                    }
                }
                assert(false);
            }
                        
            // if multi-variable leaf factors, need to check if there is a goal
            // that is not satisfied by any leaf state; if not, need to achieve all
            // goals that *ever* were unsatisfied
            assert(number_unsatisfied_goals > 0);
            
            pair<int, int> first(-1, -1);
            for (size_t i = 0; i < g_goals_per_factor[factor].size(); ++i){
                if (is_unsatisfied_goal[i]){
                    if (first.first != -1){
                        add_nes_for_fact(g_goals_per_factor[factor][i]);
                    } else {
                        first = g_goals_per_factor[factor][i];
                    }
                }
            }
        
            return first;
        }
    }
    
    // handle center goals
    for (size_t i = 0; i < g_goal.size(); ++i) {
        int goal_var = g_goal[i].first;
        int goal_value = g_goal[i].second;
        if (g_belongs_to_factor[goal_var] == -1 && state[goal_var] != goal_value) {
            return make_pair(goal_var, goal_value);
        }
    }
    return make_pair(-1, -1);
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_goal_global_variable_ordering(const State& state) {

    
    int best_goal_var = -1;
    int best_goal_val = -1;
    int best_achievers = -1;
    
    for (size_t i = 0; i < g_goal.size(); ++i) {
        int goal_var = g_goal[i].first;
        int goal_value = g_goal[i].second;
        if (state[goal_var] != goal_value) {

	    int achievers = global_variable_ordering[goal_var];

	    if (achievers < best_achievers || best_achievers == -1) {
		best_goal_var = goal_var;
		best_goal_val = goal_value;
		best_achievers = achievers;
	    }
	}
    }

    // 	    for (int j = 0; j < global_variable_ordering.size() && (j < index || index == -1); j++) {
    // 		if (global_variable_ordering[j].first == goal_var) {
    // 		    best_goal_var = goal_var;
    // 		    best_goal_val = goal_value;
    // 		    index = j;
    // 		}
    // 	    }
    // 	}
    // }
    
    return make_pair(best_goal_var, best_goal_val);

}

pair<int, int> SimpleStubbornSets::find_unsatisfied_goal_most_hitting(const State &state) { 

    int best_goal_var = -1;
    int best_goal_val = -1;
    int number = -1; // counts the *additional* unsatisfied goal facts
		     // that are achieved by candidate achiever set
    
    for (size_t i = 0; i < g_goal.size(); ++i) {
        int goal_var = g_goal[i].first;
        int goal_value = g_goal[i].second;
        if (state[goal_var] != goal_value) {
	    const vector<int> &op_nos = achievers[goal_var][goal_value];
	    if (number == -1) {
		number = op_nos.size();
		best_goal_var = goal_var;
		best_goal_val = goal_value;
	    }
	    else {
		// for each achiever, count the number of
		// additional achieved unsatisfied goal facts
		int counter = 0;
		for (size_t j = 0; j < op_nos.size(); j++) {
		    for (size_t k = 0; k < op_effects[op_nos[j]].size(); k++) {
			for (size_t l = 0; l < g_goal.size(); l++) {
			    if (op_effects[op_nos[j]][k].var == g_goal[l].first && 
				op_effects[op_nos[j]][k].val == g_goal[l].second &&
				state[g_goal[l].first] != g_goal[l].second &&
				l != i) {
				counter++;
			    }
			}
		    }
		}
		
		if (counter > number) {
		    number = counter;
		    best_goal_var = goal_var;
		    best_goal_val = goal_value;
		}
	    }
	}
    }
    
    return make_pair(best_goal_var, best_goal_val);

}


// Return the first unsatified goal pair
static inline pair<int, int> find_unsatisfied_goal_forward(const State &state) {

   for (size_t i = 0; i < g_goal.size(); ++i) {
        int goal_var = g_goal[i].first;
        int goal_value = g_goal[i].second;
        if (state[goal_var] != goal_value)
            return make_pair(goal_var, goal_value);
    }
    
    // this should not be reachable if we are not yet in a goal state
    assert(false);
    return make_pair(-1, -1);
    
}

// Return the first unsatified goal pair
static inline pair<int, int> find_unsatisfied_goal_backward(const State &state) {

    for (int i = g_goal.size()-1; i >= 0; --i) {
        int goal_var = g_goal[i].first;
        int goal_value = g_goal[i].second;
        if (state[goal_var] != goal_value)
            return make_pair(goal_var, goal_value);
    }
    
    // this should not be reachable if we are not yet at in goal state
    assert(false);
    return make_pair(-1, -1);
    
}


pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition_small_static(const Operator &op, 
									      const State &state) {
    // precondition facts of op in precomputed ordering
    vector<pair<int, int> > facts = ordering_map[&op];
    
    // return first unsatisfied precondition according to this
    // ordering
    for (size_t i = 0; i < facts.size(); ++i) {
        int var = facts[i].first;
        int value = facts[i].second;
        if (state[var] != value)
	    return make_pair(var, value);
    }

    // this should not be reachable
    assert(false);
    return make_pair(-1, -1);

}

pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition_dynamic(const Operator &op, 
									 const State &state) {

    vector<pair<int, int> > precondition_facts;

    const vector<Condition> &preconds = op.get_preconditions();
    for (size_t i = 0; i < preconds.size(); ++i) {
	precondition_facts.push_back(make_pair(preconds[i].var, preconds[i].val));
    }
    
    switch (precond_choice) {
    case 3: // small_dynamic
	return find_unsatisfied_precondition_small_dynamic(state, precondition_facts);
    case 4: // laarman heuristic
	return find_unsatisfied_precondition_laarman(state, precondition_facts);
    case 5: // most hitting
	return find_unsatisfied_precondition_most_hitting(state, precondition_facts);
    case 8: // random_dynamic
	return find_unsatisfied_precondition_random_dynamic(state, precondition_facts);
    default:
	cout << "ERROR while selecting precondition variable";
	exit(2);
    }
    
    assert(false);
    return make_pair(-1, -1);
}

// Return the first unsatified precondition, or (-1, -1) if there is none.
static inline pair<int, int> find_unsatisfied_precondition_forward(const Operator &op, 
								   const State &state) {
       
    const vector<Condition> &preconds = op.get_preconditions();
    for (size_t i = 0; i < preconds.size(); ++i) {
        int var = preconds[i].var;
        int value = preconds[i].val;
        if (state[var] != value){
            return make_pair(var, value);
        }
    }

    return make_pair(-1, -1);
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_leaf_precondition(const Operator& op, const State& state, int factor) {
    const vector<const Condition*> &preconds = op.get_preconditions(factor);
    
    if (preconds.empty()){
        return make_pair(-1, -1);
    }
        
    size_t number_open_preconds = preconds.size();
    vector<bool> is_open_precond(number_open_preconds, true); // false => is fulfilled by some leaf state
    size_t number_unsatisfied_preconds = 0;
    vector<bool> is_unsatisfied_precond(number_open_preconds, false); // true => is not fulfilled by some leaf state
    
    bool is_applicable = false;
    
    const CompliantPathGraph *tag = CPGStorage::get_cpg(state);
    size_t number_states = tag->get_number_states(factor);
    size_t id = 0;
    while (number_states > 0){
        if (tag->has_leaf_state(id, factor)){
            --number_states;
            LeafState s = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
            bool applicable_state = true;
            for (size_t i = 0; i < preconds.size(); ++i) {
                int var = preconds[i]->var;
                int value = preconds[i]->val;
                if (s[var] == value){
                    if (is_open_precond[i]){
                        --number_open_preconds;
                    }
                    is_open_precond[i] = false;
                } else {
                    if (!is_unsatisfied_precond[i]){
                        ++number_unsatisfied_preconds;
                    }
                    is_unsatisfied_precond[i] = true;
                    applicable_state = false;
                }
            }
            if (applicable_state){
                is_applicable = true;
            }
        }
        ++id;
    }
    if (number_open_preconds > 0){
        for (size_t i = 0; i < is_open_precond.size(); ++i){
            if (is_open_precond[i]){
                return make_pair(preconds[i]->var, preconds[i]->val);
            }
        }
        assert(false);
    }
    
    assert(number_unsatisfied_preconds > 0 || is_applicable);
    
    if (number_unsatisfied_preconds > 0 && !is_applicable){
        // if multi-variable leaf factors, need to check if there is a precondition
        // that is not satisfied by any leaf state; if not, need to achieve all
        // preconditions that *ever* were unsatisfied
        
        pair<int, int> first(-1, -1);
        for (size_t i = 0; i < preconds.size(); ++i){
            if (is_unsatisfied_precond[i]){
                if (first.first != -1){
                    add_nes_for_fact(make_pair(preconds[i]->var, preconds[i]->val));
                } else {
                    first = make_pair(preconds[i]->var, preconds[i]->val);
                }
            }
        }
        
        return first;
    }
    
    return make_pair(-1, -1);
}

// Return the first unsatified precondition, or (-1, -1) if there is none.
pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition_decoupled(const Operator &op, 
                                                                   const State &state) {
    
    if (op.get_affected_factor() != -1){
        pair<int, int> open_pre = find_unsatisfied_leaf_precondition(op, state, op.get_affected_factor());
        
        if (open_pre.first != -1){
            return open_pre;
        }        
    } else if (g_factoring->get_profile() != FORK){
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            pair<int, int> open_pre = find_unsatisfied_leaf_precondition(op, state, factor);
        
            if (open_pre.first != -1){
                return open_pre;
            }
        }
    }
    
    const vector<const Condition*> &preconds = op.get_preconditions(-1);
    for (size_t i = 0; i < preconds.size(); ++i) {
        int var = preconds[i]->var;
        int value = preconds[i]->val;
        if (state[var] != value){
            return make_pair(var, value);
        }
    }
    
    assert(op.is_applicable(state));

    return make_pair(-1, -1);
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition_forward_small(const Operator &op, 
                                                                                const State &state) {

    // combines "forward" and "small_dynamic": as long as the same
    // precondition fact as before is unsatisfied, chose this
    // precondition fact; if a new precondition fact has to be chosen,
    // chose such a precondition fact that minimizes the NES

    int next_var = -1;
    int next_val = -1;

    const vector<Condition> &preconds = op.get_preconditions();
    for (size_t i = 0; i < preconds.size(); ++i) {
        int var = preconds[i].var;
        int value = preconds[i].val;
        if (state[var] != value) {
	    next_var = var;
	    next_val = value;
	}
    }

    int last_var = last_variables[&op];

    if (last_var == -1) {
	// choose the first precondition fact for op
	assert(next_var != -1);
	last_variables[&op] = next_var;
	return make_pair(next_var, next_val);
    }
    
    if (last_var == next_var) {
	// choose the same
	return make_pair(next_var, next_val);
    }
    else {
	// TODO: refactor! Currently, we have the code collecting the
	// precondition facts at several places :-(

	vector<pair<int, int> > precondition_facts;
	
	const vector<Condition> &preconds = op.get_preconditions();
	for (size_t i = 0; i < preconds.size(); ++i) {
	    precondition_facts.push_back(make_pair(preconds[i].var, preconds[i].val));
	}
	
	// choose a new one which minimizes the NES 
	pair<int, int> fact = find_unsatisfied_precondition_small_dynamic(state, precondition_facts);
	last_variables[&op] = fact.first;
	return fact;
    }
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition_laarman(const State &state,
									 const vector<pair<int, int> >& precondition_facts) {

    // check all possible necessary enabling sets according to the following heuristic:
    // every applicable operator costs n (not yet in stubborn)
    // every non-applicable operator costs 1 (not yet in stubborn)
    // return a fact that correponds to a set with smallest cost
    
    int lowest_cost = -1;
    int best_var = -1;
    int best_val = -1;
    
    for (size_t i = 0; i < precondition_facts.size() && lowest_cost != 0; i++) {
	
	int var = precondition_facts[i].first;
	int value = precondition_facts[i].second;

	int cost = 0;
	if (state[var] != value) {
	    const vector<int> &op_nos = achievers[var][value];
	    for (size_t k = 0; k < op_nos.size(); k++) {
		if (!stubborn[op_nos[k]]) {
		    const Operator& op = g_operators[op_nos[k]];
		    
		    pair<int, int> fact = find_unsatisfied_precondition_forward(op, state);
		    if (fact.first == -1) { // op not applicable
			cost++;
		    }
		    else { // op applicable => cost:=cost+n for some (large?) n
			//cost=cost+10;
			cost=cost+laarman_k; //default: 10
		    }
		}
	    }
	    
	
	    if (lowest_cost == -1 || cost < lowest_cost) {
		lowest_cost = cost;
		best_var = var;
		best_val = value;
	    }
	}
	
    }

    assert(best_var != -1);
    return make_pair(best_var, best_val);
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition_small_dynamic(const State &state,
									       const vector<pair<int, int> >& precondition_facts) {

	
    int number = -1;
    int best_var = -1;
    int best_val = -1;
    
    for (size_t i = 0; i < precondition_facts.size(); i++) {
	
	int var = precondition_facts[i].first;
	int value = precondition_facts[i].second;

	if (state[var] != value) {
	    const vector<int> &op_nos = achievers[var][value];
	    
	    int counter = 0;
	    for (size_t k = 0; k < op_nos.size(); k++) {
		if (!stubborn[op_nos[k]])
		    counter++;
	    }
	    
	    if (number == -1 || counter < number) {
		number = counter;
		best_var = var;
		best_val = value;
	    }
	}
    }

    assert(best_var != -1);
    return make_pair(best_var, best_val);    
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition_random_dynamic(const State &state,
									       const vector<pair<int, int> >& precondition_facts) {

	
    double number = -1;
    int best_var = -1;
    int best_val = -1;
    
    for (size_t i = 0; i < precondition_facts.size(); i++) {
	
	int var = precondition_facts[i].first;
	int value = precondition_facts[i].second;

	if (state[var] != value) {
	    double random_value = g_rng();
	    if (number == -1 || random_value < number) {
		number = random_value;
		best_var = var;
		best_val = value;
	    }
	}
    }

    assert(best_var != -1);
    return make_pair(best_var, best_val);    
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition_most_hitting(const State &state,
									      const vector<pair<int, int> >& precondition_facts) {

    // selects a precondition fact such that corresponding achievers
    // *additionally* achieve as many unsatisfied precondition facts
    // as possible

	
    int number = -1;
    int best_var = -1;
    int best_val = -1;
    
    for (size_t i = 0; i < precondition_facts.size(); i++) {
	
	int var = precondition_facts[i].first;
	int value = precondition_facts[i].second;

	if (state[var] != value) {
	    const vector<int> &op_nos = achievers[var][value];

	    int counter = 0;
	    for (size_t k = 0; k < op_nos.size(); k++) {
		if (!stubborn[op_nos[k]]) {
		    for (size_t a = 0; a < op_effects[op_nos[k]].size(); a++) {
			for (size_t l = 0; l < precondition_facts.size(); l++) {
			    if (op_effects[op_nos[k]][a].var == precondition_facts[l].first && 
				op_effects[op_nos[k]][a].val == precondition_facts[l].second &&
				state[precondition_facts[l].first] != precondition_facts[l].second &&
				l != i) {
				counter++;
			    }
			}
		    }
		}
	    }
	    
	    if (counter > number) {
		number = counter;
		best_var = var;
		best_val = value;
	    }
	    
	}
    }
    
    assert(best_var != -1);
    return make_pair(best_var, best_val);    
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition_global_variable_ordering(const Operator& op, const State& state) {

    // find *the* unsatisfied precondition fact in the precondition of
    // op that occurs first in global_variable_ordering
    
    // TODO: refactor! Currently, we have the code collecting the
    // precondition facts at several places :-(
    
    //    int index = -1;
    int best_achievers = -1;
    int best_var = -1;
    int best_val = -1;

    const vector<Condition> &preconds = op.get_preconditions();
    for (size_t i = 0; i < preconds.size(); ++i) {
	int var = preconds[i].var;
	
	if (state[var] !=  preconds[i].val) {
	    int achievers = global_variable_ordering[var];
	    if (achievers < best_achievers || best_achievers == -1) {
		best_var = var;
		best_val = preconds[i].val;
		best_achievers = achievers;
	    }
	}
    }
    
    // 	    for (int j = 0; j < global_variable_ordering.size() && (j < index || index == -1); j++) {
    // 		if (var == global_variable_ordering[j].first && (global_variable_ordering[j].second < num_achievers || num_achievers == -1)) {
    // 		    index = j;
    // 		    num_achievers = global_variable_ordering[j].second;
    // 		    best_var = var;
    // 		    best_val = prevail[i].prev;
    // 		}
    // 	    }
    // 	}
    // }

    // 		for (int j = 0; j < global_variable_ordering.size() && (j < index || index == -1); j++) {
    // 		    if (var == global_variable_ordering[j].first && (global_variable_ordering[j].second < num_achievers || num_achievers == -1)) {
    // 			index = j;
    // 			num_achievers = global_variable_ordering[j].second;
    // 			best_var = var;
    // 			best_val = pre_post[i].pre;
    // 		    }
    // 		}
    // 	    }
    // 	}
    // }
    
    return make_pair(best_var, best_val);
}

// Return the last unsatified precondition
static inline pair<int, int> find_unsatisfied_precondition_backward(const Operator &op, 
								    const State &state) {

    // REMARK: This is "pseudo" backward because we still start with
    // prevail rather than pre-post -- its a fixed ordering stategy,
    // though

    const vector<Condition> &preconds = op.get_preconditions();
    for (int i = preconds.size()-1; i >= 0; --i) {
        int var = preconds[i].var;
        int value = preconds[i].val;
        if (state[var] != value)
	    return make_pair(var, value);
    }

    // this should not be reachable
    assert(false);
    return make_pair(-1, -1);
}


static bool compare_entries(pair<pair<int, int>, int> p1, pair<pair<int, int>, int> p2) {

    return (p1.second < p2.second);

}

// static bool compare_pairs(pair<int, int> p1, pair<int, int> p2) {
    
//     return (p1.second <= p2.second);

// }

void SimpleStubbornSets::compute_static_precondition_orderings() {
    
    for (size_t i = 0; i < g_operators.size(); i++) {
	
	Operator* op = &g_operators[i];
        const vector<Condition> &preconds = op->get_preconditions();
	
	list<pair<pair<int, int>, int> > prioritized_facts;
	list<pair<pair<int, int>, int> >::iterator it;

	for (size_t j = 0; j < preconds.size(); j++) {
            const Condition *pp = &preconds[j];
            const vector<int> &op_nos = achievers[pp->var][pp->val];
            prioritized_facts.push_back(make_pair(make_pair(pp->var, pp->val), op_nos.size()));
        }

        prioritized_facts.sort(compare_entries);

        vector<pair<int, int> > ordered_facts;
        for (it=prioritized_facts.begin(); it!=prioritized_facts.end(); ++it) {
            ordered_facts.push_back((*it).first);
        }

        ordering_map[op] = ordered_facts;

    }

}

void SimpleStubbornSets::compute_static_goal_ordering() {
    
    list<pair<pair<int, int>, int> > prioritized_facts;
    
    for (size_t i = 0; i < g_goal.size(); i++) {
	const vector<int> &op_nos = achievers[g_goal[i].first][g_goal[i].second];
	
	// cout << "goal fact: " << op_nos.size() << " achievers" << endl;

	prioritized_facts.push_back(make_pair(g_goal[i],op_nos.size()));
    }

    prioritized_facts.sort(compare_entries);
    list<pair<pair<int, int>, int> >::iterator it;
    
    // cout << "==== AFTER sorting ==== " << endl;

    for (it=prioritized_facts.begin(); it!=prioritized_facts.end(); ++it) {
	// cout << "goal fact: " << (*it).second << " achievers" << endl;
	goal_ordering.push_back((*it).first);
    }

}

void SimpleStubbornSets::compute_static_variable_ordering() {

    // compute variable ordering such that variables with fewer
    // achievers are prioritized

    // list<pair<int, int> > prioritized_achievers;

    for (size_t i = 0; i < g_variable_domain.size(); i++) {
	int num_achievers = 0;
	for (int j = 0; j < g_variable_domain[i]; j++) {
	    num_achievers += achievers[i][j].size();
	}
	
	global_variable_ordering[i] = num_achievers; //prioritized_achievers.push_back(make_pair(i,num_achievers));
    }
    
    // prioritized_achievers.sort(compare_pairs);
    
    // list<pair<int, int> >::iterator it;
    // for (it=prioritized_achievers.begin(); it!=prioritized_achievers.end(); ++it) {
    // 	global_variable_ordering.push_back(make_pair((*it).first, (*it).second));
    // }

}

void SimpleStubbornSets::compute_random_static_variable_ordering() {

    // compute random variable ordering 

    size_t number_of_vars = g_variable_domain.size();

    for (size_t i = 0; i < number_of_vars; i++) {
	int random_number = g_rng.next(number_of_vars);
	
	global_variable_ordering[i] = random_number;
    }
}

void SimpleStubbornSets::dump_precondition_choice() {

    cout << "precondition choice function: ";
    
    switch (precond_choice) {
    case 0:
        cout << "forward" << endl;
        break;
    case 1:
        cout << "backward" << endl;
        break;
    case 2:
        cout << "small static" << endl;
        break;
    case 3:
        cout << "small dynamic (no fixed ordering)" << endl;
        break;
    case 4:
        cout << "laarman heuristic proposed in spin 2013 (no fixed ordering)" << endl;
	cout << "penalty K = " << laarman_k << endl;
	break;
    case 5:
	cout << "most hitting: find precondition such that as many operators as possible set other unsatisfied precondition vars" << endl;
	break;
    case 6:
	cout << "combination of forward and small dynamic" << endl;
	break;
    case 7:
	cout << "global variable ordering based on achiever sizes" << endl;
	break;
    case 8:
	cout << "random dynamic" << endl;
	break;
    case 9:
	cout << "random global variable ordering" << endl;
	break;	
    case 10:
        cout << "decoupled forward" << endl;
        break;  
    default:
	cout << "ERROR: unknown precondition selection" << endl;
        exit(2);
    }   

}

void SimpleStubbornSets::dump_mutex_option() {

    cout << "mutexes for dependency relationship: ";
    switch (mutex_option) {
    case 0:
        cout << "none" << endl;
        break;
    case 1:
        cout << "Fast Downward mutexes" << endl;
        break;
    default:
	cout << "ERROR: unknown option for mutexes" << endl;
        exit(2);
    }   

}

void SimpleStubbornSets::dump_active_ops_option() {

    cout << "active operators: ";
    if (use_active_ops)
	cout << "Enabled" << endl;
    else
	cout << "Disabled" << endl;

}

SimpleStubbornSets::SimpleStubbornSets(const Options& opts) : 
    PORMethodWithStatistics(opts),
    precond_choice(PreconditionChoice(opts.get_enum("precond_choice"))),
    mutex_option(MutexOption(opts.get_enum("mutexes"))),
    use_active_ops(opts.get<bool>("active_ops")),
    ignore_leaf_interference(opts.get<bool>("ignore_leaf_interference")),
    laarman_k(opts.get<int>("laarman_k")) {
    if (g_use_decoupled_search || precond_choice == decoupled){
        assert(g_use_decoupled_search);
        assert(precond_choice == decoupled);
    }
    
    verify_no_axioms_no_conditional_effects();
    compute_interference_relation();
    compute_achievers();

    size_t num_variables = g_variable_domain.size();

    if (use_active_ops) {
	build_dtgs(); // needed for active operator computation
	reachability_map.resize(num_variables);
	build_reachability_map(); // needed for active operator computation
    }

    if (precond_choice == 2) {
	compute_static_precondition_orderings();
	compute_static_goal_ordering();
    }

    if (precond_choice == 7) {
	compute_static_variable_ordering();
    }
    
    if (precond_choice == 9) {
	compute_random_static_variable_ordering();
    }
    
    if (precond_choice == 6) {
	for (size_t i = 0; i < g_operators.size(); i++)
	    last_variables[&(g_operators[i])] = -1;
    }    

    dump_precondition_choice();
    dump_mutex_option();
    dump_active_ops_option();
    
}

SimpleStubbornSets::~SimpleStubbornSets() {
}

void SimpleStubbornSets::dump_options() const {
    cout << "partial order reduction method: simple stubborn sets" << endl;
}

void SimpleStubbornSets::compute_sorted_operators() {
    for (size_t op_no = 0; op_no < g_operators.size(); ++op_no) {
        Operator *op = &g_operators[op_no];
        const vector<Effect> &effects = op->get_effects();
        const vector<Condition> &preconds = op->get_preconditions();

        vector<Fact> pre;
        vector<Fact> eff;

        for (size_t i = 0; i < effects.size(); i++) {
            const Effect *pp = &effects[i];
            Fact e(pp->var, pp->val);
            eff.push_back(e);
        }
        for (size_t i = 0; i < preconds.size(); i++) {
            const Condition *pp = &preconds[i];
            Fact p(pp->var, pp->val);
            pre.push_back(p);
        }
        if (pre.size() != 0) {
            sort(pre.begin(), pre.end());
            for (size_t i = 0; i < pre.size() - 1; ++i) {
                assert(pre[i].var < pre[i + 1].var);
            }
        }
        sort(eff.begin(), eff.end());
        for (size_t i = 0; i < eff.size() - 1; ++i) {
            assert(eff[i].var < eff[i + 1].var);
        }
        op_preconds.push_back(pre);
        op_effects.push_back(eff);
    }
}

void SimpleStubbornSets::compute_interference_relation() {
    compute_sorted_operators();

    size_t num_operators = g_operators.size();
    interference_relation.resize(num_operators);
    for (size_t op1_no = 0; op1_no < num_operators; ++op1_no) {
        if (ignore_leaf_interference && g_operators[op1_no].get_affected_factor() != -1){
            // do not store interfering operators for leaf operators
            continue;
        }
        vector<int> &interfere_op1 = interference_relation[op1_no];
        for (size_t op2_no = 0; op2_no < num_operators; ++op2_no) {
            if (op1_no != op2_no && interfere(op1_no, op2_no)){
                interfere_op1.push_back(op2_no);
            }
	}
    }
}

void SimpleStubbornSets::compute_achievers() {
    size_t num_variables = g_variable_domain.size();
    achievers.resize(num_variables);
    for (size_t var_no = 0; var_no < num_variables; ++var_no)
        achievers[var_no].resize(g_variable_domain[var_no]);

    for (size_t op_no = 0; op_no < g_operators.size(); ++op_no) {
        const Operator &op = g_operators[op_no];
        const vector<Effect> &effects = op.get_effects();
        for (size_t i = 0; i < effects.size(); ++i) {
            int var = effects[i].var;
            int value = effects[i].val;
            achievers[var][value].push_back(op_no);
        }
    }
}

inline void SimpleStubbornSets::mark_as_stubborn(int op_no) {
    if (!stubborn[op_no]) {
        stubborn[op_no] = true;
        stubborn_queue.push_back(op_no);
    }
}

// Add all operators that achieve the fact (var, value) to stubborn set.
void SimpleStubbornSets::add_nes_for_fact(pair<int, int> fact) {
    int var = fact.first;
    int value = fact.second;
    const vector<int> &op_nos = achievers[var][value];
    
    for (size_t i = 0; i < op_nos.size(); ++i)
	if (active_ops[op_nos[i]])
	    mark_as_stubborn(op_nos[i]);
}

// Add all operators that interfere with op.
void SimpleStubbornSets::add_interfering(int op_no) {
    const vector<int> &interferers = interference_relation[op_no];
    for (size_t i = 0; i < interferers.size(); ++i)
	if (active_ops[interferers[i]])
	    mark_as_stubborn(interferers[i]);
}



pair<int, int> SimpleStubbornSets::find_unsatisfied_goal(const State& state) {

    switch (precond_choice) {
    case 0: 
	return find_unsatisfied_goal_forward(state);
    case 1:
	return find_unsatisfied_goal_backward(state);
    case 2:
	//return find_unsatisfied_goal_small_dynamic(state); //should be the same as in the dynamic case
	return find_unsatisfied_goal_small_static(state);
    case 3:
	return find_unsatisfied_goal_small_dynamic(state);
    case 4:
	//return find_unsatisfied_goal_small_dynamic(state);
	return find_unsatisfied_precondition_laarman(state, g_goal);
    case 5:
	return find_unsatisfied_goal_most_hitting(state);
    case 6:
	return find_unsatisfied_goal_small_dynamic(state);
    case 7:
	//return find_unsatisfied_goal_small_dynamic(state);
	return find_unsatisfied_goal_global_variable_ordering(state);
    case 8:
	return find_unsatisfied_goal_random_dynamic(state);
    case 9:
	return find_unsatisfied_goal_global_variable_ordering(state); // same as in 7, but for random var ordering
    case 10:
        return find_unsatisfied_goal_decoupled(state);
    default:
	cout << "ERROR: unknown preconditon choice function for selecting goal variable: " << precond_choice << endl;
	exit(2);
    }
    
}

pair<int, int> SimpleStubbornSets::find_unsatisfied_precondition(const Operator &op, 
								 const State &state) {

    switch (precond_choice) {
    case 0: 
	return find_unsatisfied_precondition_forward(op, state);
    case 1:
	return find_unsatisfied_precondition_backward(op, state);
    case 2:
	return find_unsatisfied_precondition_small_static(op, state);
    case 3:
	return find_unsatisfied_precondition_dynamic(op, state); // dispatchted there depending on precond_choice
    case 4:
	return find_unsatisfied_precondition_dynamic(op, state); // dispatchted there depending on precond_choice
    case 5: 
	return find_unsatisfied_precondition_dynamic(op, state); // dispatched there depending on precond_choice
    case 6:
	return find_unsatisfied_precondition_forward_small(op, state);
    case 7:
	return find_unsatisfied_precondition_global_variable_ordering(op, state);
    case 8:
	return find_unsatisfied_precondition_dynamic(op, state);
    case 9:
	return find_unsatisfied_precondition_global_variable_ordering(op, state); // same as in 7, but for random var ordering	
    case 10:
        return find_unsatisfied_precondition_decoupled(op, state);
    default:
	cout << "ERROR: unknown preconditon choice function for selecting precondition variable: " << precond_choice << endl;
	exit(2);
    }	

}

void SimpleStubbornSets::mark_open_center_pre_of_decoupled_goal_frontier(const State &state) {
    // we cannot know which open center precondition actually is achievable, i.e. can be used to
    // instantiate a disjunctive action landmark; thus, we need to collect *all* of them
    
    const CompliantPathGraph *tag = CPGStorage::get_cpg(state);
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        vector<vector<pair<const Operator*, size_t> > > &leaf_successors = CompliantPathGraph::leaf_state_successors[factor];
        
        size_t number_states = tag->get_number_states(factor);
        size_t id = 0;        
        while (number_states > 0){
            if (tag->has_leaf_state(id, factor)){
                --number_states;
                for (size_t succ = 0; succ < leaf_successors[id].size(); ++succ){
                    const Operator *op = leaf_successors[id][succ].first;
                    int successor_id = leaf_successors[id][succ].second;
                    int new_price = tag->get_cost_of_state(id, factor) + get_adjusted_action_cost(*op, g_factoring->get_cost_type());
                    if (!tag->has_leaf_state(successor_id, factor) || new_price < tag->get_cost_of_state(successor_id, factor)){
                        // is frontier state
                        const vector<const Condition*> &preconditions = op->get_preconditions(-1);
#ifndef NDEBUG
                        bool found_open_pre = false;
#endif
                        for (size_t pre = 0; pre < preconditions.size(); ++pre){
                            if (state[preconditions[pre]->var] != preconditions[pre]->val){
#ifndef NDEBUG
                                found_open_pre = true;
#endif
                                add_nes_for_fact(make_pair(preconditions[pre]->var, preconditions[pre]->val));
                            }
                        }
                        assert(found_open_pre); // there must be an open center precondition
                    }
                }
            }
            ++id;
        }
    }
}

// This code is copied from ExpansionCore
void SimpleStubbornSets::build_dtgs() {
    /*
  TODO: We definitely need to refactor this! Currently, we have this
  code in three different places (ec,sss-ec, here)
     */

    cout << "[small stubborn sets] Building DTGs for active operators..." << flush;
    assert(dtgs.empty());
    size_t num_variables = g_variable_domain.size();

    dtgs.resize(num_variables);

    // Step 1: Create the empty DTG nodes.
    for (size_t var_no = 0; var_no < num_variables; ++var_no) {
        size_t var_size = g_variable_domain[var_no];
        dtgs[var_no].nodes.resize(var_size);
        dtgs[var_no].goal_values.resize(var_size, true);
    }

    // Step 2: Mark goal values in each DTG. Variables that are not in
    //         the goal have previously been set to "all are goals".
    for (size_t i = 0; i < g_goal.size(); ++i) {
        int var_no = g_goal[i].first;
        int goal_value = g_goal[i].second;
        vector<bool> &goal_values = dtgs[var_no].goal_values;
        size_t var_size = g_variable_domain[var_no];
        goal_values.clear();
        goal_values.resize(var_size, false);
        goal_values[goal_value] = true;
    }

    // Step 3: Add DTG arcs.
    for (size_t op_no = 0; op_no < g_operators.size(); ++op_no) {
        const Operator &op = g_operators[op_no];
        const vector<Effect> &effects = op.get_effects();
        const vector<Condition> &preconds = op.get_preconditions();
        for (size_t i = 0; i < effects.size(); ++i) {
            int var = effects[i].var;
            int pre_value = -1;
            for (size_t j = 0; j < preconds.size(); j++){
                if (var == preconds[j].var){
                    pre_value = preconds[j].val;
                    break;
                }
            }
            int post_value = effects[i].val;

            ExpansionCoreDTG &dtg = dtgs[var];
            int pre_value_min, pre_value_max;
            if (pre_value == -1) {
                pre_value_min = 0;
                pre_value_max = g_variable_domain[var];
            } else {
                pre_value_min = pre_value;
                pre_value_max = pre_value + 1;
            }
            for (int value = pre_value_min; value < pre_value_max; ++value) {
                dtg.nodes[value].outgoing.push_back(
                    ExpansionCoreDTG::Arc(post_value, op_no));
                dtg.nodes[post_value].incoming.push_back(
                    ExpansionCoreDTG::Arc(value, op_no));
            }
        }
    }
    cout << " done!" << endl;
}

void SimpleStubbornSets::build_reachability_map() {

    cout << "Building reachability map ";

    size_t num_variables = g_variable_domain.size();
    for (size_t var_no = 0; var_no < num_variables; ++var_no) {
        ExpansionCoreDTG &dtg = dtgs[var_no];
        size_t num_values = dtg.nodes.size();
        reachability_map[var_no].resize(num_values);
        for (size_t val = 0; val < num_values; ++val) {
            reachability_map[var_no][val].assign(num_values, false);
        }
        for (int start_value = 0; start_value < g_variable_domain[var_no]; start_value++) {
            vector<bool> &reachable = reachability_map[var_no][start_value];
            recurse_forwards(var_no, start_value, start_value, reachable);
        }
    }

    cout << "done" << endl;

}

// Copied from SSS_ExpansionCore
void SimpleStubbornSets::recurse_forwards(int var, int start_value, int current_value, std::vector<bool> &reachable) {
    ExpansionCoreDTG &dtg = dtgs[var];
    if (!reachable[current_value]) {
        reachable[current_value] = true;
        const vector<ExpansionCoreDTG::Arc> &outgoing = dtg.nodes[current_value].outgoing;
        for (size_t i = 0; i < outgoing.size(); ++i)
            recurse_forwards(var, start_value, outgoing[i].target_value, reachable);
    }
}

// copied from SSS_ExpansionCore
void SimpleStubbornSets::compute_active_operators(const State &state) {
    for (size_t op_no = 0; op_no < g_operators.size(); ++op_no) {
        Operator &op = g_operators[op_no];
        bool all_preconditions_are_active = true;
        const vector<Condition> &preconds = op.get_preconditions();
        for (size_t i = 0; i < preconds.size(); ++i) {
            int var = preconds[i].var;
            int value = preconds[i].val;
            int current_value = state[var];
            std::vector<bool> &reachable_values = reachability_map[var][current_value];
            if (!reachable_values[value]) {
                all_preconditions_are_active = false;
                break;
            }
        }
        if (all_preconditions_are_active) {
            active_ops[op_no] = true;
        }
    }
}


void SimpleStubbornSets::do_pruning(
    const State &state, vector<const Operator *> &applicable_ops) {
    // Clear stubborn set from previous call.
    stubborn.clear();
    stubborn.assign(g_operators.size(), false);
    active_ops.clear();
    active_ops.assign(g_operators.size(), false);

    if (use_active_ops){
        compute_active_operators(state);
    } else {
        active_ops.assign(g_operators.size(), true);
    }

    if (g_use_decoupled_search && test_goal(state)){
        // get all open center preconditions of all leaf frontier states
        
        mark_open_center_pre_of_decoupled_goal_frontier(state);      
        
        if (stubborn_queue.empty()){
            // this can happen in case a decoupled goal state has an empty frontier
            // in this case, there cannot be a postfix in state achieving a lower
            // global cost
            applicable_ops.clear();
            return;
        }
    } else {
        // Add a necessary enabling set for an unsatisfied goal.    
        pair<int, int> goal_pair = find_unsatisfied_goal(state);
        assert(goal_pair != make_pair(-1, -1));
        add_nes_for_fact(goal_pair);
    }

    // Iteratively insert operators to stubborn according to the
    // definition of strong stubborn sets until a fixpoint is reached.
    while (!stubborn_queue.empty()) {
        int op_no = stubborn_queue.back();
        stubborn_queue.pop_back();
        const Operator &op = g_operators[op_no];
        
	pair<int, int> fact;
        if (g_use_decoupled_search){
            fact = find_unsatisfied_precondition_decoupled(op, state);
        } else {
            // HACK: also used for applicability test
            fact = find_unsatisfied_precondition_forward(op, state);
        }
	
	if (fact.first == -1) {
            // no unsatisfied precondition found
            // => operator is applicable
            // => add all interfering operators
    	    add_interfering(op_no);
        } else {
            // unsatisfied precondition found
            // => add an enabling set for it

	    // HACK: if precond_choice == 0, then we have the required
	    // fact already computed in the applicability test above
	    if (precond_choice != 0 && !g_use_decoupled_search) {
		fact = find_unsatisfied_precondition(op, state);
	    }
	    add_nes_for_fact(fact);
	}
    }
	
    // Now check which applicable operators are in the stubborn set.
    vector<const Operator *> pruned_ops;
    pruned_ops.reserve(applicable_ops.size());
    for (size_t i = 0; i < applicable_ops.size(); ++i) {
        const Operator *op = applicable_ops[i];
        int op_no = get_op_index(op);
        if (stubborn[op_no])
            pruned_ops.push_back(op);
    }
    if (pruned_ops.size() != applicable_ops.size()) {
        applicable_ops.swap(pruned_ops);
        sort(applicable_ops.begin(), applicable_ops.end());
    }
}
}
