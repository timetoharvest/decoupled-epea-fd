#ifndef PEA_SEARCH_H
#define PEA_SEARCH_H

#include "evaluator.h"
#include "state.h"
#include "search_engine.h"
#include "search_progress.h"
#include "search_space.h"

#include "open_lists/open_list.h"

#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

class DirectPDBOSF;
class Heuristic;
class Operator;
class Evaluator;

namespace options {
class Options;
}

enum OSFMode {
    OSF_NONE,
    OSF_FULL_CHECKING,
    OSF_DIRECT_PDB
};

class PEASearch : public SearchEngine {

    bool do_partial_expansion;

    bool check_consistency;

    bool do_debug_cache;

    std::shared_ptr<OpenList<StateID>> open_list;

    std::shared_ptr<Evaluator> f_evaluator;

    struct DeferredEdge {
        OperatorID op_id;
        StateID succ_id;
        int f;
        int edge_g;

        DeferredEdge(OperatorID op_id, StateID succ_id, int f, int edge_g)
            : op_id(op_id), succ_id(succ_id), f(f), edge_g(edge_g) {
        }
    };

    std::unordered_map<size_t, std::vector<DeferredEdge>> surplus_cache;

    OSFMode osf_mode;

    bool verify_osf;

    std::unique_ptr<DirectPDBOSF> direct_osf;

    struct DeferredOperator {
        OperatorID op_id;
        int pred_f;

        int pred_h;

        DeferredOperator(OperatorID op_id, int pred_f, int pred_h)
            : op_id(op_id), pred_f(pred_f), pred_h(pred_h) {
        }
    };

    struct OSFCacheEntry {
        StateID parent_id;
        std::vector<DeferredOperator> ops;
    };

    std::unordered_map<size_t, OSFCacheEntry> osf_cache;

    static const int NO_PREDICTION = std::numeric_limits<int>::min();

    long num_distinct_expansions;
    long num_alpha_reexpansions;
    long num_beta_deferred_edges;
    int open_list_size;
    int max_open_list_size;

    long num_osf_deferred_operators;
    long num_osf_deadend_skips;

    bool insert_state(const SearchNode &succ_node);

    void insert_into_open_list(StateID id);

    void flush_partial_expansion(StateID id, const char *reason);

    void drop_partial_expansion(StateID id, const char *reason);

    void reinsert_partially_expanded(SearchNode &node, int next_f);

    void expand_from_cache(SearchNode &node);

    void release_deferred_edge(const DeferredEdge &edge);

    void process_operator(SearchNode &node, const GlobalState &s,
                          OperatorID op_id, int threshold_f,
                          std::vector<DeferredEdge> &deferred, int &next_f,
                          int predicted_h);

    void expand_from_osf_cache(SearchNode &node);

    void verify_remaining_osf_predictions();

    void report_osf_mismatch(const Operator &op, int predicted_h,
                             int actual_h) const;

protected:
    SearchStatus step() override;
    std::pair<SearchNode, bool> fetch_next_node();
    void update_jump_statistic(const SearchNode &node);

    std::vector<Heuristic*> heuristics;
    std::vector<Heuristic*> estimate_heuristics;

    void initialize() override;

public:
    PEASearch(const options::Options &opts);

    ~PEASearch() override;

    void statistics() const override;
};

#endif
