#include "canonical_pdbs.h"

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
CanonicalPDBs::CanonicalPDBs(
    const shared_ptr<PDBCollection> &pdbs,
    const shared_ptr<vector<PatternClique>> &pattern_cliques)
    : pdbs(pdbs), pattern_cliques(pattern_cliques) {
    assert(pdbs);
    assert(pattern_cliques);

    if (g_factoring){
        // sanity check: no patterns in a clique may affect the same leaf
        // otherwise taking the sum of the values may not be admissible
        for (const PatternClique &clique : *pattern_cliques) {
            set<LeafFactorID> aff_leaves_clique;
            for (PatternID pdb_index : clique) {
                set<LeafFactorID> aff_leaves_pattern;
                for (int var : (*pdbs)[pdb_index]->get_pattern()){
                    LeafFactorID factor = g_belongs_to_factor[var];
                    if (factor != LeafFactorID::CENTER){
                        if (find(aff_leaves_clique.begin(),
                                aff_leaves_clique.end(),
                                factor) != aff_leaves_clique.end() &&
                                find(aff_leaves_pattern.begin(),
                                        aff_leaves_pattern.end(),
                                        factor) == aff_leaves_pattern.end()){
                            cerr << "Multiple patterns of a clique affect the same leaf, so are not orthogonal." << endl;
                            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
                        }
                        aff_leaves_clique.insert(factor);
                        aff_leaves_pattern.insert(factor);
                    }
                }
            }
        }
    }
}

int CanonicalPDBs::get_value(const State &state) const {
    // If we have an empty collection, then pattern_cliques = { \emptyset }.
    assert(!pattern_cliques->empty());
    assert(!g_factoring);
    int max_h = 0;
    vector<int> h_values;
    h_values.reserve(pdbs->size());
    state.unpack();
    for (const shared_ptr<PatternDatabase> &pdb : *pdbs) {
        int h = pdb->get_value(state.get_unpacked_values());
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

int CanonicalPDBs::get_value(const GlobalState &state) const {
    // If we have an empty collection, then pattern_cliques = { \emptyset }.
    assert(!pattern_cliques->empty());
    int max_h = 0;
    vector<int> h_values;
    h_values.reserve(pdbs->size());
    const ExplicitStateCPG *prices = 0;
    if (g_factoring){
        prices = static_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    }
    for (const shared_ptr<PatternDatabase> &pdb : *pdbs) {
        int h;
        if (g_factoring){
            h = pdb->get_value(state, prices);
        } else {
            h = pdb->get_value(state);
        }
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
