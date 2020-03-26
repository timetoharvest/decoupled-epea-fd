#include "ff_heuristic.h"

#include "compliant_paths/compliant_path_graph.h"
#include "compliant_paths/cpg_storage.h"
#include "state.h"
#include "operator.h"
#include "option_parser.h"
#include "plugin.h"


#include <cassert>

using namespace std;

namespace ff_heuristic {
// construction and destruction
FFHeuristic::FFHeuristic(const Options &opts)
    : AdditiveHeuristic(opts),
      relaxed_plan(g_operators.size(), false),
      current_leaf_cost(0) {
    cout << "Initializing FF heuristic..." << endl;
}

void FFHeuristic::mark_preferred_operators_and_relaxed_plan(
    const State &state, PropID goal_id) {
    Proposition *goal = get_proposition(goal_id);
    if (!goal->marked) { // Only consider each subgoal once.
        goal->marked = true;
        OpID op_id = goal->reached_by;
        if (op_id != NO_OP) { // We have not yet chained back to a start node.
            if (heuristic_type == HEURISTIC_TYPE::STAR){
                if (op_id < NO_OP){
                    size_t id = -(op_id + 2);
                    current_leaf_cost += leaf_state_costs[id];
                    leaf_state_costs[id] = 0; // in case the same state is used multiple times
                    return;
                }
            }
            assert(op_id >= 0);
            UnaryOperator *unary_op = get_operator(op_id);
            bool is_preferred = true;
            for (PropID precond : get_preconditions(op_id)) {
                mark_preferred_operators_and_relaxed_plan(
                    state, precond);
                if (get_proposition(precond)->reached_by != NO_OP) {
                    is_preferred = false;
                }
            }
            int operator_no = unary_op->operator_no;
            if (operator_no != -1) {
                // This is not an axiom.
                relaxed_plan[operator_no] = true;
                if (is_preferred) {
                    const Operator &op = g_operators[operator_no];
                    if (heuristic_type == HEURISTIC_TYPE::STD || 
                            op.get_affected_factor() == LeafFactorID::CENTER) {
                        // don't mark leaf ops as preferred
                        assert(op.is_applicable(state));
                        set_preferred(op.get_id());
                    }
                }
            }
        }
    }
}

void FFHeuristic::setup_exploration_queue_decoupled_leaf_states(const State &state) {
    const CompliantPathGraph *tag = CPGStorage::storage->get_cpg(state);
    size_t total_leaf_state_counter = 0;
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        int number_states = tag->get_number_states(factor);
        LeafStateHash id(0);
        while (number_states > 0){
            if (tag->has_leaf_state(id, factor)){
                --number_states;
                int cost = 0;
                OpID op_id = NO_OP;
                if (heuristic_type == HEURISTIC_TYPE::STAR){
                    op_id = -total_leaf_state_counter - 2;
                    cost = tag->get_cost_of_state(id, factor);
                    if (total_leaf_state_counter >= leaf_state_costs.size()){
                        leaf_state_costs.push_back(cost);
                    } else {
                        leaf_state_costs[total_leaf_state_counter] = cost;
                    }
                    ++total_leaf_state_counter;
                }
                LeafState f = g_state_registry->lookup_leaf_state(id, factor);
                for (int var : g_leaves[factor]) {
                    enqueue_if_necessary(get_prop_id(var, f[var]), cost, op_id);
                }
            }
            ++id;
        }
    }
}

int FFHeuristic::compute_heuristic(const State &state) {
    current_leaf_cost = 0;
    setup_exploration_queue();
    
    if (heuristic_type != HEURISTIC_TYPE::STD){
        setup_exploration_queue_decoupled_leaf_states(state);
    }
    
    int h_add = compute_add_and_ff(state);
    if (h_add == DEAD_END)
        return h_add;

    // Collecting the relaxed plan also sets the preferred operators.
    for (PropID goal_id : goal_propositions)
        mark_preferred_operators_and_relaxed_plan(state, goal_id);

    int h_ff = 0;
    for (size_t op_no = 0; op_no < relaxed_plan.size(); ++op_no) {
        if (relaxed_plan[op_no]) {
            relaxed_plan[op_no] = false; // Clean up for next computation.
            h_ff += get_adjusted_cost(g_operators[op_no]);
        }
    }
    assert(heuristic_type == HEURISTIC_TYPE::STAR || current_leaf_cost == 0);
    return h_ff + current_leaf_cost;
}


static Heuristic* _parse(OptionParser &parser) {
    parser.document_synopsis("FF heuristic", "");
    parser.document_language_support("action costs", "supported");
    parser.document_language_support("conditional effects", "supported");
    parser.document_language_support(
        "axioms",
        "supported (in the sense that the planner won't complain -- "
        "handling of axioms might be very stupid "
        "and even render the heuristic unsafe)");
    parser.document_property("admissible", "no");
    parser.document_property("consistent", "no");
    parser.document_property("safe", "yes for tasks without axioms");
    parser.document_property("preferred operators", "yes");

    Heuristic::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return new FFHeuristic(opts);
}

static Plugin<Heuristic> _plugin("ff", _parse);
}
