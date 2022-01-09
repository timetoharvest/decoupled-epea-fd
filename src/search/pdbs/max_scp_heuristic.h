#ifndef PDBS_MAX_SCP_HEURISTIC_H
#define PDBS_MAX_SCP_HEURISTIC_H

#include "../heuristic.h"

#include "types.h"

#include "../leaf_state_id.h"
#include "../cost_saturation/types.h"
#include "../cost_saturation/unsolvability_heuristic.h"

#include <unordered_set>

class ExplicitStateCPG;

namespace options {
class Options;
}

namespace cost_saturation {
class AbstractionFunction;
class CostPartitioningHeuristic;
}

namespace pdbs {
enum class DecoupledPatternCollectionLookUp;

/*
  Compute the maximum over multiple saturated cost partitionings over
  pattern database heuristics. This is a copy of MaxCostPartitioningHeuristic
  from cost_saturation, plus the plugin from saturated_cost_partitioning.
  Thus, this class only works for SCPs. TODO: adapt to PDBs
*/
class MaxSCPHeuristic : public Heuristic {
    const DecoupledPatternCollectionLookUp dec_lookup_type;
    std::vector<std::unique_ptr<cost_saturation::AbstractionFunction>> abstraction_functions;
    cost_saturation::CPHeuristics cp_heuristics;
    cost_saturation::UnsolvabilityHeuristic unsolvability_heuristic;

    std::vector<Pattern> patterns;

    // approx
    std::vector<std::vector<LeafFactorID>> affected_leaves_by_abstraction;

    // exponential
    std::vector<LeafFactorID> affected_leaves;

    // single leaf
    // for every leaf, we store the patterns that affect it
    std::vector<std::unordered_set<PatternID>> affecting_patterns;
    std::unordered_set<PatternID> center_only_patterns;

    // For statistics.
    mutable std::vector<int> num_best_order;

    void compute_min_distance_approx(
        const ExplicitStateCPG *cpg,
        const std::vector<int> &distances,
        const cost_saturation::AbstractionFunction &abstraction_function,
        const std::vector<LeafFactorID> affected_leaves,
        std::vector<int> &member_state,
        int i,
        int sum_prices,
        int &min_d) const;
    int compute_heuristic_approx(
        const GlobalState &state, const std::vector<int> &abstract_state_ids) const;

    void compute_min_distance_decoupled_exponential(const ExplicitStateCPG *prices,
                                                    std::vector<int> &member_state,
                                                    int i,
                                                    int sum_prices,
                                                    int &min_d) const;
    int compute_heuristic_exp(
        const GlobalState &state, const std::vector<int> &abstract_state_ids) const;

    int compute_min_distance_single_leaf(const ExplicitStateCPG *prices,
                                         const cost_saturation::CostPartitioningHeuristic &cp_heuristic,
                                         std::vector<int> &member_state,
                                         LeafFactorID leaf) const;

    int compute_heuristic_single_leaf(
        const GlobalState &state, const std::vector<int> &abstract_state_ids) const;

    void print_statistics() const;

protected:
    virtual int compute_heuristic(const GlobalState &ancestor_state) override;

public:
    MaxSCPHeuristic(
        const options::Options &opts,
        cost_saturation::Abstractions &&abstractions,
        cost_saturation::CPHeuristics &&cp_heuristics);
    virtual ~MaxSCPHeuristic() override;
};
}
#endif
