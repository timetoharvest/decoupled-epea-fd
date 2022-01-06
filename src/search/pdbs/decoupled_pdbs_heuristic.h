#ifndef PDBS_DECOUPLED_PDBS_HEURISTIC_H
#define PDBS_DECOUPLED_PDBS_HEURISTIC_H

#include "decoupled_pdbs.h"

#include "../heuristic.h"

namespace options {
class OptionParser;
}

namespace pdbs {
// Implements a general canonical PDB heuristic for decoupled search that
// allows patterns of a clique to affect the same leaves.
class DecoupledPDBsHeuristic : public Heuristic {
    DecoupledPDBs decoupled_pdbs;

protected:
    virtual int compute_heuristic(const GlobalState &ancestor_state) override;

public:
    explicit DecoupledPDBsHeuristic(const options::Options &opts);
    virtual ~DecoupledPDBsHeuristic() = default;
};

void add_decoupled_canonical_pdbs_options_to_parser(options::OptionParser &parser);
}

#endif
