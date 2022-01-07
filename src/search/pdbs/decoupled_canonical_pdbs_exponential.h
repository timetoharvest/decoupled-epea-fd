#ifndef PDBS_DECOUPLED_CANONICAL_PDBS_EXPONENTIAL_H
#define PDBS_DECOUPLED_CANONICAL_PDBS_EXPONENTIAL_H

#include "canonical_pdbs.h"

#include "../leaf_state_id.h"

class ExplicitStateCPG;
class GlobalState;
class State;

namespace pdbs {
class DecoupledCanonicalPDBsExponential : public CanonicalPDBs {
    std::vector<LeafFactorID> affected_leaves;

    void compute_min_distance_decoupled_exponential(const ExplicitStateCPG *prices,
            std::vector<int> &member_state,
            int i,
            int sum_prices,
            int &min_d) const;
public:
    DecoupledCanonicalPDBsExponential(
        const std::shared_ptr<PDBCollection> &pdbs,
        const std::shared_ptr<std::vector<PatternClique>> &pattern_cliques);
    ~DecoupledCanonicalPDBsExponential() = default;

    virtual int get_value(const GlobalState &state) const override;
};
}

#endif
