#ifndef PDBS_MAX_SCP_HEURISTIC_EXPONENTIAL_H
#define PDBS_MAX_SCP_HEURISTIC_EXPONENTIAL_H

#include "max_scp_heuristic.h"

#include "../leaf_state_id.h"

class ExplicitStateCPG;

namespace utils {
class RandomNumberGenerator;
}

namespace pdbs {
enum class ExplicitLeafSortingType;
enum class IncrementalComputation;
class MaxSCPHeuristicExponential : public MaxSCPHeuristic {
    ExplicitLeafSortingType sort_leaves_type;
    bool early_termination;
    IncrementalComputation incremental_computation;

    // list of leaves affected by any of the pdbs; mutable because can be resorted within get_value()
    mutable std::vector<LeafFactorID> affected_leaves;
    // same, but individually for all patterns
    std::vector<std::vector<LeafFactorID>> affected_leaves_by_pattern;

    std::vector<PatternID> center_only_patterns;

    // these store, for each recursion depth the patterns of which
    // all leaves have been treated, so the final value for the pattern
    // can be computed
    mutable std::vector<std::vector<PatternID>> patterns_by_rec_depth;

    void apply_resorted_affected_leaves() const;

    void compute_min_distance_decoupled_exponential(const ExplicitStateCPG *prices,
                                                    std::vector<int> &member_state,
                                                    int i,
                                                    int sum_prices,
                                                    const std::vector<int> &partial_h_by_cp_heur,
                                                    int &final_h) const;
protected:
    virtual int compute_heuristic(const GlobalState &ancestor_state) override;
public:
    MaxSCPHeuristicExponential(
        const options::Options &opts,
        cost_saturation::Abstractions &&abstractions,
        cost_saturation::CPHeuristics &&cp_heuristics);
    virtual ~MaxSCPHeuristicExponential() override = default;
};
}
#endif
