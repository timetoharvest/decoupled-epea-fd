#include "pea_search.h"

#include "compliant_paths/compliant_path_graph.h"
#include "compliant_paths/pruning_options.h"
#include "factoring.h"
#include "g_evaluator.h"
#include "globals.h"
#include "heuristic.h"
#include "open_lists/tiebreaking_open_list.h"
#include "operator_selection_function.h"
#include "option_parser.h"
#include "pdbs/pdb_heuristic.h"
#include "pdbs/pattern_database.h"
#include "pdbs/pattern_filter.h"
#include "plugin.h"
#include "state_registry.h"
#include "sum_evaluator.h"
#include "task_utils/successor_generator.h"
#include "utils/system.h"
#include "utils/timer.h"

#include <cassert>
#include <cstdlib>
#include <limits>
#include <set>

using namespace std;

PEASearch::PEASearch(const Options &opts)
    : SearchEngine(opts),
      do_partial_expansion(opts.get<bool>("partial_expansion")),
      check_consistency(opts.get<bool>("check_consistency")),
      do_debug_cache(opts.get<bool>("debug_cache")),
      open_list(opts.get<shared_ptr<OpenList<StateID>>>("open")),
      f_evaluator(opts.get<shared_ptr<Evaluator>>("f_eval")),
      osf_mode(opts.get<OSFMode>("osf")),
      verify_osf(opts.get<bool>("verify_osf")),
      num_distinct_expansions(0),
      num_alpha_reexpansions(0),
      num_beta_deferred_edges(0),
      open_list_size(0),
      max_open_list_size(0),
      num_osf_deferred_operators(0),
      num_osf_deadend_skips(0) {

    if (g_factoring && g_factoring->get_search_type() != SDA) {
        cout << "pea_astar supports decoupled search only with search type "
             << "SDA (the default). Other search types are out of scope."
             << endl;
        utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    }

    if (g_symmetry_graph) {
        cout << "pea_astar does not support symmetry/orbit search "
             << "(unreviewed combination)." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    }

    if (osf_mode != OSF_NONE && !do_partial_expansion) {
        cout << "an operator selection function requires "
             << "partial_expansion=true (osf classifies against the "
             << "partial expansion threshold)." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    }

    if (verify_osf && osf_mode != OSF_DIRECT_PDB) {
        cout << "verify_osf=true requires osf=direct_pdb (only direct "
             << "predictions can be verified)." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    }

    if (osf_mode == OSF_DIRECT_PDB && g_factoring) {
        const PruningOptions &pruning =
            CompliantPathGraph::get_pruning_options();
        if (!(pruning.use_dominance_pruning()
              || pruning.use_exact_duplicate_checking())
            || pruning.do_advanced_pruning()) {
            cout << "osf=direct_pdb under decoupling supports only the "
                 << "plain dominance/duplicate pruning flavors. Frontier "
                 << "and advanced pruning variants use unreviewed "
                 << "compliant path graph classes." << endl;
            utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }

        if (pruning.include_min_price_in_g()) {
            cout << "osf=direct_pdb under decoupling requires "
                 << "include_price_in_g=false in the pruning option, "
                 << "e.g. pruning=dominance(include_price_in_g=false)."
                 << endl;
            utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }
    }
}

PEASearch::~PEASearch() {

    g_on_cpg_replaced = nullptr;
    g_on_open_node_dominance_closed = nullptr;
}

