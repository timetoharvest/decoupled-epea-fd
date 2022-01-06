#ifndef PDBS_CANONICAL_PDBS_H
#define PDBS_CANONICAL_PDBS_H

#include "types.h"

#include "../leaf_state_id.h"

#include <memory>

class ExplicitStateCPG;
class GlobalState;
class State;

namespace pdbs {
class CanonicalPDBs {
    std::shared_ptr<PDBCollection> pdbs;
    std::shared_ptr<std::vector<PatternClique>> pattern_cliques;

    // only needed for decoupled exponential lookup
    std::vector<LeafFactorID> affected_leaves;

    void compute_min_distance_decoupled_exponential(const ExplicitStateCPG *cpg,
            std::vector<int> &member_state,
            int i,
            int sum_prices,
            int &min_d) const;
public:
    CanonicalPDBs(
        const std::shared_ptr<PDBCollection> &pdbs,
        const std::shared_ptr<std::vector<PatternClique>> &pattern_cliques);
    ~CanonicalPDBs() = default;

    int get_value(const State &state) const;
    int get_value(const GlobalState &state) const;
    int get_value_dec_approx(const GlobalState &state) const;
    int get_value_dec_single_leaf(const GlobalState &state) const;
    int get_value_dec_exponential(const GlobalState &state) const;
};
}

#endif
