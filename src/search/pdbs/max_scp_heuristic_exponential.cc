#include "max_scp_heuristic_exponential.h"

#include "pattern_filter.h"
#include "types.h"
#include "utils.h"

#include "../globals.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../compliant_paths/cpg_storage.h"
#include "../compliant_paths/explicit_state_cpg.h"

#include "../cost_saturation/cost_partitioning_heuristic.h"
#include "../cost_saturation/cost_partitioning_heuristic_collection_generator.h"
#include "../cost_saturation/projection.h"
#include "../cost_saturation/projection_generator.h"
#include "../cost_saturation/saturated_cost_partitioning_heuristic.h"
#include "../cost_saturation/utils.h"

#include "../tasks/root_task.h"
#include "../task_utils/task_properties.h"

using namespace cost_saturation;
using namespace std;

namespace pdbs {
MaxSCPHeuristicExponential::MaxSCPHeuristicExponential(
    const options::Options &opts,
    Abstractions &&abstractions,
    CPHeuristics &&cp_heuristics)
    : MaxSCPHeuristic(opts, move(abstractions), move(cp_heuristics)) {
    unordered_set<LeafFactorID> aff_leaves;
    for (size_t abs = 0; abs < abstractions.size(); ++abs) {
        if (abstractions[abs]) {
            get_leaf_factors_of_pattern(patterns[abs], aff_leaves);
        }
    }
    affected_leaves = vector<LeafFactorID>(aff_leaves.begin(), aff_leaves.end());
}

void MaxSCPHeuristicExponential::compute_min_distance(
    const ExplicitStateCPG *cpg,
    vector<int> &member_state,
    int i,
    int sum_prices,
    int &min_d) const {

    if (sum_prices >= min_d){
        // we cannot possibly improve the heuristic
        return;
    }

    int number_states = cpg->get_number_states(affected_leaves[i]);
    LeafStateHash id(0);
    while (number_states > 0){
        if (cpg->has_leaf_state(id, affected_leaves[i])){
            --number_states;

            set_leaf_facts(id, affected_leaves[i], member_state);

            int price = cpg->get_cost_of_state(id, affected_leaves[i]);
            if (i < static_cast<int>(affected_leaves.size()) - 1){
                // recursion
                compute_min_distance(cpg, member_state, i + 1, sum_prices + price, min_d);
            } else {
                // end recursion
                int max_h = 0;
                vector<int> member_abs_state_ids = get_abstract_state_ids(abstraction_functions, member_state);
                for (const auto &cp_heuristic : cp_heuristics) {
                    int sum_h = cp_heuristic.compute_heuristic(member_abs_state_ids);
                    max_h = max(max_h, sum_h);
                    if (sum_prices + price + max_h >= min_d){
                        // we cannot possibly improve the heuristic
                        return;
                    }
                }
                min_d = min(min_d, sum_prices + price + max_h);
            }
        }
        ++id;
    }
}

int MaxSCPHeuristicExponential::compute_heuristic(const GlobalState &state) {
    vector<int> abstract_state_ids = get_abstract_state_ids(
        abstraction_functions, state);
    if (unsolvability_heuristic.is_unsolvable(abstract_state_ids)) {
        return DEAD_END;
    }

    if (affected_leaves.empty()){
        // compute maximal heuristic value for the center state
        int max_h = 0;
        for (const auto &cp_heuristic : cp_heuristics) {
            int sum_h = cp_heuristic.compute_heuristic(abstract_state_ids);
            if (sum_h > max_h) {
                max_h = sum_h;
            }
            if (max_h == INF) {
                break;
            }
        }
        return max_h;
    } else {
        // TODO in case there exists a center-only pattern that detects the center
        // state as a deadend, we can spare ourselves all the recursion!

        // compute minimum heuristic value across all member states
        vector<int> member_state(g_variable_domain.size(), -1);
        set_center_facts(state, member_state);
        int min_h = numeric_limits<int>::max();
        const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
        compute_min_distance(prices, member_state, 0, 0, min_h);
        return min_h;
    }
}

static shared_ptr<Evaluator> _parse(OptionParser &parser) {
    add_projection_options_to_parser(parser);
    cost_saturation::add_order_options_to_parser(parser);
    Heuristic::add_options_to_parser(parser);

    options::Options opts = parser.parse();
    if (parser.help_mode())
        return nullptr;

    if (parser.dry_run())
        return nullptr;

    // TODO: in modern FD, use the task from options
//    shared_ptr<AbstractTask> task = opts.get<shared_ptr<AbstractTask>>("transform");
    shared_ptr<AbstractTask> task = tasks::g_root_task;
    TaskProxy task_proxy(*task);
    vector<int> costs = task_properties::get_operator_costs(task_proxy);
    shared_ptr<ProjectionGenerator> projection_generator = make_shared<ProjectionGenerator>(opts);
    Abstractions abstractions = projection_generator->generate_abstractions(task);
    CPFunction cp_function = get_cp_function_from_options(opts);
    CPHeuristics cp_heuristics =
        get_cp_heuristic_collection_generator_from_options(opts).generate_cost_partitionings(
            task_proxy, abstractions, costs, cp_function);
    return make_shared<MaxSCPHeuristicExponential>(
        opts,
        move(abstractions),
        move(cp_heuristics));
}

static Plugin<Evaluator> _plugin("max_scp_exponential", _parse);
}
