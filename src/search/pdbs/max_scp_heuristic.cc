#include "max_scp_heuristic.h"

#include "canonical_pdbs_heuristic.h"
#include "pattern_filter.h"
#include "types.h"
#include "utils.h"

#include "../globals.h"
#include "../leaf_state_id.h"
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

#include "../utils/logging.h"

using namespace cost_saturation;
using namespace std;

namespace pdbs {
static void log_info_about_stored_lookup_tables(
    const Abstractions &abstractions,
    const CPHeuristics &cp_heuristics) {
    int num_abstractions = abstractions.size();

    // Print statistics about the number of lookup tables.
    int num_lookup_tables = num_abstractions * cp_heuristics.size();
    int num_stored_lookup_tables = 0;
    for (const auto &cp_heuristic: cp_heuristics) {
        num_stored_lookup_tables += cp_heuristic.get_num_lookup_tables();
    }
    utils::Log() << "Stored lookup tables: " << num_stored_lookup_tables << "/"
                 << num_lookup_tables << " = "
                 << num_stored_lookup_tables / static_cast<double>(num_lookup_tables)
                 << endl;

    // Print statistics about the number of stored values.
    int num_stored_values = 0;
    for (const auto &cp_heuristic : cp_heuristics) {
        num_stored_values += cp_heuristic.get_num_heuristic_values();
    }
    int num_total_values = 0;
    for (const auto &abstraction : abstractions) {
        num_total_values += abstraction->get_num_states();
    }
    num_total_values *= cp_heuristics.size();
    utils::Log() << "Stored values: " << num_stored_values << "/"
                 << num_total_values << " = "
                 << num_stored_values / static_cast<double>(num_total_values) << endl;
}

static AbstractionFunctions extract_abstraction_functions_from_useful_abstractions(
    const CPHeuristics &cp_heuristics,
    const UnsolvabilityHeuristic &unsolvability_heuristic,
    Abstractions &abstractions,
    vector<Pattern> &patterns) {
    int num_abstractions = abstractions.size();

    // Collect IDs of useful abstractions.
    vector<bool> useful_abstractions(num_abstractions, false);
    unsolvability_heuristic.mark_useful_abstractions(useful_abstractions);
    for (const auto &cp_heuristic : cp_heuristics) {
        cp_heuristic.mark_useful_abstractions(useful_abstractions);
    }

    AbstractionFunctions abstraction_functions;
    abstraction_functions.reserve(num_abstractions);
    assert(patterns.empty());
    patterns.reserve(num_abstractions);
    for (int i = 0; i < num_abstractions; ++i) {
        if (useful_abstractions[i]) {
            abstraction_functions.push_back(
                abstractions[i]->extract_abstraction_function());
            Projection *projection = dynamic_cast<Projection *>(abstractions[i].get());
            patterns.emplace_back(projection->get_pattern());
        } else {
            abstraction_functions.push_back(nullptr);
            patterns.emplace_back();
        }
    }
    return abstraction_functions;
}

MaxSCPHeuristic::MaxSCPHeuristic(
    const options::Options &opts,
    Abstractions &&abstractions,
    CPHeuristics &&cp_heuristics_)
    : Heuristic(opts),
      dec_lookup_type(opts.get<DecoupledPatternCollectionLookUp>("dec_lookup_type")),
      cp_heuristics(move(cp_heuristics_)),
      unsolvability_heuristic(abstractions, cp_heuristics) {
    log_info_about_stored_lookup_tables(abstractions, cp_heuristics);

    // We only need abstraction functions during search and no transition systems.
    // We additionally extract the patterns.
    abstraction_functions = extract_abstraction_functions_from_useful_abstractions(
        cp_heuristics, unsolvability_heuristic, abstractions, patterns);

    int num_abstractions = abstractions.size();
    int num_useless_abstractions = count(
        abstraction_functions.begin(), abstraction_functions.end(), nullptr);
    int num_useful_abstractions = num_abstractions - num_useless_abstractions;
    utils::Log() << "Useful abstractions: " << num_useful_abstractions << "/"
                 << num_abstractions << " = "
                 << static_cast<double>(num_useful_abstractions) / num_abstractions
                 << endl;

    // TODO: assertion for approx that all patterns used in each CPHeuristic only affect one leaf

    if (dec_lookup_type == DecoupledPatternCollectionLookUp::APPROX) {
        for (size_t abs = 0; abs < abstractions.size(); ++abs) {
            if (abstractions[abs]) {
                unordered_set<LeafFactorID> aff_leaves = get_leaf_factors_of_pattern(patterns[abs]);
                affected_leaves_by_abstraction.emplace_back(aff_leaves.begin(), aff_leaves.end());
            } else {
                affected_leaves_by_abstraction.emplace_back();
            }
        }
    }

    if (dec_lookup_type == DecoupledPatternCollectionLookUp::EXP) {
        unordered_set<LeafFactorID> aff_leaves;
        for (size_t abs = 0; abs < abstractions.size(); ++abs) {
            if (abstractions[abs]) {
                get_leaf_factors_of_pattern(patterns[abs], aff_leaves);
            }
        }
        affected_leaves = vector<LeafFactorID>(aff_leaves.begin(), aff_leaves.end());
    }

    if (dec_lookup_type == DecoupledPatternCollectionLookUp::SINGLE_LEAF) {
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
}

MaxSCPHeuristic::~MaxSCPHeuristic() {
    print_statistics();
}

void MaxSCPHeuristic::compute_min_distance_approx(
    const ExplicitStateCPG *cpg,
    const vector<int> &distances,
    const AbstractionFunction &abstraction_function,
    const vector<LeafFactorID> affected_leaves,
    vector<int> &member_state,
    int i,
    int sum_prices,
    int &min_d) const {

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
                compute_min_distance_approx(cpg, distances, abstraction_function, affected_leaves, member_state, i + 1, sum_prices + price, min_d);
            } else {
                // end recursion
                min_d = min(min_d, sum_prices + price + distances[abstraction_function.get_abstract_state_id(member_state)]);
            }
        }
        ++id;
    }
}