void PEASearch::initialize() {

    cout << "Conducting partial expansion A* search"
         << (do_partial_expansion ? "" : " [degenerate A* mode: partial_expansion=false]")
         << ", (real) bound = " << bound << endl;
    assert(open_list != nullptr);

    set<Heuristic*> hset;
    open_list->get_involved_heuristics(hset);
    for (Heuristic *h : hset) {
        estimate_heuristics.push_back(h);
        search_progress.add_heuristic(h);
    }

    if (f_evaluator) {
        f_evaluator->get_involved_heuristics(hset);
    }
    for (Heuristic *h : hset) {
        heuristics.push_back(h);
    }
    assert(!heuristics.empty());

    if (heuristics.size() != 1) {
        cout << "pea_astar requires eval to be a single heuristic. "
             << "The given eval involves " << heuristics.size()
             << " heuristics." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    }

    if (osf_mode == OSF_DIRECT_PDB) {

        if (has_axioms() || has_conditional_effects()) {
            cout << "osf=direct_pdb does not support axioms or "
                 << "conditional effects (successor pattern values must "
                 << "be computable from the operator alone)." << endl;
            utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }
        pdbs::PDBHeuristic *pdb_heuristic =
            dynamic_cast<pdbs::PDBHeuristic *>(heuristics[0]);
        if (!pdb_heuristic) {
            cout << "osf=direct_pdb requires eval to be a single pdb(...) "
                 << "heuristic. The direct OSF predicts successor h values "
                 << "from that PDB's lookup table." << endl;
            utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }

        if (g_factoring
            && !pdbs::get_leaf_factors_of_pattern(
                    pdb_heuristic->get_pattern_database().get_pattern())
                    .empty()) {
            cout << "osf=direct_pdb under decoupling requires a "
                 << "CENTER ONLY pattern, e.g. pdb(pattern=greedy("
                 << "max_states=1000000, "
                 << "pattern_filter=n_leaves_patterns(n=0))). The "
                 << "configured pattern touches leaf variables. Their "
                 << "successor values live in the pricing function and "
                 << "are not operator predictable." << endl;
            utils::exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }

        if (g_factoring) {
            bool pattern_has_goal_var = false;
            for (int var : pdb_heuristic->get_pattern_database()
                               .get_pattern()) {
                for (const auto &goal : g_goal) {
                    if (goal.first == var) {
                        pattern_has_goal_var = true;
                    }
                }
            }
            if (!pattern_has_goal_var) {
                cout << "WARNING: the center only pattern contains no "
                     << "goal variable. The PDB is identically 0 and the "
                     << "direct OSF degrades to Delta g only partitioning "
                     << "(the documented goalless center limit)." << endl;
            }
        }

        direct_osf = make_unique<DirectPDBOSF>(
            pdb_heuristic->get_pattern_database());
    }

    g_on_cpg_replaced = [this](StateID id) {
        flush_partial_expansion(id, "CPG replaced in place, I7");
    };
    g_on_open_node_dominance_closed = [this](StateID id) {
        flush_partial_expansion(id, "dominance closed placeholder, I5");
    };

    const GlobalState &initial_state = g_initial_state();
    for (Heuristic *h : heuristics) {
        h->evaluate(initial_state);
    }
    open_list->evaluate(0, false);
    search_progress.inc_evaluated_states();
    search_progress.inc_evaluations(heuristics.size());

    if (open_list->is_dead_end()) {
        cout << "Initial state is a dead end." << endl;
    } else {
        search_progress.get_initial_h_values();
        if (f_evaluator) {
            f_evaluator->evaluate(0, false);
            search_progress.report_f_value(f_evaluator->get_value());
        }
        search_progress.check_h_progress(0);
        SearchNode node = search_space.get_node(initial_state);
        node.open_initial(heuristics[0]->get_value());

        insert_into_open_list(initial_state.get_id());
    }
}

void PEASearch::statistics() const {

    search_progress.print_statistics();
    search_space.statistics();

    cout << "Distinct expansions: " << num_distinct_expansions
         << " state(s)." << endl;
    cout << "Partial re-expansions (alpha): " << num_alpha_reexpansions
         << endl;
    cout << "Deferred surplus edges (beta): " << num_beta_deferred_edges
         << endl;
    cout << "Maximum open list size: " << max_open_list_size << endl;

    if (osf_mode == OSF_DIRECT_PDB) {
        cout << "Deferred surplus operators (osf): "
             << num_osf_deferred_operators << endl;
        cout << "Dead-end successors never generated (osf): "
             << num_osf_deadend_skips << endl;
    }
}

void PEASearch::insert_into_open_list(StateID id) {

    open_list_size += open_list->insert(id);
    max_open_list_size = max(max_open_list_size, open_list_size);
}

