#include "stubborn_sets_decoupled.h"

#include "../compliant_paths/cpg_storage.h"
#include "../globals.h"
#include "../operator.h"
#include "../option_parser.h"
#include "../plugin.h"


using namespace std;

namespace stubborn_sets_decoupled {
/* Implementation of simple instantiation of strong stubborn sets.
   Disjunctive action landmarks are computed trivially.*/


// Return the first unsatified precondition, or (-1, -1) if there is none.
static inline vector<pair<int, int>> find_unsatisfied_preconditions(
    const Operator &op, const State &state, const CompliantPathGraph *current_cpg) {

    vector<pair<int, int>> res;
    
    // first handle open leaf preconditions, to get back to frontier states
    // if there exists a precondition that is not satisfied by any reachable
    // state => add NES for this precondition. next, if the operator is not
    // applicable in a leaf, add NES for all preconditions that are open in
    // in any reached leaf state. finally, add NES for open center precondition
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (op.has_precondition_on(factor)){
            const vector<const Condition*> &preconds = op.get_preconditions(factor);
            
            vector<bool> unsat(preconds.size(), true);
            vector<bool> sat(preconds.size(), false);
            
            bool applicable = false;
            
            for (size_t id = 0; id < g_state_registry->size(factor); ++id){
                if (current_cpg->has_leaf_state(id, factor)){
                    const LeafState &leaf_state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
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
    
    for (const Condition *precondition : op.get_preconditions(-1)) {
        int var = precondition->var;
        int value = precondition->val;
        if (state[var] != value){
            res.push_back(pair<int, int>(var, value));
            return res;
        }
    }
    return res;
}

StubbornSetsDecoupled::StubbornSetsDecoupled(const Options &opts) : 
        ignore_leaf_interference(opts.get<bool>("ignore_leaf_interference")) {
    // TODO implement safety belt in base class
    if (!g_use_decoupled_search){
        cout << "Decoupled strong stubborn sets only work if decoupled search is enabled!" << endl;
        exit_with(EXIT_INPUT_ERROR);
    }
    cout << "pruning method: stubborn sets decoupled" << endl;
    CompliantPathGraph::set_compute_leaf_backwards_graph();
    compute_interference_relation();
}

void StubbornSetsDecoupled::mark_frontier_actions_as_stubborn(int factor) {
    for (size_t id = 0; id < g_state_registry->size(factor); ++id){
        if (current_cpg->has_leaf_state(id, factor)){
            int curr_cost = current_cpg->get_cost_of_state(id, factor);
            for (auto &succ : CompliantPathGraph::leaf_state_successors[factor][id]){
                int new_price = curr_cost;
                new_price += get_adjusted_action_cost(*succ.first, g_factoring->get_cost_type());
                if (!current_cpg->has_leaf_state(succ.second, factor) || new_price < current_cpg->get_cost_of_state(succ.second, factor)){
                    mark_as_stubborn(succ.first->get_index());
                }
            }
        }
    }
}

void StubbornSetsDecoupled::compute_interference_relation() {
    int num_operators = g_operators.size();
    interference_relation.resize(num_operators);

    /*
       TODO: as interference is symmetric, we only need to compute the
       relation for operators (o1, o2) with (o1 < o2) and add a lookup
       method that looks up (i, j) if i < j and (j, i) otherwise.
    */
    for (int op1_no = 0; op1_no < num_operators; ++op1_no) {
        if (ignore_leaf_interference && g_factoring->get_profile() == FORK && g_operators[op1_no].get_affected_factor() != -1){
            // TODO possibly introduce this also for non-fork factorings
            // (and add a check for competing leaf preconditions)
            continue;
        }
        vector<int> &interfere_op1 = interference_relation[op1_no];
        for (int op2_no = 0; op2_no < num_operators; ++op2_no) {
            if (op1_no != op2_no && interfere(op1_no, op2_no)) {
                interfere_op1.push_back(op2_no);
            }
        }
    }
}

void StubbornSetsDecoupled::initialize_stubborn_set(const State &state) {
    current_cpg = CPGStorage::get_cpg(state);

    // if goal in a factor not reached, mark all ingoing transition to all
    // goal states as stubborn
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (!g_goals_per_factor[factor].empty() && !current_cpg->goal_reachable(factor)){
            
            
            
            
            
//             vector<bool> unsat(g_goals_per_factor[factor].size(), true);
//             vector<bool> sat(g_goals_per_factor[factor].size(), false);
//             
//             for (size_t id = 0; id < g_state_registry->size(factor); ++id){
//                 if (current_cpg->has_leaf_state(id, factor)){
//                     assert(!CompliantPathGraph::is_leaf_goal_state(id, factor));
//                     const LeafState &leaf_state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
//                     for (size_t i = 0; i < g_goals_per_factor[factor].size(); ++i){
//                         if (leaf_state[g_goals_per_factor[factor][i].first] != g_goals_per_factor[factor][i].second){
//                             unsat[i] = false;
//                         } else {
//                             sat[i] = true;
//                         }
//                     }
//                 }
//             }
//             for (size_t i = 0; i < sat.size(); ++i){
//                 if (!sat[i]){
//                     add_necessary_enabling_set(g_goals_per_factor[factor][i]);
//                     return;
//                 }
//             }
//             for (size_t i = 0; i < unsat.size(); ++i){
//                 if (unsat[i]){
//                     add_necessary_enabling_set(g_goals_per_factor[factor][i]);
//                 }
//             }
//             return;
            
            
            
            for (size_t id = 0; id < g_state_registry->size(factor); ++id){
                if (CompliantPathGraph::is_leaf_goal_state(id, factor)){
                    assert(!current_cpg->has_leaf_state(id, factor));
                    for (auto &pred : CompliantPathGraph::leaf_state_predecessors[factor][id]){
                        // TODO maybe it's better not to mark all ingoing transitions, but to
                        // add a NES for all open goal facts, similar to open preconditions
                        mark_as_stubborn(pred.first->get_index());
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
    
    // for decoupled goal states, mark all frontier actions in all leaves 
    // that have a goal condition as stubborn
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (!g_goals_per_factor[factor].empty() && current_cpg->get_goal_cost(factor) > g_min_goal_cost[factor]){
            mark_frontier_actions_as_stubborn(factor);
        }
    }
}

void StubbornSetsDecoupled::handle_stubborn_operator(const State &state,
                                                     int op_no) {
    const Operator &op = g_operators[op_no];
    
    vector<pair<int, int>> unsatisfied_preconditions = find_unsatisfied_preconditions(op, state, current_cpg);
    if (unsatisfied_preconditions.empty()) {
        /* no unsatisfied precondition found
        => operator is applicable
        => add all interfering operators */
        add_interfering(op_no);
        
        // in optimal search, add frontier of an operator to
        // stubborn set, for all leaves this op has preconditions on
        // needed to capture improvements of the precondition's price
        if (g_factoring->get_search_type() != SAT && g_factoring->get_search_type() != UNSAT){
            // TODO only add frontier, if precondition can actually be improved!
            for (size_t factor = 0; factor < g_factors.size(); ++factor){
                if (op.has_precondition_on(factor)){
                    mark_frontier_actions_as_stubborn(factor);
                }
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

static PORMethod* _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Stubborn sets decoupled",
        "TODO");
    
    parser.add_option<bool>("ignore_leaf_interference",
                            "TODO",
                            "true");

    if (parser.dry_run()) {
        return 0;
    }

    return new StubbornSetsDecoupled(parser.parse());
}

static Plugin<PORMethod> _plugin("stubborn_sets_decoupled", _parse);
}
