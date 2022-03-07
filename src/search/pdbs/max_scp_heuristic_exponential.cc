#include "max_scp_heuristic_exponential.h"

#include "decoupled_canonical_pdbs_exponential.h"
#include "decoupled_pdb_utils.h"
#include "pattern_filter.h"
#include "types.h"

#include "../globals.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../compliant_paths/cpg_storage.h"
#include "../compliant_paths/explicit_state_cpg.h"

#include "../cost_saturation/cost_partitioning_heuristic.h"
#include "../cost_saturation/cost_partitioning_heuristic_collection_generator.h"
#include "../cost_saturation/projection.h"
#include "../cost_saturation/saturated_cost_partitioning_heuristic.h"
#include "../cost_saturation/utils.h"

#include "../tasks/root_task.h"
#include "../task_utils/task_properties.h"

#include "../utils/logging.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

using namespace cost_saturation;
using namespace std;

namespace pdbs {
MaxSCPHeuristicExponential::MaxSCPHeuristicExponential(
    const options::Options &opts,
    Abstractions &&abstractions,
    CPHeuristics &&cp_heuristics,
    ExplicitLeafSortingType sort_leaves_type,
    bool early_termination,
    const shared_ptr<utils::RandomNumberGenerator> &rng)
    : MaxSCPHeuristic(opts, move(abstractions), move(cp_heuristics)),
      sort_leaves_type(sort_leaves_type),
      early_termination(early_termination) {
    unordered_set<LeafFactorID> aff_leaves;
    affected_leaves_by_pattern.resize(patterns.size());
    vector<int> num_patterns_per_leaf(g_leaves.size());
    for (PatternID pid = 0; pid < static_cast<int>(patterns.size()); ++pid) {
        unordered_set<LeafFactorID> tmp = get_leaf_factors_of_pattern(patterns[pid]);
        affected_leaves_by_pattern[pid] = vector<LeafFactorID>(tmp.begin(), tmp.end());
        if (affected_leaves_by_pattern[pid].empty()) {
            center_only_patterns.push_back(pid);
        } else {
            for (LeafFactorID leaf : affected_leaves_by_pattern[pid]) {
                num_patterns_per_leaf[leaf]++;
                aff_leaves.insert(leaf);
            }
        }
    }
    affected_leaves = vector<LeafFactorID>(aff_leaves.begin(), aff_leaves.end());

    if (sort_leaves_type == ExplicitLeafSortingType::STATIC) {
        // sort leaf factors by descending number of affecting patterns
        sort(affected_leaves.begin(), affected_leaves.end(),
             [&](const LeafFactorID &l1, const LeafFactorID &l2) -> bool {
                 return num_patterns_per_leaf[l1] > num_patterns_per_leaf[l2];
             });
    } else if (sort_leaves_type == ExplicitLeafSortingType::LEAFID){
        // sort by ascending LeafFactorID
        sort(affected_leaves.begin(), affected_leaves.end());
    } else if (sort_leaves_type == ExplicitLeafSortingType::RANDOM){
        // first sort by LeafFactorID to have a deterministic start, then shuffle
        sort(affected_leaves.begin(), affected_leaves.end());
        rng->shuffle(affected_leaves);
    }

    apply_resorted_affected_leaves();
}

void MaxSCPHeuristicExponential::apply_resorted_affected_leaves() const {
    patterns_by_rec_depth = vector<vector<PatternID>>(g_leaves.size());
    vector<int> rec_depth_by_pattern(patterns.size(), -1);
    vector<int> leaf_indices(g_leaves.size());
    for (int i = 0; i < static_cast<int>(affected_leaves.size()); ++i){
        leaf_indices[affected_leaves[i]] = i;
    }
    for (PatternID pid = 0; pid < static_cast<int>(patterns.size()); ++pid) {
        int max_d = -1;
        for (LeafFactorID leaf: affected_leaves_by_pattern[pid]){
            max_d = max(max_d, leaf_indices[leaf]);
            if (max_d == static_cast<int>(affected_leaves.size() - 1)){
                break;
            }
        }
        rec_depth_by_pattern[pid] = max_d;
        if (max_d != -1) {
            // not a center-only pattern
            patterns_by_rec_depth[max_d].push_back(pid);
        }
    }
}

void MaxSCPHeuristicExponential::compute_min_distance_decoupled_exponential(
    const ExplicitStateCPG *cpg,
    vector<int> &member_state,
    int i,
    int sum_prices,
    const vector<int> &partial_h_by_cp_heur,
    int &final_h) const {

//    cout << "recursion depth/leaf: " << i << endl;
//    cout << "sum prices: " << sum_prices << endl;
//    cout << "min_h: " << final_h << endl;

    if (early_termination) {
        int max_over_partial_h = *max_element(partial_h_by_cp_heur.begin(), partial_h_by_cp_heur.end());
        if (sum_prices + max_over_partial_h >= final_h) {
            // TODO: I don't think we need this since we check this *before* each recursive call
            // we cannot possibly improve the heuristic
//            cout << "skipping at start" << endl;
            return;
        }
    }

    int number_states = cpg->get_number_states(affected_leaves[i]);
    for (LeafStateHash id(0); id < g_state_registry->size(affected_leaves[i]); ++id) {
        if (number_states == 0){
            break;
        }
        if (cpg->has_leaf_state(id, affected_leaves[i])){
            --number_states;

            set_leaf_facts(id, affected_leaves[i], member_state);
//            cout << "current member state: " << member_state << endl;
            int price = cpg->get_cost_of_state(id, affected_leaves[i]);

            bool prune = false;
            vector<int> member_abs_state_ids = get_abstract_state_ids(abstraction_functions, member_state);
            vector<int> new_partial_h_by_cp_heur(partial_h_by_cp_heur);
            int max_over_partial_h = 0;
            for (size_t h_id = 0; h_id < cp_heuristics.size(); ++h_id) {
//                cout << "h_id " << h_id << endl;
                const auto &cp_heuristic = cp_heuristics[h_id];
                int sum_h = partial_h_by_cp_heur[h_id];
//                cout << "already have a value of: " << sum_h << endl;
                for (const auto &lookup_table : cp_heuristic.lookup_tables) {
                    int abs_id = lookup_table.abstraction_id;
                    if (find(patterns_by_rec_depth[i].begin(), patterns_by_rec_depth[i].end(), abs_id ) != patterns_by_rec_depth[i].end()) {
                        assert(utils::in_bounds(abs_id, member_abs_state_ids));
                        int state_id = member_abs_state_ids[abs_id];
                        assert(utils::in_bounds(state_id, lookup_table.h_values));
                        int h = lookup_table.h_values[state_id];
                        assert(h >= 0);
                        if (h == INF || (early_termination && sum_prices + price + h >= final_h)) {
                            // deadend we cannot possibly improve the heuristic
                            prune = true;
                            break;
                        }
                        sum_h += h;
                        assert(sum_h >= 0);
//                        cout << "berechne abs_id " << abs_id << " für cp h_id " << h_id << endl;
                    }
                }
//                cout << "new value " << sum_h << endl;

                if (prune) {
                    // we cannot possibly improve the heuristic
                    break;
                }

                new_partial_h_by_cp_heur[h_id] = sum_h;
                max_over_partial_h = max(max_over_partial_h, sum_h);

                if (early_termination && sum_prices + price + max_over_partial_h >= final_h) {
                    // deadend or we cannot possibly improve the heuristic
                    prune = true;
                    break;
                }
            }

            if (prune){
                // deadend or we cannot possibly improve the heuristic
//                cout << "cannot improve" << endl;
                continue;
            }

            if (i < static_cast<int>(affected_leaves.size()) - 1){
                // recursion
                compute_min_distance_decoupled_exponential(cpg,
                                                           member_state,
                                                           i + 1,
                                                           sum_prices + price,
                                                           new_partial_h_by_cp_heur,
                                                           final_h);
            } else {
                // end recursion
//                cout << "final values: " << new_partial_h_by_cp_heur << endl;
//                cout << "end recursion: " << sum_prices << "+" << price << "+" << max_over_partial_h << endl;
                final_h = min(final_h, sum_prices + price + max_over_partial_h);
            }
        }
    }
}

int MaxSCPHeuristicExponential::compute_heuristic(const GlobalState &state) {
    int max_h;
    if (affected_leaves.empty()){
        // compute maximal heuristic value for the center state
        vector<int> abstract_state_ids = get_abstract_state_ids(
            abstraction_functions, state);
        max_h = compute_max_h(cp_heuristics, abstract_state_ids);
    } else {
        // compute minimum heuristic value across all member states
        vector<int> member_state(g_variable_domain.size(), -1);
        set_center_facts(state, member_state);

        vector<int> member_abs_state_ids = get_abstract_state_ids(abstraction_functions, member_state);
        vector<int> partial_h_by_cp_heur(cp_heuristics.size(), 0);
        for (size_t h_id = 0; h_id < cp_heuristics.size(); ++h_id) {
            const auto &cp_heuristic = cp_heuristics[h_id];
            for (const auto &lookup_table : cp_heuristic.lookup_tables) {
                int abs_id = lookup_table.abstraction_id;
                if (find(center_only_patterns.begin(), center_only_patterns.end(), abs_id ) != center_only_patterns.end()) {
                    assert(utils::in_bounds(abs_id, member_abs_state_ids));
                    int state_id = member_abs_state_ids[abs_id];
                    assert(utils::in_bounds(state_id, lookup_table.h_values));
                    int h = lookup_table.h_values[state_id];
                    assert(h >= 0);
                    if (h == INF) {
                        // center state is detected as deadend
                        return DEAD_END;
                    }
                    partial_h_by_cp_heur[h_id] += h;
                    assert(partial_h_by_cp_heur[h_id] >= 0);
                }
            }
        }

        const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));

