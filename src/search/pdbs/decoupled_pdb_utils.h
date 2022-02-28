#ifndef PDBS_DECOUPLED_PDB_UTILS_H
#define PDBS_DECOUPLED_PDB_UTILS_H

#include "types.h"

#include "../leaf_state_id.h"

class ExplicitStateCPG;
class GlobalState;

namespace pdbs {
/*
  These two functions set the value of variables from the respective factor
  state in state. state is required to have the same size as g_variable_domain.
 */
extern void set_center_facts(const GlobalState &center_state, std::vector<int> &state);
extern void set_leaf_facts(LeafStateHash id, LeafFactorID factor, std::vector<int> &state);

extern std::vector<int> unrank(const PatternDatabase& pdb, int rank);

extern std::vector<int> compute_affected_pattern_var_indices(
    const Pattern &pattern, LeafFactorID factor);
extern int compute_pdb_value_with_prices(
    const GlobalState &state,
    const PatternDatabase &pdb,
    const ExplicitStateCPG* prices,
    const std::vector<LeafFactorID> &affected_leaves,
    const std::vector<std::vector<int>> &pattern_var_indices_by_affected_leaf_indices,
    bool polynomial);
}

#endif