void PEASearch::flush_partial_expansion(StateID id, const char *reason) {

    size_t erased_ops = osf_cache.erase(id.hash());
    if (do_debug_cache && erased_ops > 0) {
        cout << "[pea cache] flush (" << reason << "): state " << id
             << " discarded its parked operators (reclassified on next "
             << "pop)" << endl;
    }

    auto it = surplus_cache.find(id.hash());
    if (it == surplus_cache.end()) {
        return;
    }
    if (do_debug_cache) {
        cout << "[pea cache] flush (" << reason << "): performing "
             << it->second.size() << " parked insertion(s) of state " << id
             << " ahead of schedule, then dropping its entry" << endl;
    }

    int saved_inc = g_inc_g_by;

    vector<DeferredEdge> edges = std::move(it->second);
    surplus_cache.erase(it);
    for (const DeferredEdge &edge : edges) {
        release_deferred_edge(edge);
    }
    g_inc_g_by = saved_inc;
}

void PEASearch::drop_partial_expansion(StateID id, const char *reason) {

    size_t erased = surplus_cache.erase(id.hash());

    erased += osf_cache.erase(id.hash());
    if (do_debug_cache && erased > 0) {
        cout << "[pea cache] drop (" << reason << "): state " << id
             << " discarded its parked edges/operators (provably "
             << "irrelevant)" << endl;
    }
}

void PEASearch::reinsert_partially_expanded(SearchNode &node, int next_f) {

    node.reopen_unchanged();

    assert(node.get_h() >= 0);
    assert(next_f - node.get_h() > node.get_g());
    if (do_debug_cache) {

        size_t parked = 0;
        auto edge_it = surplus_cache.find(node.get_state_id().hash());
        if (edge_it != surplus_cache.end()) {
            parked += edge_it->second.size();
        }
        auto op_it = osf_cache.find(node.get_state_id().hash());
        if (op_it != osf_cache.end()) {
            parked += op_it->second.ops.size();
        }
        cout << "[pea cache] reinsert: state " << node.get_state_id()
             << " goes back into OPEN under key [F=" << next_f
             << ", h=" << node.get_h() << "] as the placeholder for its "
             << parked << " still parked edge(s)/operator(s)" << endl;
    }
    heuristics[0]->set_evaluator_value(node.get_h());
    open_list->evaluate(next_f - node.get_h(), false);
    insert_into_open_list(node.get_state_id());
}

void PEASearch::release_deferred_edge(const DeferredEdge &edge) {

    GlobalState succ_state = g_state_registry->lookup_state(edge.succ_id);
    SearchNode succ_node = search_space.get_node(succ_state);

    assert(!succ_node.is_new());
    if (succ_node.is_dead_end()) {
        return;
    }

    if (succ_node.is_closed()) {

        if (do_debug_cache) {
            cout << "[pea cache] release: parked insertion of "
                 << edge.succ_id << " (f=" << edge.f << ") is moot. The "
                 << "state is CLOSED (expanded or dominance closed)" << endl;
        }
        return;
    }

    if (succ_node.get_g() < edge.edge_g) {

        if (do_debug_cache) {
            cout << "[pea cache] release: parked insertion of "
                 << edge.succ_id << " (f=" << edge.f << ") became OBSOLETE "
                 << "(state now has cheaper g=" << succ_node.get_g()
                 << " < " << edge.edge_g << "). Dropped." << endl;
        }
        return;
    }

    assert(succ_node.get_g() == edge.edge_g);
    assert(!succ_node.is_h_dirty());
    if (do_debug_cache) {
        cout << "[pea cache] release: performing scheduled insertion of "
             << edge.succ_id << " (f=" << edge.f << ", op '"
             << g_operators[edge.op_id].get_name() << "') into OPEN" << endl;
    }
    heuristics[0]->set_evaluator_value(succ_node.get_h());
    open_list->evaluate(succ_node.get_g(), false);
    if (!insert_state(succ_node)) {
        return;
    }
    insert_into_open_list(edge.succ_id);
}