        if (sort_leaves_type == ExplicitLeafSortingType::DYNAMIC) {
            // sort leaf factors by ascending number of reached leaf states
            if (!is_sorted(affected_leaves.begin(), affected_leaves.end(),
                           [&](const LeafFactorID &l1, const LeafFactorID &l2) -> bool {
                               return prices->get_number_states(l1) < prices->get_number_states(l2);
                           })){
                sort(affected_leaves.begin(), affected_leaves.end(),
                     [&](const LeafFactorID &l1, const LeafFactorID &l2) -> bool {
                         return prices->get_number_states(l1) < prices->get_number_states(l2);
                     });
                apply_resorted_affected_leaves();
            }
        }

        max_h = numeric_limits<int>::max();
//        cout << "starting recursion" << endl;
        compute_min_distance_decoupled_exponential(prices, member_state, 0, 0, partial_h_by_cp_heur, max_h);
    }
//    cout << max_h << endl;
    assert(max_h >= 0);
    if (max_h == INF) {
        return DEAD_END;
    }
    return max_h;
}

static shared_ptr<Evaluator> _parse(OptionParser &parser) {
    prepare_parser_for_cost_partitioning_heuristic(parser);
    add_saturator_option(parser);
    add_order_options_to_parser(parser);
    add_exponential_options_to_parser(parser);
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
    Abstractions abstractions = generate_abstractions(
        task, opts.get_list<shared_ptr<AbstractionGenerator>>("abstractions"));
    CPFunction cp_function = get_cp_function_from_options(opts);
    CPHeuristics cp_heuristics =
        get_cp_heuristic_collection_generator_from_options(opts).generate_cost_partitionings(
            task_proxy, abstractions, costs, cp_function);
    return make_shared<MaxSCPHeuristicExponential>(
        opts,
        move(abstractions),
        move(cp_heuristics),
        opts.get<ExplicitLeafSortingType>("sort_leaves_type"),
        opts.get<bool>("early_termination"),
        utils::parse_rng_from_options(opts));
}

static Plugin<Evaluator> _plugin("max_scp_exp", _parse);
}
