#include "decoupled_canonical_pdbs_exponential.h"

#include "decoupled_pdb_utils.h"
#include "pattern_database.h"
#include "pattern_filter.h"

#include "../globals.h"
#include "../state.h"

#include "../compliant_paths/cpg_storage.h"
#include "../compliant_paths/explicit_state_cpg.h"

#include "../option_parser.h"

#include <cassert>

using namespace std;

namespace pdbs {
DecoupledCanonicalPDBsExponential::DecoupledCanonicalPDBsExponential(
    const shared_ptr<PDBCollection> &pdbs,
    const shared_ptr<vector<PatternClique>> &pattern_cliques,
    ExplicitLeafSortingType sort_leaves_type)
    : CanonicalPDBs(pdbs, pattern_cliques), sort_leaves_type(sort_leaves_type) {

    unordered_set<LeafFactorID> aff_leaves;
    affected_leaves_by_pattern.resize(pdbs->size());
    vector<int> num_patterns_per_leaf(g_leaves.size());
    for (PatternID pid = 0; pid < static_cast<int>(pdbs->size()); ++pid) {
        unordered_set<LeafFactorID> tmp = get_leaf_factors_of_pattern((*pdbs)[pid]->get_pattern());
        affected_leaves_by_pattern[pid] = vector<LeafFactorID>(tmp.begin(), tmp.end());
        if (affected_leaves_by_pattern[pid].empty()) {
            center_only_patterns.push_back(pid);
        } else {
            for (LeafFactorID leaf : affected_leaves_by_pattern[pid]) {
                num_patterns_per_leaf[leaf]++;
                aff_leaves.insert(leaf);
            }
        }
    }
    affected_leaves = vector<LeafFactorID>(aff_leaves.begin(), aff_leaves.end());

    for (int cid = 0; cid < static_cast<int>(pattern_cliques->size()); ++cid) {
        bool is_center_clique = true;
        for (PatternID pid : (*pattern_cliques)[cid]) {
            if (!affected_leaves_by_pattern[pid].empty()){
                is_center_clique = false;
                break;
            }
        }
        if (is_center_clique) {
            center_only_cliques.push_back(cid);
        }
    }

    // TODO add random and sorting according to leaf ID
    // TODO add bool option for early termination

    if (sort_leaves_type == ExplicitLeafSortingType::STATIC) {
        // sort leaf factors by descending number of affecting patterns
        sort(affected_leaves.begin(), affected_leaves.end(),
             [&](const LeafFactorID &l1, const LeafFactorID &l2) -> bool {
                 return num_patterns_per_leaf[l1] > num_patterns_per_leaf[l2];
             });
    }

    apply_resorted_affected_leaves();
}

void DecoupledCanonicalPDBsExponential::apply_resorted_affected_leaves() const {
    cliques_by_rec_depth = vector<vector<PatternID>>(g_leaves.size());
    patterns_by_rec_depth = vector<vector<PatternID>>(g_leaves.size());
    vector<int> rec_depth_by_pattern(pdbs->size(), -1);
    vector<int> leaf_indices(g_leaves.size());
    for (int i = 0; i < static_cast<int>(affected_leaves.size()); ++i){
        leaf_indices[affected_leaves[i]] = i;
    }
    for (PatternID pid = 0; pid < static_cast<int>(pdbs->size()); ++pid) {
        int max_d = 0;
        for (LeafFactorID leaf: affected_leaves_by_pattern[pid]){
            max_d = max(max_d, leaf_indices[leaf]);
            if (max_d == static_cast<int>(affected_leaves.size() - 1)){
                break;
            }
        }
        rec_depth_by_pattern[pid] = max_d;
        patterns_by_rec_depth[max_d].push_back(pid);
    }
    for (int cid = 0; cid < static_cast<int>(pattern_cliques->size()); ++cid) {
        int max_c_d = 0;
        for (PatternID pid: (*pattern_cliques)[cid]) {
            max_c_d = max(max_c_d, rec_depth_by_pattern[pid]);
            if (max_c_d == static_cast<int>(affected_leaves.size() - 1)){
                break;
            }
        }
        cliques_by_rec_depth[max_c_d].push_back(cid);
    }
}

int DecoupledCanonicalPDBsExponential::get_value(const GlobalState &state) const {
    // If we have an empty collection, then pattern_cliques = { \emptyset }.
    assert(!pattern_cliques->empty());

    if (affected_leaves.empty()){
        // compute maximal heuristic value for the center state
        return CanonicalPDBs::get_value(state);
    } else {
        // compute minimum heuristic value across all member states
        vector<int> member_state(g_variable_domain.size(), -1);
        set_center_facts(state, member_state);

        vector<int> h_values(pdbs->size());
        // the heuristic value of these patterns is independent of the leaves
        for (PatternID p_id : center_only_patterns){
            h_values[p_id] = (*pdbs)[p_id]->get_value(member_state);
            if (h_values[p_id] == numeric_limits<int>::max()){
                // center state is detected as deadend
                return numeric_limits<int>::max();
            }
        }

        int min_clique_h = 0;
        for (int cid : center_only_cliques){
            int clique_h = 0;
            for (PatternID pdb_index : (*pattern_cliques)[cid]) {
                clique_h += h_values[pdb_index];
            }
            min_clique_h = max(min_clique_h, clique_h);
        }

        const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));