void PEASearch::expand_from_cache(SearchNode &node) {
    auto it = surplus_cache.find(node.get_state_id().hash());
    assert(it != surplus_cache.end());
    vector<DeferredEdge> &edges = it->second;
    assert(!edges.empty());

    int layer_f = numeric_limits<int>::max();
    for (const DeferredEdge &edge : edges) {
        layer_f = min(layer_f, edge.f);
    }
    if (do_debug_cache) {
        cout << "[pea cache] serve: state " << node.get_state_id()
             << " repopped. Releasing its f=" << layer_f << " layer out of "
             << edges.size() << " parked edge(s)" << endl;
    }

    vector<DeferredEdge> remaining;
    remaining.reserve(edges.size());
    int next_f = numeric_limits<int>::max();
    for (const DeferredEdge &edge : edges) {
        if (edge.f == layer_f) {

            release_deferred_edge(edge);
        } else {
            next_f = min(next_f, edge.f);
            remaining.push_back(edge);
        }
    }

    if (remaining.empty()) {

        if (do_debug_cache) {
            cout << "[pea cache] exhausted: state " << node.get_state_id()
                 << " has no parked edges left. It stays CLOSED."
                 << endl;
        }
        surplus_cache.erase(it);
    } else {
        it->second = std::move(remaining);
        reinsert_partially_expanded(node, next_f);
    }
}

void PEASearch::process_operator(SearchNode &node, const GlobalState &s,
                                 OperatorID op_id, int threshold_f,
                                 vector<DeferredEdge> &deferred, int &next_f,
                                 int predicted_h) {

    const Operator &op = g_operators[op_id];

    int goal_cost = max(0, CompliantPathGraph::get_min_leaf_goal_cost()
                           - node.get_leaf_part_g());
    if (node.get_g() + get_adjusted_cost(op) + goal_cost >= bound) {
        return;
    }

    GlobalState succ_state = g_state_registry->get_successor_state(s, op);
    search_progress.inc_generated();

    int captured_inc = g_inc_g_by;

    SearchNode succ_node = search_space.get_node(succ_state);

    if (succ_node.is_dead_end()) {

        if (verify_osf && predicted_h != NO_PREDICTION
            && predicted_h != numeric_limits<int>::max()) {
            report_osf_mismatch(op, predicted_h,
                                numeric_limits<int>::max());
        }
        return;
    }

    if (verify_osf && predicted_h != NO_PREDICTION && !succ_node.is_new()
        && predicted_h != succ_node.get_h()) {
        report_osf_mismatch(op, predicted_h, succ_node.get_h());
    }

    if (succ_node.is_new()) {

        for (Heuristic *h : heuristics) {
            h->reach_state(s, op_id, succ_state);
        }

        for (Heuristic *h : heuristics) {
            h->evaluate(succ_state);
        }
        succ_node.clear_h_dirty();
        search_progress.inc_evaluated_states();
        search_progress.inc_evaluations(heuristics.size());

        open_list->evaluate(node.get_g() + get_adjusted_cost(op) + captured_inc,
                            false);
        if (open_list->is_dead_end()) {

            if (verify_osf && predicted_h != NO_PREDICTION
                && predicted_h != numeric_limits<int>::max()) {
                report_osf_mismatch(op, predicted_h,
                                    numeric_limits<int>::max());
            }
            succ_node.mark_as_dead_end();
            search_progress.inc_dead_ends();
            return;
        }

        int succ_h = heuristics[0]->get_heuristic();

        if (verify_osf && predicted_h != NO_PREDICTION
            && predicted_h != succ_h) {
            report_osf_mismatch(op, predicted_h, succ_h);
        }
        if (check_consistency
            && (node.get_h() - get_adjusted_cost(op) - captured_inc) > succ_h) {

            cout << "WARNING: heuristic is inconsistent" << endl;
            check_consistency = false;
        }

        g_inc_g_by = captured_inc;
        succ_node.open(succ_h, node, op_id);

        if (!insert_state(succ_node)) {

            return;
        }

        int succ_f = succ_node.get_g() + succ_node.get_h();
        if (succ_f <= threshold_f) {

            insert_into_open_list(succ_state.get_id());

            search_progress.check_h_progress(succ_node.get_g());
        } else {

            deferred.emplace_back(op_id, succ_state.get_id(), succ_f,
                                  succ_node.get_g());
            ++num_beta_deferred_edges;
            next_f = min(next_f, succ_f);
            if (do_debug_cache) {
                cout << "[pea cache] defer: parent " << node.get_state_id()
                     << " parks NEW successor " << succ_state.get_id()
                     << " (f=" << succ_f << " > F=" << threshold_f
                     << ") via op '" << op.get_name() << "'" << endl;
            }
        }
    } else if (succ_node.get_g()
               > node.get_g() + get_adjusted_cost(op) + captured_inc) {

        if (!g_factoring || !PruningOptions::ignore_current_state()) {

            if (succ_node.is_closed()) {
                search_progress.inc_reopened();
            }

            g_inc_g_by = captured_inc;
            succ_node.reopen(node, op_id);

            flush_partial_expansion(succ_state.get_id(), "cheaper g, I6");
            bool was_dirty = succ_node.is_h_dirty();
            if (was_dirty) {

                for (Heuristic *h : heuristics) {
                    h->evaluate(succ_state);
                }
                succ_node.clear_h_dirty();
                search_progress.inc_evaluated_states();
                search_progress.inc_evaluations(heuristics.size());
                int new_h = heuristics[0]->get_heuristic();
                if (new_h < succ_node.get_h()) {
                    succ_node.decrease_h(new_h);
                }
                if (check_consistency
                    && (node.get_h() - get_adjusted_cost(op)
                        - captured_inc) > new_h) {
                    cout << "WARNING: heuristic is inconsistent" << endl;
                    check_consistency = false;
                }
            }

            int succ_f = succ_node.get_g() + succ_node.get_h();
            if (!insert_state(succ_node)) {

                return;
            }
            if (succ_f <= threshold_f) {
                if (!was_dirty) {
                    heuristics[0]->set_evaluator_value(succ_node.get_h());
                }
                open_list->evaluate(succ_node.get_g(), false);
                insert_into_open_list(succ_state.get_id());
            } else {

                deferred.emplace_back(op_id, succ_state.get_id(), succ_f,
                                      succ_node.get_g());
                ++num_beta_deferred_edges;
                next_f = min(next_f, succ_f);
                if (do_debug_cache) {
                    cout << "[pea cache] defer: parent "
                         << node.get_state_id()
                         << " applied a cheaper path to "
                         << succ_state.get_id()
                         << " and parks only its insertion (f=" << succ_f
                         << " > F=" << threshold_f << ") via op '"
                         << op.get_name() << "'" << endl;
                }
            }
        }
    }

}

