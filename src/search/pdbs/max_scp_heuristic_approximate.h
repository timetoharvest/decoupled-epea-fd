#ifndef PDBS_MAX_SCP_HEURISTIC_APPROXIMATE_H
#define PDBS_MAX_SCP_HEURISTIC_APPROXIMATE_H

#include "max_scp_heuristic.h"

#include "../leaf_state_id.h"

class ExplicitStateCPG;

namespace pdbs {
class MaxSCPHeuristicApproximate : public MaxSCPHeuristic {
    std::vector<std::vector<LeafFactorID>> affected_leaves_by_abstraction;

    void compute_min_distance_approx(
        const ExplicitStateCPG *cpg,
        const std::vector<int> &distances,
        const cost_saturation::AbstractionFunction &abstraction_function,
        const std::vector<LeafFactorID> affected_leaves,
        std::vector<int> &member_state,
        int i,
        int sum_prices,
        int &min_d) const;
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
