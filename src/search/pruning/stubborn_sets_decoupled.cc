#include "stubborn_sets_decoupled.h"

#include "../compliant_paths/cpg_storage.h"
#include "../globals.h"
#include "../operator.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../search_space.h"


using namespace std;

namespace stubborn_sets_decoupled {
/* Implementation of simple instantiation of strong stubborn sets.
   Disjunctive action landmarks are computed trivially.*/


static inline vector<pair<int, int>> find_unsatisfied_preconditions(
    const Operator &op, const State &state, const CompliantPathGraph *current_cpg) {

    vector<pair<int, int>> res;
    
    // first handle open leaf preconditions, to get back to frontier states
    // if there exists a precondition that is not satisfied by any reachable
    // state => add NES for this precondition. next, if the operator is not
    // applicable in a leaf, add NES for all preconditions that are open in
    // in any reached leaf state. finally, add NES for open center precondition
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        if (op.has_precondition_on(factor)){
            const vector<const Condition*> &preconds = op.get_preconditions(factor);
            
            vector<bool> unsat(preconds.size(), true);
            vector<bool> sat(preconds.size(), false);
            
            bool applicable = false;
            
            for (LeafStateHash id(0); id < g_state_registry->size(factor); ++id){
                if (current_cpg->has_leaf_state(id, factor)){
                    const LeafState &leaf_state = g_state_registry->lookup_leaf_state(id, factor);
                    bool applicable_state = true;
                    for (size_t i = 0; i < preconds.size(); ++i){
                        if (leaf_state[preconds[i]->var] != preconds[i]->val){
                            unsat[i] = false;
                            applicable_state = false;
                        } else {
                            sat[i] = true;
                        }
                    }
                    if (applicable_state){
                        applicable = true;
                        break;
                    }
                }
            }
            if (!applicable){
                for (size_t i = 0; i < sat.size(); ++i){
                    if (!sat[i]){
                        res.push_back(make_pair(preconds[i]->var, preconds[i]->val));
                        return res;
                    }
                }
                for (size_t i = 0; i < unsat.size(); ++i){
                    if (unsat[i]){
                        res.push_back(make_pair(preconds[i]->var, preconds[i]->val));
                    }
                }
                return res;
            }
        }
    }
    
    for (const Condition *precondition : op.get_preconditions(LeafFactorID::CENTER)) {
        int var = precondition->var;
        int value = precondition->val;
        if (state[var] != value){
            res.push_back(make_pair(var, value));
            return res;
        }
    }
    return res;
}

static inline bool have_mutex_preconditions(const vector<pair<int, int>> &pre1, const vector<pair<int, int>> &pre2) {
    size_t pre1_count = 0;
    size_t pre2_count = 0;
    while (pre1_count < pre1.size() && pre2_count < pre2.size()) {
        if (pre1[pre1_count].first < pre2[pre2_count].first){
            ++pre1_count;
            continue;
        }
        if (pre1[pre1_count].first > pre2[pre2_count].first){
            ++pre2_count;
            continue;
        }
        if (pre1[pre1_count].second != pre2[pre2_count].second){
            return true;
        }
        ++pre1_count;
        ++pre2_count;
    }
    return false;
}

StubbornSetsDecoupled::StubbornSetsDecoupled(const Options &opts) : 
            goal_ingoing_transitions(opts.get<bool>("goal_ingoing_transitions")),
            special_case_optimizations(opts.get<bool>("special_case_optimizations")),
            interference_no_mutex_pre(opts.get<bool>("interference_no_mutex_pre")) {
    // TODO implement safety belt in base class
    if (!g_factoring){
        cout << "Decoupled strong stubborn sets only work if decoupled search is enabled!" << endl;
        exit_with(EXIT_INPUT_ERROR);
    }
    cout << "pruning method: stubborn sets decoupled" << endl;
    CompliantPathGraph::set_compute_leaf_backwards_graph();
    
    if (g_factoring->get_profile() == FORK){
        is_fork_factoring = true;
    } else if (g_factoring->get_profile() == IFORK){
        is_ifork_factoring = true;
        is_single_var_ifork_factoring = true;
        for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
            if (g_leaves[factor].size() > 1){
                is_single_var_ifork_factoring = false;
                break;
            }
        }
    } else {
        is_fork_leaf.resize(g_leaves.size(), false);
        is_ifork_leaf.resize(g_leaves.size(), false);
        for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
            if (g_factoring->is_fork_leaf(factor)){
                is_fork_leaf[factor] = true;
            } else if (g_factoring->is_ifork_leaf(factor)){
                is_ifork_leaf[factor] = true;
            }
        }
    }
    
    compute_interference_relation();
}