void PEASearch::expand_from_osf_cache(SearchNode &node) {

    auto it = osf_cache.find(node.get_state_id().hash());
    assert(it != osf_cache.end());
    assert(it->second.parent_id == node.get_state_id());
    vector<DeferredOperator> ops = std::move(it->second.ops);
    osf_cache.erase(it);
    assert(!ops.empty());

    int layer_f = numeric_limits<int>::max();
    for (const DeferredOperator &dop : ops) {
        layer_f = min(layer_f, dop.pred_f);
    }
    if (do_debug_cache) {
        cout << "[pea cache] serve: state " << node.get_state_id()
             << " repopped. Generating its predicted f=" << layer_f
             << " operator layer out of " << ops.size()
             << " parked operator(s)" << endl;
    }

    GlobalState s = node.get_state();
    vector<DeferredOperator> remaining;
    remaining.reserve(ops.size());
    int next_f = numeric_limits<int>::max();

    vector<DeferredEdge> no_edges;
    int unused_next_f = numeric_limits<int>::max();

    for (const DeferredOperator &dop : ops) {
        if (dop.pred_f == layer_f) {
            process_operator(node, s, dop.op_id,
                             numeric_limits<int>::max(), no_edges,
                             unused_next_f, dop.pred_h);
        } else {
            next_f = min(next_f, dop.pred_f);
            remaining.push_back(dop);
        }
    }
    assert(no_edges.empty());

    int edge_min_f = numeric_limits<int>::max();
    auto edge_it = surplus_cache.find(node.get_state_id().hash());
    if (edge_it != surplus_cache.end()) {
        for (const DeferredEdge &edge : edge_it->second) {
            edge_min_f = min(edge_min_f, edge.f);
        }
    }

    if (remaining.empty() && edge_min_f == numeric_limits<int>::max()) {

        if (do_debug_cache) {
            cout << "[pea cache] exhausted: state " << node.get_state_id()
                 << " has no parked operators left. It stays CLOSED."
                 << endl;
        }
    } else {
        if (!remaining.empty()) {

            osf_cache.emplace(node.get_state_id().hash(),
                              OSFCacheEntry{node.get_state_id(),
                                            std::move(remaining)});
        }
        reinsert_partially_expanded(node, min(next_f, edge_min_f));
    }
}

