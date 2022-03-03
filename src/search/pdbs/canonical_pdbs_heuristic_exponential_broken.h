#ifndef PDBS_CANONICAL_PDBS_HEURISTIC_EXPONENTIAL_BROKEN_H
#define PDBS_CANONICAL_PDBS_HEURISTIC_EXPONENTIAL_BROKEN_H

#include "canonical_pdbs_heuristic.h"

namespace pdbs {
class CanonicalPDBsHeuristicExponentialBroken : public CanonicalPDBsHeuristic {
    virtual std::unique_ptr<CanonicalPDBs> get_canonical_pdbs_from_options(
        const std::shared_ptr<AbstractTask> &task,
        const options::Options &opts) override;
public:
    explicit CanonicalPDBsHeuristicExponentialBroken(const options::Options &opts);
    virtual ~CanonicalPDBsHeuristicExponentialBroken() = default;
};
}

#endif