        if (sort_leaves_type == ExplicitLeafSortingType::DYNAMIC) {
            // sort leaf factors by ascending number of reached leaf states
            if (!is_sorted(affected_leaves.begin(), affected_leaves.end(),
                                [&](const LeafFactorID &l1, const LeafFactorID &l2) -> bool {
                                    return prices->get_number_states(l1) < prices->get_number_states(l2);
                                })){
                sort(affected_leaves.begin(), affected_leaves.end(),
                     [&](const LeafFactorID &l1, const LeafFactorID &l2) -> bool {
                         return prices->get_number_states(l1) < prices->get_number_states(l2);
                     });
                apply_resorted_affected_leaves();
            }
        }

        int final_h = numeric_limits<int>::max();
        compute_min_distance_decoupled_exponential(prices, member_state, h_values, 0, 0, min_clique_h, final_h);
        return final_h;
    }
}

void DecoupledCanonicalPDBsExponential::compute_min_distance_decoupled_exponential(
        const ExplicitStateCPG *cpg,
        vector<int> &member_state,
        vector<int> &h_values,
        int i,
        int sum_prices,
        int min_clique_h,
        int &final_h) const {

    if (sum_prices + min_clique_h >= final_h){
        // we cannot possibly improve the heuristic
        return;
    }

    int number_states = cpg->get_number_states(affected_leaves[i]);
    for (LeafStateHash id(0); id < g_state_registry->size(affected_leaves[i]); ++id) {
        if (number_states == 0){
            break;
        }
        if (cpg->has_leaf_state(id, affected_leaves[i])){
            --number_states;

            int min_clique_h_lstate = min_clique_h;

            set_leaf_facts(id, affected_leaves[i], member_state);

            bool is_dead_end_member_state = false;
            for (PatternID p_id : patterns_by_rec_depth[i]){
                h_values[p_id] = (*pdbs)[p_id]->get_value(member_state);
                if (h_values[p_id] == numeric_limits<int>::max()){
                    // partial member state is detected as deadend
                    is_dead_end_member_state = true;
                    break;
                }
            }
            if (is_dead_end_member_state){
                continue;
            }

            for (int cid : cliques_by_rec_depth[i]){
                int clique_h = 0;
                for (PatternID pdb_index : (*pattern_cliques)[cid]) {
                    clique_h += h_values[pdb_index];
                }
                min_clique_h_lstate = max(min_clique_h_lstate, clique_h);
            }

            int price = cpg->get_cost_of_state(id, affected_leaves[i]);
            if (i < static_cast<int>(affected_leaves.size()) - 1){
                // recursion
                compute_min_distance_decoupled_exponential(cpg,
                                                           member_state,
                                                           h_values,
                                                           i + 1,
                                                           sum_prices + price,
                                                           min_clique_h_lstate,
                                                           final_h);
            } else {
                // end recursion
                final_h = min(final_h, sum_prices + price + min_clique_h_lstate);
            }
        }
    }
}

void add_exponential_options_to_parser(options::OptionParser &parser) {
    vector<string> sort_leaves_types;
    vector<string> sort_leaves_descriptions;

    sort_leaves_types.push_back("NO");
    sort_leaves_descriptions.push_back("Do not resort the leaves, keep arbitrary initial order.");

    sort_leaves_types.push_back("STATIC");
    sort_leaves_descriptions.push_back("Sort once before search, descending by number of patterns affecting a leaf.");

    sort_leaves_types.push_back("DYNAMIC");
    sort_leaves_descriptions.push_back("Resort for every decoupled state, ascending by number of reached leaf states per leaf.");

    parser.add_enum_option<ExplicitLeafSortingType>(
            "sort_leaves_type",
            sort_leaves_types,
            "TODO: Help",
            "NO",
            sort_leaves_descriptions);
}
}