void PEASearch::verify_remaining_osf_predictions() {

    if (osf_cache.empty()) {
        return;
    }

    vector<OSFCacheEntry> drained;
    drained.reserve(osf_cache.size());
    for (auto &entry : osf_cache) {
        drained.push_back(std::move(entry.second));
    }
    osf_cache.clear();

    long checked = 0;
    for (const OSFCacheEntry &entry : drained) {
        GlobalState parent =
            g_state_registry->lookup_state(entry.parent_id);
        for (const DeferredOperator &dop : entry.ops) {
            const Operator &op = g_operators[dop.op_id];
            GlobalState succ_state =
                g_state_registry->get_successor_state(parent, op);
            search_progress.inc_generated();
            int actual = direct_osf->table_value(succ_state);
            if (actual != dop.pred_h) {
                report_osf_mismatch(op, dop.pred_h, actual);
            }
            ++checked;
        }
    }

    g_inc_g_by = 0;
    cout << "OSF termination sweep verified " << checked
         << " still parked prediction(s)." << endl;
}

void PEASearch::report_osf_mismatch(const Operator &op, int predicted_h,
                                    int actual_h) const {

    cout << "OSF VERIFICATION FAILURE: operator '" << op.get_name()
         << "' predicted successor h = ";
    if (predicted_h == numeric_limits<int>::max()) {
        cout << "dead end";
    } else {
        cout << predicted_h;
    }
    cout << " but generation and evaluation produced h = ";
    if (actual_h == numeric_limits<int>::max()) {
        cout << "dead end";
    } else {
        cout << actual_h;
    }
    cout << "." << endl;
    utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
}

SearchStatus PEASearch::step() {
    pair<SearchNode, bool> n = fetch_next_node();
    if (!n.second) {

        if (verify_osf) {
            verify_remaining_osf_predictions();
        }
        return FAILED;
    }
    SearchNode node = n.first;

    if (g_factoring &&
        g_factoring->get_search_type() == SDA &&
        node.get_g() + node.get_h() >= bound) {
        cout << "f value of to be expanded node (" << node.get_g() + node.get_h()
             << ") is greater/equal to cost of best found plan => goal reached [t="
             << utils::g_timer << "]" << endl;

        if (verify_osf) {
            verify_remaining_osf_predictions();
        }
        return SOLVED;
    }

    GlobalState s = node.get_state();
    if (check_goal_and_set_plan(s)) {

        if (verify_osf) {
            verify_remaining_osf_predictions();
        }

        return SOLVED;
    }

    if (osf_cache.find(node.get_state_id().hash()) != osf_cache.end()) {
        ++num_alpha_reexpansions;
        expand_from_osf_cache(node);
        return IN_PROGRESS;
    }
    if (surplus_cache.find(node.get_state_id().hash()) != surplus_cache.end()) {
        ++num_alpha_reexpansions;
        expand_from_cache(node);
        return IN_PROGRESS;
    }

    ++num_distinct_expansions;

    int threshold_f;
    if (do_partial_expansion) {
        threshold_f = node.get_g() + node.get_h();
    } else {
        threshold_f = numeric_limits<int>::max();
    }

    vector<DeferredEdge> deferred;
    int next_f = numeric_limits<int>::max();

    vector<DeferredOperator> deferred_ops;
    int next_op_f = numeric_limits<int>::max();

    vector<OperatorID> applicable_ops;
    g_successor_generator->generate_applicable_ops(s, applicable_ops);

    for (OperatorID op_id : applicable_ops) {
        if (osf_mode == OSF_DIRECT_PDB) {

            const Operator &op = g_operators[op_id];
            int goal_cost = max(0,
                                CompliantPathGraph::get_min_leaf_goal_cost()
                                - node.get_leaf_part_g());
            if (node.get_g() + get_adjusted_cost(op) + goal_cost >= bound) {
                continue;
            }

            int predicted_h = direct_osf->predict_successor_h(s, op);
            if (predicted_h == numeric_limits<int>::max() && !verify_osf) {

                ++num_osf_deadend_skips;
                if (do_debug_cache) {
                    cout << "[pea cache] osf skip: parent "
                         << node.get_state_id() << " never generates the "
                         << "predicted dead end successor of op '"
                         << op.get_name() << "'" << endl;
                }
                continue;
            }
            if (predicted_h != numeric_limits<int>::max()) {
                int predicted_f = node.get_g() + get_adjusted_cost(op)
                                  + predicted_h;
                if (predicted_f > threshold_f) {

                    deferred_ops.emplace_back(op_id, predicted_f,
                                              predicted_h);
                    ++num_osf_deferred_operators;
                    next_op_f = min(next_op_f, predicted_f);
                    if (do_debug_cache) {
                        cout << "[pea cache] osf defer: parent "
                             << node.get_state_id() << " parks op '"
                             << op.get_name() << "' ungenerated (pred_f="
                             << predicted_f << " > F=" << threshold_f
                             << ")" << endl;
                    }
                    continue;
                }
            }

            process_operator(node, s, op_id, threshold_f, deferred,
                             next_f, predicted_h);
        } else {
            process_operator(node, s, op_id, threshold_f, deferred,
                             next_f, NO_PREDICTION);
        }
    }

    bool any_parked = false;
    int reinsert_f = numeric_limits<int>::max();
    if (!deferred.empty()) {
        surplus_cache[node.get_state_id().hash()] = std::move(deferred);
        any_parked = true;
        reinsert_f = min(reinsert_f, next_f);
    }
    if (!deferred_ops.empty()) {

        assert(osf_cache.find(node.get_state_id().hash()) == osf_cache.end());
        osf_cache.emplace(node.get_state_id().hash(),
                          OSFCacheEntry{node.get_state_id(),
                                        std::move(deferred_ops)});
        any_parked = true;
        reinsert_f = min(reinsert_f, next_op_f);
    }
    if (any_parked) {
        reinsert_partially_expanded(node, reinsert_f);
    }

    return IN_PROGRESS;
}

