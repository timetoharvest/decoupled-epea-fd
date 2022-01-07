#ifndef PDBS_MAX_SCP_HEURISTIC_H
#define PDBS_MAX_SCP_HEURISTIC_H

#include "../heuristic.h"

#include "../cost_saturation/unsolvability_heuristic.h"

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
    std::vector<cost_saturation::CostPartitioningHeuristic> cp_heuristics;
    cost_saturation::UnsolvabilityHeuristic unsolvability_heuristic;

    // For statistics.
    mutable std::vector<int> num_best_order;

    void print_statistics() const;

protected:
    virtual int compute_heuristic(const GlobalState &ancestor_state) override;

public:
    MaxSCPHeuristic(
        const options::Options &opts,
        cost_saturation::Abstractions &&abstractions,
        std::vector<cost_saturation::CostPartitioningHeuristic> &&cp_heuristics);
    virtual ~MaxSCPHeuristic() override;
};
}
#endif
