#include "decoupled_canonical_pdbs_single_leaf.h"

#include "pattern_database.h"
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
DecoupledCanonicalPDBsSingleLeaf::DecoupledCanonicalPDBsSingleLeaf(
    const shared_ptr<PDBCollection> &pdbs,
    const shared_ptr<vector<PatternClique>> &pattern_cliques)
    : CanonicalPDBs(pdbs, pattern_cliques) {

    affecting_patterns.resize(pattern_cliques->size());
    for (int c_id = 0; c_id < static_cast<int>(pattern_cliques->size()); ++c_id) {
        for (PatternID p_id : (*pattern_cliques)[c_id]) {
            for (int var : (*pdbs)[p_id]->get_pattern()){
                LeafFactorID factor = g_belongs_to_factor[var];
                if (factor != LeafFactorID::CENTER){
                    if (affecting_patterns[c_id].size() <= factor){
                        affecting_patterns[c_id].resize(factor + 1);
                    }
                    affecting_patterns[c_id][factor].push_back(p_id);
                }
            }
        }
    }
}

int DecoupledCanonicalPDBsSingleLeaf::get_value(const GlobalState &state) const {
    // If we have an empty collection, then pattern_cliques = { \emptyset }.
    assert(!pattern_cliques->empty());
    const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    vector<int> member_state(g_variable_domain.size(), -1);
    set_center_facts(state, member_state);
    // TODO we never account for patterns that only affect the center unless they are
    // contained in a clique in which all patterns only affect the center; this has to be fixed
    int max_h = 0;
    for (int clique_id = 0; clique_id < static_cast<int>(pattern_cliques->size()); ++clique_id) {
        int clique_h = 0;
        if (affecting_patterns[clique_id].empty()){
            // clique affects only the center
            for (PatternID pdb_index : (*pattern_cliques)[clique_id]) {
                int h = (*pdbs)[pdb_index]->get_value(state);
                if (h == numeric_limits<int>::max()) {
                    return numeric_limits<int>::max();
                }
                clique_h += h;
            }
        } else {
            for (LeafFactorID leaf(0); leaf < g_leaves.size(); ++leaf) {
                if (leaf < affecting_patterns[clique_id].size() && !affecting_patterns[clique_id][leaf].empty()){
                    clique_h += get_min_distance(prices, member_state, leaf, clique_id);
                }
            }
        }
        max_h = max(max_h, clique_h);
    }
    return max_h;
}

int DecoupledCanonicalPDBsSingleLeaf::get_min_distance(
        const ExplicitStateCPG *prices,
        vector<int> &member_state,
        LeafFactorID leaf,
        int clique_id) const {

    int min_d = numeric_limits<int>::max();

    int number_states = prices->get_number_states(leaf);
    LeafStateHash id(0);
    while (number_states > 0){
        if (prices->has_leaf_state(id, leaf)){
            --number_states;

            set_leaf_facts(id, leaf, member_state);

            int price = prices->get_cost_of_state(id, leaf);

            int sum_dists = 0;
            for (PatternID p_id : affecting_patterns[clique_id][leaf]){
                sum_dists += (*pdbs)[p_id]->get_value(member_state);
                if (price + sum_dists >= min_d){
                    break;
                }
            }

            min_d = min(min_d, price + sum_dists);
        }
        ++id;
    }
    return min_d;
}
}