bool PEASearch::insert_state(const SearchNode &succ_node) {

    if (!g_factoring || !found_solution()) {
        return true;
    }
    if (succ_node.get_g() + succ_node.get_h() >= bound) {
        return false;
    }
    return true;
}

pair<SearchNode, bool> PEASearch::fetch_next_node() {

    bool ds_optimality_guaranteed = false;
    while (true) {
        if (open_list->empty() || ds_optimality_guaranteed) {
            if (!found_solution()) {
                assert(!ds_optimality_guaranteed);
                cout << "Completely explored state space. No solution! [t="
                     << utils::g_timer << "]" << endl;
            } else {

                cout << "no more states in openlist [t=" << utils::g_timer
                     << "]" << endl;
            }

            SearchNode dummy_node = search_space.get_node(g_initial_state());
            return make_pair(dummy_node, false);
        }

        StateID id = open_list->remove_min(0);

        --open_list_size;

        GlobalState s = g_state_registry->lookup_state(id);
        SearchNode node = search_space.get_node(s);

        if (node.is_closed()) {
            continue;
        }

        if (g_factoring && found_solution()) {
            int goal_cost = max(0, CompliantPathGraph::get_min_leaf_goal_cost()
                                   - node.get_leaf_part_g());
            if (node.get_g() + max(goal_cost, node.get_h()) >= bound) {
                if (node.get_h() >= goal_cost) {

                    ds_optimality_guaranteed = true;
                }
                drop_partial_expansion(id, "bound pruned, I10");
                continue;
            }
        }

        node.close();
        assert(!node.is_dead_end());
        update_jump_statistic(node);
        search_progress.inc_expanded();
        return make_pair(node, true);
    }
}

void PEASearch::update_jump_statistic(const SearchNode &node) {

    if (f_evaluator) {
        heuristics[0]->set_evaluator_value(node.get_h());
        f_evaluator->evaluate(node.get_g(), false);
        int new_f_value = f_evaluator->get_value();
        search_progress.report_f_value(new_f_value);
    }
}

