#ifndef PDBS_DECOUPLED_CANONICAL_PDBS_EXPONENTIAL_H
#define PDBS_DECOUPLED_CANONICAL_PDBS_EXPONENTIAL_H

#include "canonical_pdbs.h"

#include "../leaf_state_id.h"

class ExplicitStateCPG;
class GlobalState;
class State;
namespace options {
    class OptionParser;
}

namespace pdbs {
class DecoupledCanonicalPDBsExponential : public CanonicalPDBs {
    ExplicitLeafSortingType sort_leaves_type;

    mutable std::vector<LeafFactorID> affected_leaves;

    // these store, for each recursion depth the patterns/cliques of which
    // all leaves have been treated, so the final value for the pattern/clique
    // can be computed
    mutable std::vector<std::vector<PatternID>> patterns_by_rec_depth;
    mutable std::vector<std::vector<PatternID>> cliques_by_rec_depth;

    void apply_resorted_affected_leaves() const;

    void compute_min_distance_decoupled_exponential(const ExplicitStateCPG *prices,
            std::vector<int> &member_state,
            std::vector<int> &h_values,
            int i,
            int sum_prices,
            int min_clique_h,
            int &final_h) const;
public:
    DecoupledCanonicalPDBsExponential(
        const std::shared_ptr<PDBCollection> &pdbs,
        const std::shared_ptr<std::vector<PatternClique>> &pattern_cliques,
        ExplicitLeafSortingType sort_leaves_type);
    ~DecoupledCanonicalPDBsExponential() = default;

    virtual int get_value(const GlobalState &state) const override;
};

extern void add_exponential_options_to_parser(options::OptionParser &parser);
}

#endif
