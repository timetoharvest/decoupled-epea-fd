#include "decoupled_pdbs.h"

#include "pattern_database.h"

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
DecoupledPDBs::DecoupledPDBs(
    const shared_ptr<PDBCollection> &pdbs,
    const shared_ptr<vector<PatternClique>> &pattern_cliques)
    : pdbs(pdbs), pattern_cliques(pattern_cliques) {
    assert(pdbs);
    assert(pattern_cliques);
    assert(g_factoring);

    for (const shared_ptr<PatternDatabase> &pdb : *pdbs) {
        for (int var : pdb->get_pattern()){
            LeafFactorID factor = g_belongs_to_factor[var];
            if (factor != LeafFactorID::CENTER &&
                    find(affected_leaves.begin(),
                            affected_leaves.end(),
                            factor) == affected_leaves.end()){
                affected_leaves.push_back(factor);
            }
        }
    }
}

void DecoupledPDBs::compute_min_distance_decoupled(const ExplicitStateCPG *cpg,
        vector<int> &member_state,
        int i,
        int sum_prices,
        int &min_d) const {

    if (sum_prices >= min_d){
        // we cannot possibly improve the heuristic
        return;
    }

    int number_states = cpg->get_number_states(affected_leaves[i]);
    LeafStateHash id(0);
    while (number_states > 0){
        if (cpg->has_leaf_state(id, affected_leaves[i])){
            --number_states;

            LeafState lstate = g_state_registry->lookup_leaf_state(id, affected_leaves[i]);
            for (int var : g_leaves[affected_leaves[i]]){
                member_state[var] = lstate[var];
            }

            int price = cpg->get_cost_of_state(id, affected_leaves[i]);
            if (i < static_cast<int>(affected_leaves.size()) - 1){
                // recursion
                compute_min_distance_decoupled(cpg, member_state, i + 1, sum_prices + price, min_d);
            } else {
                // end recursion
                int max_h = 0;
                vector<int> h_values;
                h_values.reserve(pdbs->size());
                for (const shared_ptr<PatternDatabase> &pdb : *pdbs) {
                    int h = pdb->get_value(member_state);
                    if (h == numeric_limits<int>::max()) {
                        // dead end member state
                        return;
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
                min_d = min(min_d, sum_prices + price + max_h);
            }
        }
        ++id;
    }
}

int DecoupledPDBs::get_value(const GlobalState &state) const {
    // If we have an empty collection, then pattern_cliques = { \emptyset }.
    assert(!pattern_cliques->empty());

    if (affected_leaves.empty()){
        // compute maximal heuristic value for the center state
        int max_h = 0;
        vector<int> h_values;
        h_values.reserve(pdbs->size());
        for (const shared_ptr<PatternDatabase> &pdb : *pdbs) {
            int h = pdb->get_value(state);
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
    } else {
        // compute minimum heuristic value across all member states
        vector<int> member_state(g_variable_domain.size(), -1);
        for (int var : g_center){
            member_state[var] = state[var];
        }
        int min_h = numeric_limits<int>::max();
        const ExplicitStateCPG *prices = static_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
        compute_min_distance_decoupled(prices, member_state, 0, 0, min_h);
        return min_h;
    }
}
}