static void _add_pea_family_options(OptionParser &parser,
                                    const string &osf_default) {
    parser.add_option<shared_ptr<Evaluator>>("eval", "evaluator for h value");
    parser.add_option<bool>("partial_expansion",
            "enable partial expansion",
            "true");
    parser.add_option<bool>("check_consistency",
            "check for every transition if the heuristic is consistent",
            "false");
    parser.add_option<bool>("debug_cache",
            "narrate every cache event on stdout for small tasks",
            "false");

    vector<string> osf_modes;
    osf_modes.push_back("none");
    osf_modes.push_back("full_checking");
    osf_modes.push_back("direct_pdb");
    parser.add_enum_option<OSFMode>("osf", osf_modes,
            "operator selection function. none = plain PEA*. "
            "full_checking = the generate and check reference mode. "
            "direct_pdb = EPEA* classifying from the PDB table without "
            "generating. Requires eval to be a single pdb(...) heuristic. "
            "Under decoupling also requires SDA, plain dominance/duplicate "
            "pruning with include_price_in_g=false, and a center only pattern",
            osf_default);
    parser.add_option<bool>("verify_osf",
            "crosscheck every direct OSF prediction against the generated "
            "truth and abort (exit 32) on any mismatch. Requires "
            "osf=direct_pdb",
            "false");
}

static shared_ptr<SearchEngine> _parse_pea_astar(OptionParser &parser) {

    parser.document_synopsis(
        "Partial Expansion A* search (eager)",
        "A* variant (Yoshizumi, Miura & Ishida, AAAI 2000). Keeps "
        "successors whose f value exceeds the expansion threshold out of "
        "the open list and reinserts the node under the smallest withheld "
        "f value instead. Ties are broken by the evaluator. Closed nodes "
        "are reopened. Runs classical search and decoupled search "
        "(optimal SDA search type only). With partial_expansion=false the "
        "engine degenerates to plain A* (validation mode).");
    _add_pea_family_options(parser, "none");
    SearchEngine::add_options_to_parser(parser);
    Options opts = parser.parse();

    if (!parser.dry_run()) {

        vector<shared_ptr<Evaluator>> sum_evals;
        sum_evals.push_back(make_shared<GEvaluator>());
        shared_ptr<Evaluator> eval = opts.get<shared_ptr<Evaluator>>("eval");
        sum_evals.push_back(eval);
        shared_ptr<Evaluator> f_eval = make_shared<SumEvaluator>(sum_evals);

        vector<shared_ptr<Evaluator>> evals;
        evals.push_back(f_eval);
        evals.push_back(eval);
        shared_ptr<OpenList<StateID>> open =
            make_shared<TieBreakingOpenList<StateID>>(evals, false, false);

        opts.set("open", open);
        opts.set("f_eval", f_eval);
        return make_shared<PEASearch>(opts);
    }

    return nullptr;
}

static Plugin<SearchEngine> _plugin_pea_astar("pea_astar", _parse_pea_astar);

static shared_ptr<SearchEngine> _parse_epea_astar(OptionParser &parser) {

    parser.document_synopsis(
        "Enhanced Partial Expansion A* search (eager)",
        "EPEA* (Felner et al., AAAI 2012, Goldenberg et al., JAIR 2014). "
        "PEA* with an operator selection function that classifies "
        "operators BEFORE generating successors. With the default "
        "osf=direct_pdb, eval must be a single pdb(...) heuristic. "
        "Decoupled search is supported in the simple configuration (SDA, "
        "dominance/duplicate pruning with include_price_in_g=false, "
        "center only pattern). Same engine class as pea_astar.");
    _add_pea_family_options(parser, "direct_pdb");
    SearchEngine::add_options_to_parser(parser);
    Options opts = parser.parse();

    if (!parser.dry_run()) {

        vector<shared_ptr<Evaluator>> sum_evals;
        sum_evals.push_back(make_shared<GEvaluator>());
        shared_ptr<Evaluator> eval = opts.get<shared_ptr<Evaluator>>("eval");
        sum_evals.push_back(eval);
        shared_ptr<Evaluator> f_eval = make_shared<SumEvaluator>(sum_evals);

        vector<shared_ptr<Evaluator>> evals;
        evals.push_back(f_eval);
        evals.push_back(eval);
        shared_ptr<OpenList<StateID>> open =
            make_shared<TieBreakingOpenList<StateID>>(evals, false, false);

        opts.set("open", open);
        opts.set("f_eval", f_eval);
        return make_shared<PEASearch>(opts);
    }

    return nullptr;
}

static Plugin<SearchEngine> _plugin_epea_astar("epea_astar", _parse_epea_astar);
