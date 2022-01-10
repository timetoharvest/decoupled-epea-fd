#ifndef PDBS_MAX_SCP_HEURISTIC_APPROXIMATE_H
#define PDBS_MAX_SCP_HEURISTIC_APPROXIMATE_H

#include "max_scp_heuristic.h"

#include "../leaf_state_id.h"

#include "../cost_saturation/cost_partitioning_heuristic.h"

class ExplicitStateCPG;

namespace pdbs {
class MaxSCPHeuristicApproximate : public MaxSCPHeuristic {
    std::vector<std::vector<LeafFactorID>> affected_leaves_by_abstraction;

    int compute_pdb_value_with_prices(
        const GlobalState &state,
        const std::vector<int> &abstract_state_ids,
        const cost_saturation::CostPartitioningHeuristic::LookupTable &lookup_table,
        const ExplicitStateCPG* prices) const;
protected:
    virtual int compute_heuristic(const GlobalState &ancestor_state) override;
public:
    MaxSCPHeuristicApproximate(
        const options::Options &opts,
        cost_saturation::Abstractions &&abstractions,
        cost_saturation::CPHeuristics &&cp_heuristics);
    virtual ~MaxSCPHeuristicApproximate() override = default;
};
}
#endif
