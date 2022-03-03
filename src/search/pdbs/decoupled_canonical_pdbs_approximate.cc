#include "decoupled_canonical_pdbs_approximate.h"

#include "decoupled_pdb_utils.h"
#include "pattern_database.h"
#include "pattern_filter.h"

#include "../globals.h"
#include "../option_parser.h"
#include "../state.h"

#include "../compliant_paths/cpg_storage.h"
#include "../compliant_paths/explicit_state_cpg.h"

#include <cassert>
#include <iostream>
#include <limits>

using namespace std;

namespace pdbs {
DecoupledCanonicalPDBsApproximate::DecoupledCanonicalPDBsApproximate(
    const shared_ptr<PDBCollection> &pdbs,
    const shared_ptr<vector<PatternClique>> &pattern_cliques,
    bool polynomial_decoupled_pdb)
    : CanonicalPDBs(pdbs, pattern_cliques),
      polynomial_decoupled_pdb(polynomial_decoupled_pdb) {
    // TODO should we verify that no two patterns of the same
    // clique affect the same leaf? Maybe in a #ifndef NDEBUG block?
    // This is ensured by the construction of patterns in
    // CanonicalPDBsHeuristic, though.
    affected_leaves_by_pattern.reserve(pdbs->size());
    affected_pattern_var_indices_by_leaf_by_pattern.reserve(pdbs->size());
    for (const shared_ptr<PatternDatabase> &pdb : *pdbs) {
        unordered_set<LeafFactorID> aff_leaves = get_leaf_factors_of_pattern(pdb->get_pattern());
        affected_leaves_by_pattern.emplace_back(aff_leaves.begin(), aff_leaves.end());
        affected_pattern_var_indices_by_leaf_by_pattern.push_back(compute_affected_pattern_var_indices_by_leaf(pdb->get_pattern()));
    }
}

int DecoupledCanonicalPDBsApproximate::get_value(const GlobalState &state) const {
    // If we have an empty collection, then pattern_cliques = { \emptyset }.
    assert(!pattern_cliques->empty());
    int max_h = 0;
    vector<int> h_values;
    h_values.reserve(pdbs->size());
    const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    for (size_t index = 0; index < pdbs->size(); ++index) {
        const shared_ptr<PatternDatabase> &pdb = (*pdbs)[index];
        int h = compute_pdb_value_with_prices(
            state,
            *pdb,
            prices,
            affected_leaves_by_pattern[index],
            affected_pattern_var_indices_by_leaf_by_pattern[index],
            polynomial_decoupled_pdb);
        if (h == numeric_limits<int>::max()) {
            return numeric_limits<int>::max();
        }
        h_values.push_back(h);
    }
    for (const PatternClique &clique : *pattern_cliques) {
        int clique_h = 0;
        for (PatternID pdb_index : clique) {
            clique_h += h_values[pdb_index];
        }
        max_h = max(max_h, clique_h);
    }
    return max_h;
}
}