void StubbornSetsDecoupled::mark_frontier_actions_as_stubborn(LeafFactorID factor) {
    // in optimal search, add frontier of an operator to
    // stubborn set, for all leaves this op has preconditions on
    // needed to capture improvements of the precondition's price
    // TODO verify this
    if (g_factoring->get_search_type() != SAT && g_factoring->get_search_type() != UNSAT){
        for (LeafStateHash id(0); id < g_state_registry->size(factor); ++id){
            if (current_cpg->has_leaf_state(id, factor)){
                int curr_cost = current_cpg->get_cost_of_state(id, factor);
                for (auto &succ : CompliantPathGraph::leaf_state_successors[factor][id]){
                    int new_price = curr_cost;
                    new_price += get_adjusted_action_cost(*succ.first, CompliantPathGraph::get_cost_type());
                    if (!current_cpg->has_leaf_state(succ.second, factor) || new_price < current_cpg->get_cost_of_state(succ.second, factor)){
                        mark_as_stubborn(succ.first->get_index());
                    }
                }
            }
        }
    }
}

void StubbornSetsDecoupled::mark_reached_enabling_set_as_stubborn(LeafFactorID factor) {
    for (LeafStateHash id(0); id < g_state_registry->size(factor); ++id){
        if (current_cpg->has_leaf_state(id, factor)){
            for (auto &succ : CompliantPathGraph::leaf_state_successors[factor][id]){
                mark_as_stubborn(succ.first->get_index());
            }
        }
    }
}

void StubbornSetsDecoupled::compute_interference_relation() {
    size_t num_operators = g_operators.size();
    interference_relation.resize(num_operators);

    /*
       TODO: as interference is symmetric, we only need to compute the
       relation for operators (o1, o2) with (o1 < o2) and add a lookup
       method that looks up (i, j) if i < j and (j, i) otherwise.
    */
    for (size_t op1_no = 0; op1_no < num_operators; ++op1_no) {
//         if (g_factoring->get_profile() == FORK && g_operators[op1_no].get_affected_factor() != LeafFactorID::CENTER){
//             // TODO possibly introduce this also for non-fork factorings
//             // (and add a check for competing leaf preconditions)
//             continue;
        // TODO fix this
//         }
        // TODO special case handling
        vector<size_t> &interfere_op1 = interference_relation[op1_no];
        for (size_t op2_no = 0; op2_no < num_operators; ++op2_no) {
            if (op1_no != op2_no && 
                (!interference_no_mutex_pre || !have_mutex_preconditions(sorted_op_preconditions[op1_no], sorted_op_preconditions[op2_no])) && 
                interfere(op1_no, op2_no)) {
                interfere_op1.push_back(op2_no);
            }
        }
    }
}

