#include "max_scp_heuristic_single_leaf.h"

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
MaxSCPHeuristicSingleLeaf::MaxSCPHeuristicSingleLeaf(
    const options::Options &opts,
    Abstractions &&abstractions,
    CPHeuristics &&cp_heuristics)
    : MaxSCPHeuristic(opts, move(abstractions), move(cp_heuristics)) {
    // Verify that every pattern affects at most one leaf factor.
    for (unique_ptr<Abstraction> &abstraction : abstractions) {
        Projection *projection = dynamic_cast<Projection *>(abstraction.get());
        unordered_set<LeafFactorID> affected_leaves = get_leaf_factors_of_pattern(projection->get_pattern());
        if (affected_leaves.size() > 1){
            cerr << "Every pattern may affect at most one leaf for SINGLE_LEAF decoupled lookup." << endl;
            utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
        }
    }

    vector<bool> is_leaf_pattern(patterns.size(), false);
    for (PatternID p_id = 0; p_id < static_cast<int>(patterns.size()); ++p_id) {
        for (int var : patterns[p_id]){
            LeafFactorID factor = g_belongs_to_factor[var];
            if (factor != LeafFactorID::CENTER){
                is_leaf_pattern[p_id] = true;
                if (affecting_patterns.size() <= factor){
                    affecting_patterns.resize(factor + 1);
                }
                affecting_patterns[factor].insert(p_id);
            }
        }
    }
    for (PatternID p_id = 0; p_id < static_cast<int>(patterns.size()); ++p_id){
        if (!is_leaf_pattern[p_id]){
            center_only_patterns.insert(p_id);
        }
    }
}

int MaxSCPHeuristicSingleLeaf::compute_min_distance(
    const ExplicitStateCPG *prices,
    const CostPartitioningHeuristic &cp_heuristic,
    vector<int> &member_state,
    LeafFactorID leaf) const {

    int min_d = numeric_limits<int>::max();

    int number_states = prices->get_number_states(leaf);
    LeafStateHash id(0);
    while (number_states > 0){
        if (prices->has_leaf_state(id, leaf)){
            --number_states;

            set_leaf_facts(id, leaf, member_state);

            int price = prices->get_cost_of_state(id, leaf);
            vector<int> abstract_state_ids = get_abstract_state_ids(abstraction_functions, member_state);

            bool is_dead_end_member_state = false;
            int sum_dists = 0;
            for (const auto &lookup_table : cp_heuristic.lookup_tables) {
                if (affecting_patterns[leaf].count(lookup_table.abstraction_id)) {
                    assert(utils::in_bounds(lookup_table.abstraction_id, abstract_state_ids));
                    int state_id = abstract_state_ids[lookup_table.abstraction_id];
                    assert(utils::in_bounds(state_id, lookup_table.h_values));
                    int h = lookup_table.h_values[state_id];
                    assert(h >= 0);
                    if (h == numeric_limits<int>::max()){
                        is_dead_end_member_state = true;
                        break;
                    }
                    sum_dists += h;
                    if (price + sum_dists >= min_d){
                        break;
                    }
                }
            }

            if (!is_dead_end_member_state){
                min_d = min(min_d, price + sum_dists);
            }
        }
        ++id;
    }
    return min_d;
}

int MaxSCPHeuristicSingleLeaf::compute_heuristic(const GlobalState &state) {
    vector<int> abstract_state_ids = get_abstract_state_ids(
        abstraction_functions, state);
    if (unsolvability_heuristic.is_unsolvable(abstract_state_ids)) {
        return DEAD_END;
    }

    int max_h = 0;
    for (const auto &cp_heuristic : cp_heuristics) {
        int sum_h = 0;
        if (affecting_patterns.empty()){
            // all patterns only affect the center
            sum_h = cp_heuristic.compute_heuristic(abstract_state_ids);
        } else {
            for (const auto &lookup_table : cp_heuristic.lookup_tables) {
                if (center_only_patterns.count(lookup_table.abstraction_id)) {
                    assert(utils::in_bounds(lookup_table.abstraction_id, abstract_state_ids));
                    int state_id = abstract_state_ids[lookup_table.abstraction_id];
                    assert(utils::in_bounds(state_id, lookup_table.h_values));
                    int h = lookup_table.h_values[state_id];
                    assert(h >= 0);
                    if (h == INF) {
                        return INF;
                    }
                    sum_h += h;
                }
            }
            const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
            vector<int> member_state(g_variable_domain.size(), -1);
            set_center_facts(state, member_state);
            for (LeafFactorID leaf(0); leaf < g_leaves.size(); ++leaf) {
                if (!affecting_patterns[leaf].empty()){
                    int h = compute_min_distance(prices, cp_heuristic, member_state, leaf);
                    if (h == numeric_limits<int>::max()){
                        // all reached leaf states are dead-ends
                        return numeric_limits<int>::max();
                    }
                    sum_h += h;
                }
            }
        }
        max_h = max(max_h, sum_h);
    }
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
    return make_shared<MaxSCPHeuristicSingleLeaf>(
        opts,
        move(abstractions),
        move(cp_heuristics));
}

static Plugin<Evaluator> _plugin("max_scp_single_leaf", _parse);
}
