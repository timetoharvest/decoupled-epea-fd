#include "decoupled_canonical_pdbs_approximate.h"

#include "pattern_database.h"
#include "pattern_filter.h"
#include "utils.h"

#include "../globals.h"
#include "../state.h"

#include "../compliant_paths/cpg_storage.h"
#include "../compliant_paths/explicit_state_cpg.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>

using namespace std;

namespace pdbs {
DecoupledCanonicalPDBsApproximate::DecoupledCanonicalPDBsApproximate(
    const shared_ptr<PDBCollection> &pdbs,
    const shared_ptr<vector<PatternClique>> &pattern_cliques)
    : CanonicalPDBs(pdbs, pattern_cliques) {
    // TODO should we verify that no two patterns of the same
    // clique affect the same leaf? Maybe in a #ifndef NDEBUG block?
    // This is ensured by the construction of patterns in
    // CanonicalPDBsHeuristic, though.
    affected_leaves_by_pattern.reserve(pdbs->size());
    for (const shared_ptr<PatternDatabase> &pdb : *pdbs) {
        unordered_set<LeafFactorID> aff_leaves = get_leaf_factors_of_pattern(pdb->get_pattern());
        affected_leaves_by_pattern.emplace_back(aff_leaves.begin(), aff_leaves.end());
    }
}

void compute_min_distance(
    const PatternDatabase &pdb,
    const ExplicitStateCPG *cpg,
    const vector<LeafFactorID> &affected_leaves,
    vector<int> &member_state,
    int i,
    int sum_prices,
    int &min_d) {

    if (sum_prices >= min_d){
        // we cannot possibly improve the heuristic
        return;
    }

    LeafFactorID leaf = affected_leaves[i];

    int number_states = cpg->get_number_states(leaf);
    LeafStateHash id(0);
    while (number_states > 0){
        if (cpg->has_leaf_state(id, leaf)){
            --number_states;

            set_leaf_facts(id, leaf, member_state);

            int price = cpg->get_cost_of_state(id, leaf);
            if (i < static_cast<int>(affected_leaves.size()) - 1){
                // recursion
                compute_min_distance(pdb, cpg, affected_leaves, member_state, i + 1, sum_prices + price, min_d);
            } else {
                // end recursion
                int h = pdb.get_value(member_state);
                if (h != numeric_limits<int>::max()) {
                    min_d = min(min_d, sum_prices + price + h);
                }
            }
        }
        ++id;
    }
}

int compute_pdb_value_with_prices(
    const GlobalState &state,
    const PatternDatabase &pdb,
    const ExplicitStateCPG* prices,
    const vector<LeafFactorID> &affected_leaves) {
    int min_h = numeric_limits<int>::max();
    if (affected_leaves.empty()){
        min_h = pdb.get_value(state);
    } else {
        // used to call get_value(vector<int>) for all member states
        vector<int> member_state(g_variable_domain.size(), -1);
        set_center_facts(state, member_state);
        compute_min_distance(pdb, prices, affected_leaves, member_state, 0, 0, min_h);
    }
    return min_h;
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
        int h = compute_pdb_value_with_prices(state, *pdb, prices, affected_leaves_by_pattern[index]);
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