void StubbornSetsDecoupled::initialize_stubborn_set(const State &state) {
    // TODO maybe it's better not to mark all ingoing transitions, but to
    // add a NES for all open goal facts, similar to open preconditions
    // on the other hand, including the ingoing transitions to goal states is fast!
    
    current_cpg = CPGStorage::storage->get_cpg(state);

    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        if (!g_goals_per_factor[factor].empty() && !current_cpg->goal_reachable(factor)){
            if (goal_ingoing_transitions){
                // if goal in a factor not reached, mark all ingoing transition to all
                // goal states as stubborn
                for (LeafStateHash id : CompliantPathGraph::leaf_goal_states[factor]){
                    assert(!current_cpg->has_leaf_state(id, factor));
                    for (auto &pred : CompliantPathGraph::leaf_state_predecessors[factor][id]){
                        mark_as_stubborn(pred.first->get_index());
                    }
                }
            } else {
                // if there exists a leaf fact that is not achieved in any reached leaf state => add enablers
                // otherwise, all leaf goal facts are reached in some state and we need to include the
                // enablers of all leaf goal facts unsatisfied in any reached leaf state.
                vector<bool> unsat(g_goals_per_factor[factor].size(), true);
                vector<bool> sat(g_goals_per_factor[factor].size(), false);
                
                for (LeafStateHash id(0); id < g_state_registry->size(factor); ++id){
                    if (current_cpg->has_leaf_state(id, factor)){
                        assert(!CompliantPathGraph::is_leaf_goal_state(id, factor));
                        const LeafState &leaf_state = g_state_registry->lookup_leaf_state(id, factor);
                        for (size_t i = 0; i < g_goals_per_factor[factor].size(); ++i){
                            if (leaf_state[g_goals_per_factor[factor][i].first] != g_goals_per_factor[factor][i].second){
                                unsat[i] = false;
                            } else {
                                sat[i] = true;
                            }
                        }
                    }
                }
                for (size_t i = 0; i < sat.size(); ++i){
                    if (!sat[i]){
                        add_necessary_enabling_set(g_goals_per_factor[factor][i]);
                        // can stop at first leaf fact that is not reached in any reached leaf state
                        return;
                    }
                }
                for (size_t i = 0; i < unsat.size(); ++i){
                    if (unsat[i]){
                        add_necessary_enabling_set(g_goals_per_factor[factor][i]);
                    }
                }
            }
            return;
        }
    }
    
    // if all leaf goals reached, look for open center goal
    for (size_t i = 0; i < g_goal.size(); ++i){
        if (state[g_goal[i].first] != g_goal[i].second){
            add_necessary_enabling_set(g_goal[i]);
            return;
        }
    }
    
    assert(g_factoring->get_search_type() != SAT && g_factoring->get_search_type() != UNSAT);
    assert(test_goal(state));
    
    int leaf_part_g = 0;
    if (!is_fork_factoring){
        leaf_part_g = CompliantPathGraph::search_space->get_node(state).get_leaf_part_g();
    }
    
    // for decoupled goal states, mark all frontier actions in all leaves 
    // that have a goal condition as stubborn
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        if (!g_goals_per_factor[factor].empty()){
            if (is_fork_leaf[factor]){
                if (current_cpg->get_goal_cost(factor) > g_min_goal_cost[factor]){
                    // TODO implement specialized version for forks?
                    mark_frontier_actions_as_stubborn(factor);
                }
            } else if (!is_ifork_leaf[factor] && (leaf_part_g + current_cpg->get_goal_cost(factor)) > g_min_goal_cost[factor]) {
                mark_frontier_actions_as_stubborn(factor);
            }
        }
    }
}

void StubbornSetsDecoupled::handle_stubborn_operator(const State &state,
                                                     size_t op_no) {
    const Operator &op = g_operators[op_no];
    
    vector<pair<int, int>> unsatisfied_preconditions = find_unsatisfied_preconditions(op, state, current_cpg);
    if (unsatisfied_preconditions.empty()) {
        /* no unsatisfied precondition found
        => operator is applicable
        => add all interfering operators */
        add_interfering(op_no);
        
        for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
            if (op.has_precondition_on(factor)){
                mark_reached_enabling_set_as_stubborn(factor);
//                     mark_frontier_actions_as_stubborn(factor);
                // TODO special case handling
            }
        }
    } else {
        /* unsatisfied precondition found
        => add a necessary enabling set for it */
        for (auto &open_pre : unsatisfied_preconditions){
            add_necessary_enabling_set(open_pre);
        }
    }
}

static PruningMethod* _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Stubborn sets decoupled",
        "TODO");
    
    parser.add_option<bool>("goal_ingoing_transitions",
                            "TODO",
                            "false");
    
    parser.add_option<bool>("special_case_optimizations",
                            "TODO",
                            "false");
    
    parser.add_option<bool>("interference_no_mutex_pre",
                            "TODO",
                            "true");

    if (parser.dry_run()) {
        return 0;
    }

    return new StubbornSetsDecoupled(parser.parse());
}

static Plugin<PruningMethod> _plugin("stubborn_sets_decoupled", _parse);
}
