#ifndef PDBS_DECOUPLED_CANONICAL_PDBS_EXPONENTIAL_ILP_H
#define PDBS_DECOUPLED_CANONICAL_PDBS_EXPONENTIAL_ILP_H

#include "canonical_pdbs.h"
#include "../leaf_state_id.h"
#include "../utils/hash.h"

#include <limits>


class ExplicitStateCPG;
class GlobalState;
class State;

namespace lp {
    class LPVariable;
    class LPConstraint;
}

namespace pdbs {
class DecoupledCanonicalPDBsExponentialILP : public CanonicalPDBs {
    struct AbstractLeafStateInfo {
        int price;
        std::vector<int> concrete_lstate_lp_var_ids;
        AbstractLeafStateInfo() : price(std::numeric_limits<int>::max()) {
        }
    };

    mutable double lp_infty;
    bool are_leaves_affected; // true if there exists a pattern where is_center_only_pattern is false

    std::vector<utils::HashSet<PatternID>> affecting_patterns;
    std::vector<std::vector<LeafFactorID>> affected_leaves_by_pdb;
    std::vector<bool> is_center_only_pattern;
    std::vector<PatternID> center_only_patterns;

    void enumerate_abstract_states(int pdb_id,
                                   int pdb_lp_var,
                                   std::vector<lp::LPVariable> &lp_variables,
                                   std::vector<lp::LPConstraint> &constraints,
                                   const std::vector<utils::HashMap<std::vector<int>, AbstractLeafStateInfo>> &abstract_leaf_state_prices,
                                   int index,
                                   std::vector<int> &abs_state) const;

public:
    DecoupledCanonicalPDBsExponentialILP(
        const std::shared_ptr<PDBCollection> &pdbs,
        const std::shared_ptr<std::vector<PatternClique>> &pattern_cliques);
    ~DecoupledCanonicalPDBsExponentialILP() = default;

    virtual int get_value(const GlobalState &state) const override;
};
}

#endif
