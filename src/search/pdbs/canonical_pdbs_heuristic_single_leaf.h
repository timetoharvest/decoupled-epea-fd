#ifndef PDBS_CANONICAL_PDBS_HEURISTIC_SINGLE_LEAF_H
#define PDBS_CANONICAL_PDBS_HEURISTIC_SINGLE_LEAF_H

#include "canonical_pdbs_heuristic.h"

namespace pdbs {
class CanonicalPDBsHeuristicSingleLeaf : public CanonicalPDBsHeuristic {
    virtual std::unique_ptr<CanonicalPDBs> get_canonical_pdbs_from_options(
        const std::shared_ptr<AbstractTask> &task,
        const options::Options &opts) override;
public:
    explicit CanonicalPDBsHeuristicSingleLeaf(
        const options::Options &opts);
    virtual ~CanonicalPDBsHeuristicSingleLeaf() = default;
};
}

#endif
