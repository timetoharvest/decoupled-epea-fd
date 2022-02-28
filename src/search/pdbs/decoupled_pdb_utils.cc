#include "decoupled_pdb_utils.h"

#include "pattern_database.h"
#include "pattern_filter.h"

#include "../compliant_paths/cpg_storage.h"
#include "../compliant_paths/explicit_state_cpg.h"

#include "../globals.h"
#include "../tasks/root_task.h"

using namespace std;

namespace pdbs {
void set_center_facts(const GlobalState &center_state, std::vector<int> &state) {
    assert(state.size() == g_variable_domain.size());
    for (int var : g_center){
        state[var] = center_state[var];
    }
}

void set_leaf_facts(LeafStateHash id, LeafFactorID factor, std::vector<int> &state) {
    assert(state.size() == g_variable_domain.size());
    LeafState lstate = g_state_registry->lookup_leaf_state(id, factor);
    for (int var : g_leaves[factor]){
        state[var] = lstate[var];
    }
}

int unrank(const PatternDatabase& pdb, int rank, int pattern_index) {
    int temp = rank / pdb.get_hash_multipliers()[pattern_index];
    int var = pdb.get_pattern()[pattern_index];
    return temp % tasks::g_root_task->get_variable_domain_size(var);
}

vector<int> unrank(const PatternDatabase& pdb, int rank) {
    vector<int> values;
    values.reserve(pdb.get_pattern().size());
    for (size_t pattern_index = 0; pattern_index < pdb.get_pattern().size(); ++pattern_index) {
        values.push_back(unrank(pdb, rank, pattern_index));
    }
    return values;
}

static void compute_min_distance_mem_states(
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
                compute_min_distance_mem_states(pdb, cpg, affected_leaves, member_state, i + 1, sum_prices + price, min_d);
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

static void compute_min_distance_abs_states(
        const PatternDatabase &pdb,
        const vector<vector<pair<int, int>>> &abs_leaf_state_hash_prices,
        int partial_index,
        int sum_prices,
        int i,
        int &min_d) {

    if (min_d == 0 || sum_prices >= min_d){
        return;
    }

    if (i == static_cast<int>(abs_leaf_state_hash_prices.size())){
        int h = pdb.get_value(partial_index);
        if (h != numeric_limits<int>::max()){
            min_d = min(min_d, sum_prices + h);
        }
        return;
    }

    for (const auto &[partial_leaf_index, price] : abs_leaf_state_hash_prices[i]){
        compute_min_distance_abs_states(pdb,
                                        abs_leaf_state_hash_prices,
                                        partial_index + partial_leaf_index,
                                        sum_prices + price,
                                        i + 1,
                                        min_d);
    }
}

int compute_pdb_value_with_prices(
    const GlobalState &state,
    const PatternDatabase &pdb,
    const ExplicitStateCPG* prices,
    const vector<LeafFactorID> &affected_leaves,
    bool polynomial) {
    int min_h = numeric_limits<int>::max();
    if (affected_leaves.empty()){
        min_h = pdb.get_value(state);
    } else {
        if (polynomial) {
            const vector<int> &hash_multipliers = pdb.get_hash_multipliers();
            vector<vector<pair<int, int>>> abs_leaf_state_hash_prices;
            int center_index = 0;
            const vector<int> &pattern = pdb.get_pattern();
            vector<vector<int>> affected_leaf_var_ids(g_leaves.size());
            for (int i = 0; i < static_cast<int>(pattern.size()); ++i){
                LeafFactorID factor = g_belongs_to_factor[pattern[i]];
                if (factor == LeafFactorID::CENTER){
                    center_index += hash_multipliers[i] * state[pattern[i]];
                } else {
                    affected_leaf_var_ids[factor].push_back(i);
                }
            }
            for (LeafFactorID leaf : get_leaf_factors_of_pattern(pdb.get_pattern())){
                abs_leaf_state_hash_prices.emplace_back();
                int num_reached_lstates = prices->get_number_states(leaf);
                for (LeafStateHash id(0); id < g_state_registry->size(leaf); ++id){
                    if (prices->has_leaf_state(id, leaf)) {
                        // TODO since many reached leaf states can lead to the same partial_index if the leaf
                        // is not fully covered, we might want to cache the cheapest price for all partial_index'es
                        // if |leaf| >> |leaf \cap patter| this could make quite a difference; also be because within
                        // compute_min_distance_abs_states, we potentially handle the same state very often
                        LeafState lstate = g_state_registry->lookup_leaf_state(id, leaf);
                        int partial_index = 0;
                        for (int i: affected_leaf_var_ids[leaf]) {
                            partial_index += hash_multipliers[i] * lstate[pattern[i]];
                        }
                        abs_leaf_state_hash_prices.back().push_back(make_pair(partial_index, prices->get_cost_of_state(id, leaf)));
                        if (--num_reached_lstates == 0){
                            break;
                        }
                    }
                }
            }
            compute_min_distance_abs_states(pdb, abs_leaf_state_hash_prices, center_index, 0, 0, min_h);
        } else {
            // used to call get_value(vector<int>) for all member states
            vector<int> member_state(g_variable_domain.size(), -1);
            set_center_facts(state, member_state);
            compute_min_distance_mem_states(pdb, prices, affected_leaves, member_state, 0, 0, min_h);
        }
    }
    return min_h;
}
}
