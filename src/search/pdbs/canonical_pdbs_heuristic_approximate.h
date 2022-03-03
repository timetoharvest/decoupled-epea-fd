#ifndef PDBS_CANONICAL_PDBS_HEURISTIC_APPROXIMATE_H
#define PDBS_CANONICAL_PDBS_HEURISTIC_APPROXIMATE_H

#include "canonical_pdbs_heuristic.h"

namespace pdbs {
class CanonicalPDBsHeuristicApproximate : public CanonicalPDBsHeuristic {
    virtual std::unique_ptr<CanonicalPDBs> get_canonical_pdbs_from_options(
        const std::shared_ptr<AbstractTask> &task, const options::Options &opts) override;
public:
    explicit CanonicalPDBsHeuristicApproximate(const options::Options &opts);
    virtual ~CanonicalPDBsHeuristicApproximate() = default;
};
}

#endif
