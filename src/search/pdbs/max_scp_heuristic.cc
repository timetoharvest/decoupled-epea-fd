#include "max_scp_heuristic.h"

#include "canonical_pdbs_heuristic.h"
#include "pattern_filter.h"

#include "../globals.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../cost_saturation/cost_partitioning_heuristic_decoupled_approx.h"
#include "../cost_saturation/cost_partitioning_heuristic_decoupled_exp.h"
#include "../cost_saturation/cost_partitioning_heuristic_decoupled_single_leaf.h"
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
    const vector<CostPartitioningHeuristic> &cp_heuristics) {
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
    const vector<CostPartitioningHeuristic> &cp_heuristics,
    const UnsolvabilityHeuristic &unsolvability_heuristic,
    Abstractions &abstractions) {
    int num_abstractions = abstractions.size();

    // Collect IDs of useful abstractions.
    vector<bool> useful_abstractions(num_abstractions, false);
    unsolvability_heuristic.mark_useful_abstractions(useful_abstractions);
    for (const auto &cp_heuristic : cp_heuristics) {
        cp_heuristic.mark_useful_abstractions(useful_abstractions);
    }

    AbstractionFunctions abstraction_functions;
    abstraction_functions.reserve(num_abstractions);
    for (int i = 0; i < num_abstractions; ++i) {
        if (useful_abstractions[i]) {
            abstraction_functions.push_back(
                abstractions[i]->extract_abstraction_function());
        } else {
            abstraction_functions.push_back(nullptr);
        }
    }
    return abstraction_functions;
}

MaxSCPHeuristic::MaxSCPHeuristic(
    const options::Options &opts,
    Abstractions &&abstractions,
    vector<CostPartitioningHeuristic> &&cp_heuristics_)
    : Heuristic(opts),
      dec_lookup_type(opts.get<DecoupledPatternCollectionLookUp>("dec_lookup_type")),
      cp_heuristics(move(cp_heuristics_)),
      unsolvability_heuristic(abstractions, cp_heuristics) {
    log_info_about_stored_lookup_tables(abstractions, cp_heuristics);


    if (g_factoring && dec_lookup_type == DecoupledPatternCollectionLookUp::SINGLE_LEAF) {
        // Verify that every pattern affects at most one leaf factor.
        for (unique_ptr<Abstraction> &abstraction : abstractions) {
            Projection *projection = dynamic_cast<Projection *>(abstraction.get());
            unordered_set<LeafFactorID> affected_leaves = get_leaf_factors_of_pattern(projection->get_pattern());
            if (affected_leaves.size() > 1){
                cerr << "Every pattern may affect at most one leaf for SINGLE_LEAF decoupled lookup." << endl;
                utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
            }
        }
    }

    // TODO: need to extract the patterns

    // We only need abstraction functions during search and no transition systems.
    abstraction_functions = extract_abstraction_functions_from_useful_abstractions(
        cp_heuristics, unsolvability_heuristic, abstractions);

    int num_abstractions = abstractions.size();
    int num_useless_abstractions = count(
        abstraction_functions.begin(), abstraction_functions.end(), nullptr);
    int num_useful_abstractions = num_abstractions - num_useless_abstractions;
    utils::Log() << "Useful abstractions: " << num_useful_abstractions << "/"
                 << num_abstractions << " = "
                 << static_cast<double>(num_useful_abstractions) / num_abstractions
                 << endl;
}

MaxSCPHeuristic::~MaxSCPHeuristic() {
    print_statistics();
}

int compute_max_h(
    const CPHeuristics &cp_heuristics,
    const vector<int> &abstract_state_ids,
    vector<int> *num_best_order) {
    int max_h = 0;
    int best_id = -1;
    int current_id = 0;
    for (const CostPartitioningHeuristic &cp_heuristic : cp_heuristics) {
        int sum_h = cp_heuristic.compute_heuristic(abstract_state_ids);
        if (sum_h > max_h) {
            max_h = sum_h;
            best_id = current_id;
        }
        if (max_h == INF) {
            break;
        }
        ++current_id;
    }
    assert(max_h >= 0);

    if (num_best_order) {
        num_best_order->resize(cp_heuristics.size(), 0);
        if (best_id != -1) {
            ++(*num_best_order)[best_id];
        }
    }

    return max_h;
}

