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
        // TODO this is only needed for the decoupled exponential lookup
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
        // TODO this is only needed for the decoupled single-leaf lookup
        affecting_patterns.resize(g_leaves.size());
        for (PatternID p_id = 0; p_id < static_cast<int>(pdbs->size()); ++p_id) {
            for (int var : (*pdbs)[p_id]->get_pattern()){
                LeafFactorID factor = g_belongs_to_factor[var];
                if (factor != LeafFactorID::CENTER){
                    affecting_patterns[factor].push_back(p_id);
                }
            }
        }
    }
}

int CanonicalPDBs::get_value(const State &state) const {
    // If we have an empty collection, then pattern_cliques = { \emptyset }.
    assert(!pattern_cliques->empty());
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
    assert(!g_factoring);
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
}

int CanonicalPDBs::get_value_dec_approx(const GlobalState &state) const {
    // If we have an empty collection, then pattern_cliques = { \emptyset }.
    assert(!pattern_cliques->empty());
    int max_h = 0;
    vector<int> h_values;
    h_values.reserve(pdbs->size());
    const ExplicitStateCPG *prices = static_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    for (const shared_ptr<PatternDatabase> &pdb : *pdbs) {
        int h = pdb->get_value(state, prices);
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

int CanonicalPDBs::get_value_dec_single_leaf(const GlobalState &state) const {
    // If we have an empty collection, then pattern_cliques = { \emptyset }.
    assert(!pattern_cliques->empty());
    const ExplicitStateCPG *prices = static_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    vector<int> member_state(g_variable_domain.size(), -1);
    for (int var : g_center){
        // TODO make a method for this
        member_state[var] = state[var];
    }
    int max_h = 0;
    for (const PatternClique &clique : *pattern_cliques) {
        int clique_h = 0;
        for (LeafFactorID leaf(0); leaf < g_leaves.size(); ++leaf) {
            clique_h += get_min_distance_decoupled_single_leaf(prices, member_state, leaf, clique);
        }
        max_h = max(max_h, clique_h);
    }
    return max_h;
}

int CanonicalPDBs::get_min_distance_decoupled_single_leaf(const ExplicitStateCPG *prices,
        vector<int> &member_state,
        LeafFactorID leaf,
        const PatternClique &clique) const {

    int min_d = numeric_limits<int>::max();

    int number_states = prices->get_number_states(leaf);
    LeafStateHash id(0);
    while (number_states > 0){
        if (prices->has_leaf_state(id, leaf)){
            --number_states;

            LeafState lstate = g_state_registry->lookup_leaf_state(id, leaf);
            for (int var : g_leaves[leaf]){
                member_state[var] = lstate[var];
            }

            int sum_dists = 0;
            for (PatternID p_id : affecting_patterns[leaf]){
                // TODO better store affecting patterns per clique?
                if (find(clique.begin(), clique.end(), p_id) != clique.end()){
                    sum_dists += (*pdbs)[p_id]->get_value(member_state);
                }
            }

            min_d = min(min_d, prices->get_cost_of_state(id, leaf) + sum_dists);
        }
        ++id;
    }
    return min_d;
}

int CanonicalPDBs::get_value_dec_exponential(const GlobalState &state) const {
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
        compute_min_distance_decoupled_exponential(prices, member_state, 0, 0, min_h);
        return min_h;
    }
}

void CanonicalPDBs::compute_min_distance_decoupled_exponential(const ExplicitStateCPG *cpg,
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
                compute_min_distance_decoupled_exponential(cpg, member_state, i + 1, sum_prices + price, min_d);
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
}
