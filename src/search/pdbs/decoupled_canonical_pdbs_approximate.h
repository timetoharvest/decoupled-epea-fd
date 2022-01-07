#ifndef PDBS_DECOUPLED_CANONICAL_PDBS_APPROXIMATE_H
#define PDBS_DECOUPLED_CANONICAL_PDBS_APPROXIMATE_H

#include "canonical_pdbs.h"

class GlobalState;

namespace pdbs {
class DecoupledCanonicalPDBsApproximate : public CanonicalPDBs {
public:
    DecoupledCanonicalPDBsApproximate(
        const std::shared_ptr<PDBCollection> &pdbs,
        const std::shared_ptr<std::vector<PatternClique>> &pattern_cliques);
    ~DecoupledCanonicalPDBsApproximate() = default;

    virtual int get_value(const GlobalState &state) const override;
};
}

#endif