int MaxSCPHeuristic::compute_heuristic(const GlobalState &ancestor_state) {
    vector<int> abstract_state_ids = get_abstract_state_ids(
        abstraction_functions, ancestor_state);
    if (unsolvability_heuristic.is_unsolvable(abstract_state_ids)) {
        return DEAD_END;
    }

    // Copy of compute max_h in cost_saturation/utils.
    int max_h = 0;
    int best_id = -1;
    int current_id = 0;
    for (const CostPartitioningHeuristic &cp_heuristic : cp_heuristics) {
        int sum_h = cp_heuristic.compute_heuristic(abstract_state_ids);
        if (sum_h > max_h) {
            max_h = sum_h;
            best_id = current_id;
        }
        if (max_h == INF) {
            break;
        }
        ++current_id;
    }
    assert(max_h >= 0);

    num_best_order.resize(cp_heuristics.size(), 0);
    if (best_id != -1) {
        ++(num_best_order)[best_id];
    }

    return max_h;
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

CostPartitioningHeuristic compute_scp_approx(
    const Abstractions &abstractions,
    const vector<int> &order,
    vector<int> &remaining_costs,
    const vector<int> &) {
    assert(abstractions.size() == order.size());
    CostPartitioningHeuristicDecoupledApprox cp_heuristic;
    for (int pos : order) {
        const Abstraction &abstraction = *abstractions[pos];
        vector<int> h_values = abstraction.compute_goal_distances(remaining_costs);
        vector<int> saturated_costs = abstraction.compute_saturated_costs(h_values);
        cp_heuristic.add_h_values(pos, move(h_values));
        reduce_costs(remaining_costs, saturated_costs);
    }
    return cp_heuristic;
}

CostPartitioningHeuristic compute_scp_exp(
    const Abstractions &abstractions,
    const vector<int> &order,
    vector<int> &remaining_costs,
    const vector<int> &) {
    assert(abstractions.size() == order.size());
    CostPartitioningHeuristicDecoupledExp cp_heuristic;
    for (int pos : order) {
        const Abstraction &abstraction = *abstractions[pos];
        vector<int> h_values = abstraction.compute_goal_distances(remaining_costs);
        vector<int> saturated_costs = abstraction.compute_saturated_costs(h_values);
        cp_heuristic.add_h_values(pos, move(h_values));
        reduce_costs(remaining_costs, saturated_costs);
    }
    return cp_heuristic;
}

CostPartitioningHeuristic compute_scp_single_leaf(
    const Abstractions &abstractions,
    const vector<int> &order,
    vector<int> &remaining_costs,
    const vector<int> &) {
    assert(abstractions.size() == order.size());
    CostPartitioningHeuristicDecoupledSingleLeaf cp_heuristic;
    for (int pos : order) {
        const Abstraction &abstraction = *abstractions[pos];
        vector<int> h_values = abstraction.compute_goal_distances(remaining_costs);
        vector<int> saturated_costs = abstraction.compute_saturated_costs(h_values);
        cp_heuristic.add_h_values(pos, move(h_values));
        reduce_costs(remaining_costs, saturated_costs);
    }
    return cp_heuristic;
}

static CPFunction get_scp_function_from_options(const Options &opts) {
    DecoupledPatternCollectionLookUp dec_lookup_type = opts.get<DecoupledPatternCollectionLookUp>("dec_lookup_type");
    CPFunction cp_function = nullptr;
    if (dec_lookup_type == DecoupledPatternCollectionLookUp::APPROX) {
        cp_function = compute_scp_approx;
    } else if (dec_lookup_type == DecoupledPatternCollectionLookUp::EXP) {
        cp_function = compute_scp_exp;
    } else if (dec_lookup_type == DecoupledPatternCollectionLookUp::SINGLE_LEAF) {
        cp_function = compute_scp_single_leaf;
    } else {
        ABORT("Invalid value for dec_lookup_type.");
    }
    return cp_function;
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
    CPFunction cp_function = get_scp_function_from_options(opts);
    vector<CostPartitioningHeuristic> cp_heuristics =
        get_cp_heuristic_collection_generator_from_options(opts).generate_cost_partitionings(
            task_proxy, abstractions, costs, cp_function);
    return make_shared<MaxSCPHeuristic>(
        opts,
        move(abstractions),
        move(cp_heuristics));
}

static Plugin<Evaluator> _plugin("scp", _parse, "heuristics_cost_partitioning");
}