int MaxSCPHeuristic::compute_heuristic_approx(
    const GlobalState &state, const vector<int> &abstract_state_ids) const {
    int max_h = 0;
    const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    for (const auto &cp_heuristic : cp_heuristics) {
        int sum_h = 0;
        for (const auto &lookup_table : cp_heuristic.lookup_tables) {
            int abs_id = lookup_table.abstraction_id;
            int h;
            if (affected_leaves_by_abstraction[abs_id].empty()) {
                assert(utils::in_bounds(abs_id, abstract_state_ids));
                int state_id = abstract_state_ids[abs_id];
                assert(utils::in_bounds(state_id, lookup_table.h_values));
                h = lookup_table.h_values[state_id];
                assert(h >= 0);
                if (h == INF) {
                    return INF;
                }
            } else {
                // used to call get_value(vector<int>) for all member states
                vector<int> member_state(g_variable_domain.size(), -1);
                set_center_facts(state, member_state);
                compute_min_distance_approx(
                    prices,
                    lookup_table.h_values,
                    *abstraction_functions[abs_id],
                    affected_leaves_by_abstraction[abs_id],
                    member_state,
                    0,
                    0,
                    h);
            }
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

void MaxSCPHeuristic::compute_min_distance_decoupled_exponential(
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
                compute_min_distance_decoupled_exponential(cpg, member_state, i + 1, sum_prices + price, min_d);
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

int MaxSCPHeuristic::compute_heuristic_exp(
    const GlobalState &state, const vector<int> &abstract_state_ids) const {
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
        compute_min_distance_decoupled_exponential(prices, member_state, 0, 0, min_h);
        return min_h;
    }
}

int MaxSCPHeuristic::compute_min_distance_single_leaf(
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

            int sum_dists = 0;
            for (const auto &lookup_table : cp_heuristic.lookup_tables) {
                if (affecting_patterns[leaf].count(lookup_table.abstraction_id)) {
                    assert(utils::in_bounds(lookup_table.abstraction_id, abstract_state_ids));
                    int state_id = abstract_state_ids[lookup_table.abstraction_id];
                    assert(utils::in_bounds(state_id, lookup_table.h_values));
                    int h = lookup_table.h_values[state_id];
                    assert(h >= 0);
                    if (h == INF) {
                        return INF;
                    }
                    sum_dists += h;
                    if (price + sum_dists >= min_d){
                        break;
                    }
                    }
            }

            min_d = min(min_d, price + sum_dists);
        }
        ++id;
    }
    return min_d;
}

int MaxSCPHeuristic::compute_heuristic_single_leaf(
    const GlobalState &state, const vector<int> &abstract_state_ids) const {
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
                if (leaf < affecting_patterns.size() && !affecting_patterns[leaf].empty()){
                    sum_h += compute_min_distance_single_leaf(prices, cp_heuristic, member_state, leaf);
                }
            }
        }
        max_h = max(max_h, sum_h);
    }
    return max_h;
}

int MaxSCPHeuristic::compute_heuristic(const GlobalState &ancestor_state) {
    vector<int> abstract_state_ids = get_abstract_state_ids(
        abstraction_functions, ancestor_state);
    if (unsolvability_heuristic.is_unsolvable(abstract_state_ids)) {
        return DEAD_END;
    }

    switch (dec_lookup_type) {
        case DecoupledPatternCollectionLookUp::APPROX:
            return compute_heuristic_approx(ancestor_state, abstract_state_ids);
        case DecoupledPatternCollectionLookUp::EXP:
            return compute_heuristic_exp(ancestor_state, abstract_state_ids);
        case DecoupledPatternCollectionLookUp::SINGLE_LEAF:
            return compute_heuristic_single_leaf(ancestor_state, abstract_state_ids);
        default:
            cerr << "unknown value for DecoupledPatternCollectionLookUp" << endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
}

void MaxSCPHeuristic::print_statistics() const {
    int num_orders = num_best_order.size();
    int num_probably_superfluous = count(num_best_order.begin(), num_best_order.end(), 0);
    int num_probably_useful = num_orders - num_probably_superfluous;
    cout << "Number of times each order was the best order: "
         << num_best_order << endl;
    cout << "Probably useful orders: " << num_probably_useful << "/" << num_orders
         << " = " << 100. * num_probably_useful / num_orders << "%" << endl;
}

static shared_ptr<Evaluator> _parse(OptionParser &parser) {
    add_projection_options_to_parser(parser);
    cost_saturation::add_order_options_to_parser(parser);
    Heuristic::add_options_to_parser(parser);
    add_decoupled_lookup_types_option_to_parser(parser);

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
    return make_shared<MaxSCPHeuristic>(
        opts,
        move(abstractions),
        move(cp_heuristics));
}

static Plugin<Evaluator> _plugin("max_scp", _parse, "heuristics_cost_partitioning");
}
