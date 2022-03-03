#ifndef PDBS_CANONICAL_PDBS_HEURISTIC_H
#define PDBS_CANONICAL_PDBS_HEURISTIC_H

#include "../heuristic.h"

#include "canonical_pdbs.h"

class AbstractTask;

namespace options {
class OptionParser;
class Options;
}

namespace pdbs {
// Implements the canonical heuristic function.
class CanonicalPDBsHeuristic : public Heuristic {
    std::unique_ptr<CanonicalPDBs> canonical_pdbs;

    virtual std::unique_ptr<CanonicalPDBs> get_canonical_pdbs_from_options(
        const std::shared_ptr<AbstractTask> &task, const options::Options &opts);
protected:
    virtual int compute_heuristic(const GlobalState &ancestor_state) override;

public:
    explicit CanonicalPDBsHeuristic(const options::Options &opts);
    virtual ~CanonicalPDBsHeuristic() = default;
};

extern void add_canonical_pdbs_options_to_parser(options::OptionParser &parser);
}

#endif
