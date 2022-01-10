#include "max_scp_heuristic_approximate.h"

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
MaxSCPHeuristicApproximate::MaxSCPHeuristicApproximate(
    const options::Options &opts,
    Abstractions &&abstractions,
    CPHeuristics &&cp_heuristics)
    : MaxSCPHeuristic(opts, move(abstractions), move(cp_heuristics)) {
    // TODO: assertion for approx that all patterns used in each CPHeuristic only affect one leaf

    for (size_t abs = 0; abs < abstractions.size(); ++abs) {
        if (abstractions[abs]) {
            unordered_set<LeafFactorID> aff_leaves = get_leaf_factors_of_pattern(patterns[abs]);
            affected_leaves_by_abstraction.emplace_back(aff_leaves.begin(), aff_leaves.end());
        } else {
            affected_leaves_by_abstraction.emplace_back();
        }
    }
}

static void compute_min_distance(
    const ExplicitStateCPG *cpg,
    const vector<int> &distances,
    const AbstractionFunction &abstraction_function,
    const vector<LeafFactorID> &affected_leaves,
    vector<int> &member_state,
    int i,
    int sum_prices,
    int &min_d) {

    if (sum_prices >= min_d){
        // we cannot possibly improve the heuristic
        return;
    }

    LeafFactorID leaf = affected_leaves[i];

    int number_states = cpg->get_number_states(leaf);
    LeafStateHash id(0);
    while (number_states > 0){
        if (cpg->has_leaf_state(id, leaf)){
            --number_states;

            set_leaf_facts(id, leaf, member_state);

            int price = cpg->get_cost_of_state(id, leaf);
            if (i < static_cast<int>(affected_leaves.size()) - 1){
                // recursion
                compute_min_distance(cpg, distances, abstraction_function, affected_leaves, member_state, i + 1, sum_prices + price, min_d);
            } else {
                // end recursion
                min_d = min(min_d, sum_prices + price + distances[abstraction_function.get_abstract_state_id(member_state)]);
            }
        }
        ++id;
    }
}

int MaxSCPHeuristicApproximate::compute_pdb_value_with_prices(
    const GlobalState &state,
    const vector<int> &abstract_state_ids,
    const CostPartitioningHeuristic::LookupTable &lookup_table,
    const ExplicitStateCPG* prices) const {
    int min_h = numeric_limits<int>::max();
    int abs_id = lookup_table.abstraction_id;
    if (affected_leaves_by_abstraction[abs_id].empty()) {
        assert(utils::in_bounds(abs_id, abstract_state_ids));
        int state_id = abstract_state_ids[abs_id];
        assert(utils::in_bounds(state_id, lookup_table.h_values));
        min_h = lookup_table.h_values[state_id];
    } else {
        // used to call get_value(vector<int>) for all member states
        vector<int> member_state(g_variable_domain.size(), -1);
        set_center_facts(state, member_state);
        compute_min_distance(
            prices,
            lookup_table.h_values,
            *abstraction_functions[abs_id],
            affected_leaves_by_abstraction[abs_id],
            member_state,
            0,
            0,
            min_h);
    }
    return min_h;
}

int MaxSCPHeuristicApproximate::compute_heuristic(const GlobalState &state) {
    vector<int> abstract_state_ids = get_abstract_state_ids(
        abstraction_functions, state);
    if (unsolvability_heuristic.is_unsolvable(abstract_state_ids)) {
        return DEAD_END;
    }

    int max_h = 0;
    const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    for (const auto &cp_heuristic : cp_heuristics) {
        int sum_h = 0;
        unordered_set<LeafFactorID> affected_leaves_so_far;
        for (const auto &lookup_table : cp_heuristic.lookup_tables) {
            const vector<LeafFactorID> &abs_aff_leaves = affected_leaves_by_abstraction[lookup_table.abstraction_id];
            for (LeafFactorID leaf_id : abs_aff_leaves) {
                if (affected_leaves_so_far.count(leaf_id)) {
                    continue;
                }
            }
            affected_leaves_so_far.insert(abs_aff_leaves.begin(), abs_aff_leaves.end());
            int h = compute_pdb_value_with_prices(state, abstract_state_ids, lookup_table, prices);
            sum_h += h;
            assert(sum_h >= 0);
        }

        if (sum_h > max_h) {
            max_h = sum_h;
        }
        if (max_h == INF) {
            break;
        }
    }
    assert(max_h >= 0);
    return max_h;
}

static shared_ptr<Evaluator> _parse(OptionParser &parser) {
    add_projection_options_to_parser(parser);
    add_saturator_option(parser);
    add_order_options_to_parser(parser);
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
    return make_shared<MaxSCPHeuristicApproximate>(
        opts,
        move(abstractions),
        move(cp_heuristics));
}

static Plugin<Evaluator> _plugin("max_scp_approx", _parse);
}
