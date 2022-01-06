#ifndef PDBS_DECOUPLED_PDBS_H
#define PDBS_DECOUPLED_PDBS_H

#include "types.h"

#include "../leaf_state_id.h"

#include <memory>

class ExplicitStateCPG;
class GlobalState;

namespace pdbs {
class DecoupledPDBs {
    std::shared_ptr<PDBCollection> pdbs;
    std::shared_ptr<std::vector<PatternClique>> pattern_cliques;

    std::vector<LeafFactorID> affected_leaves;

    void compute_min_distance_decoupled(const ExplicitStateCPG *cpg,
            std::vector<int> &member_state,
            int i,
            int sum_prices,
            int &min_d) const;
public:
    DecoupledPDBs(
        const std::shared_ptr<PDBCollection> &pdbs,
        const std::shared_ptr<std::vector<PatternClique>> &pattern_cliques);
    ~DecoupledPDBs() = default;

    int get_value(const GlobalState &state) const;
};
}

#endif
