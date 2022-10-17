#include "merge_and_shrink_representation.h"

#include "distances.h"
#include "types.h"

#include "../globals.h"
#include "../state.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../compliant_paths/cpg_storage.h"
#include "../compliant_paths/explicit_state_cpg.h"

#include "../utils/logging.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>

using namespace std;

namespace merge_and_shrink {
MergeAndShrinkRepresentation::MergeAndShrinkRepresentation(int domain_size)
    : domain_size(domain_size), leaf_id(LeafFactorID::CENTER) {
}

MergeAndShrinkRepresentation::~MergeAndShrinkRepresentation() {
}

static vector<vector<int>> compute_reached_values_by_var(const GlobalState &state) {
    // Compute all reached facts (list of values by var) for both leaf and center vars.
    const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    int num_leaves = g_leaves.size();
    vector<vector<int>> reached_values_by_var(g_variable_domain.size());
    for (LeafFactorID leaf(0); leaf < num_leaves; ++leaf) {
        int num_reached_lstates = prices->get_number_states(leaf);
        for (LeafStateHash id(0); id < g_state_registry->size(leaf); ++id) {
            if (prices->has_leaf_state(id, leaf)) {
                LeafState lstate = g_state_registry->lookup_leaf_state(id, leaf);
                for (int leaf_var : g_leaves[leaf]) {
                    reached_values_by_var[leaf_var].push_back(lstate[leaf_var]);
                }
                if (--num_reached_lstates == 0) {
                    break;
                }
            }
        }
        for (int leaf_var : g_leaves[leaf]) {
            utils::sort_unique(reached_values_by_var[leaf_var]);
        }
    }
    for (int var : g_center) {
        reached_values_by_var[var].push_back(state[var]);
    }
    return reached_values_by_var;
}

// TODO: copy of compute_min_leaf_price below except for using
// reached_leaf_values_by_var_index which is now indexed by pos rather than var.
static int compute_min_leaf_price_exact(
    const ExplicitStateCPG *prices,
    LeafFactorID leaf_id,
    int pos,
    const vector<vector<int>> &reached_leaf_values_by_var_index,
    vector<int> &leaf_state) {
    const vector<int> &leaf_vars = g_leaves[leaf_id];
    const vector<int> &reached_leaf_values = reached_leaf_values_by_var_index[pos];
    int min_price = INF;
    for (int value : reached_leaf_values) {
        leaf_state[pos] = value;
        int price;
        if (pos < static_cast<int>(leaf_vars.size()) - 1) {
            price = compute_min_leaf_price_exact(
                prices,
                leaf_id,
                pos + 1,
                reached_leaf_values_by_var_index,
                leaf_state);
        } else {
            LeafStateHash leaf_state_id = g_state_registry->get_leaf_state_hash(leaf_state, leaf_id);
            if (prices->has_leaf_state(leaf_state_id, leaf_id)) {
                price = prices->get_cost_of_state(leaf_state_id, leaf_id);
            } else {
                price = INF;
            }
        }
        min_price = min(min_price, price);
    }
    return min_price;
}

int MergeAndShrinkRepresentation::compute_decoupled_value_exact(const GlobalState &state) const {
    assert(g_factoring);
    vector<vector<int>> reached_values_by_var = compute_reached_values_by_var(state);
    const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    unordered_map<int, pair<int, vector<vector<pair<int, int>>>>> hval_to_price_and_reached_leaf_facts =
        get_decoupled_value_exact(prices, reached_values_by_var);

    int num_leaves = g_leaves.size();
    int min_h_and_price = INF;
    for (auto &entry : hval_to_price_and_reached_leaf_facts) {
        int h = entry.first;
        if (h == INF || h == PRUNED_STATE) {
            continue;
        }
        // TODO: when computing prices on the way, use them here.
        assert(entry.second.first == -1);
        int sum_prices = 0;
        for (LeafFactorID leaf(0); leaf < num_leaves; ++leaf) {
            const vector<int> &leaf_vars = g_leaves[leaf];
            for (const vector<pair<int, int>> &reached_facts : entry.second.second) {
                vector<vector<int>> reached_leaf_values_by_leaf_var_index(leaf_vars.size());
                for (size_t i = 0; i < leaf_vars.size(); ++i) {
                    int leaf_var  = leaf_vars[i];
                    for (const pair<int, int> &fact : reached_facts)  {
                        if (fact.first == leaf_var) {
                            reached_leaf_values_by_leaf_var_index[i].push_back(fact.second);
                        }
                    }
                    // Use reached values for a leaf var if it is not covered by the M&S tree.
                    if (reached_leaf_values_by_leaf_var_index[i].empty()) {
                        reached_leaf_values_by_leaf_var_index[i] = reached_values_by_var[leaf_var];
                    }
                }
                vector<int> leaf_state(leaf_vars.size());
                int price = compute_min_leaf_price_exact(
                    prices,
                    leaf,
                    0,
                    reached_leaf_values_by_leaf_var_index,
                    leaf_state);
                if (price == INF) {
                    sum_prices = INF;
                    break;
                }
                sum_prices += price;
            }

        }
        if (sum_prices != INF) {
            min_h_and_price = min(min_h_and_price, h + sum_prices);
        }
    }
    return min_h_and_price;
}

static int compute_min_leaf_price(
    const ExplicitStateCPG *prices,
    LeafFactorID leaf_id,
    int pos,
    const vector<vector<int>> &reached_leaf_values_by_var,
    vector<int> &leaf_state) {
    const vector<int> &leaf_vars = g_leaves[leaf_id];
    int var = leaf_vars[pos];
    const vector<int> &reached_leaf_values = reached_leaf_values_by_var[var];
    int min_price = INF;
    for (int value : reached_leaf_values) {
        leaf_state[pos] = value;
        int price;
        if (pos < static_cast<int>(leaf_vars.size()) - 1) {
            price = compute_min_leaf_price(
                prices,
                leaf_id,
                pos + 1,
                reached_leaf_values_by_var,
                leaf_state);
        } else {
            LeafStateHash leaf_state_id = g_state_registry->get_leaf_state_hash(leaf_state, leaf_id);
            if (prices->has_leaf_state(leaf_state_id, leaf_id)) {
                price = prices->get_cost_of_state(leaf_state_id, leaf_id);
            } else {
                price = INF;
            }
        }
        min_price = min(min_price, price);
    }
    return min_price;
}

int MergeAndShrinkRepresentation::compute_decoupled_value_approx(const GlobalState &state) const {
    assert(g_factoring);
    vector<vector<int>> reached_values_by_var = compute_reached_values_by_var(state);
    const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    unordered_map<int, vector<vector<int>>> hval_to_reached_leaf_values_by_var =
        get_decoupled_value_approx(prices, reached_values_by_var);

    int num_leaves = g_leaves.size();
    int min_h_and_price = INF;
    for (auto &entry : hval_to_reached_leaf_values_by_var) {
        int h = entry.first;
        if (h == INF || h == PRUNED_STATE) {
            continue;
        }
        int sum_prices = 0;
        for (LeafFactorID leaf(0); leaf < num_leaves; ++leaf) {
            const vector<int> &leaf_vars = g_leaves[leaf];
            auto &reached_leaf_values_by_var = entry.second;
            // Use reached values for a leaf var if it is not covered by the M&S tree.
            for (int leaf_var : leaf_vars) {
                if (reached_leaf_values_by_var[leaf_var].empty()) {
                    reached_leaf_values_by_var[leaf_var] = reached_values_by_var[leaf_var];
                }
            }
            vector<int> leaf_state(leaf_vars.size());
            int price = compute_min_leaf_price(
                prices,
                leaf,
                0,
                entry.second,
                leaf_state);
            if (price == INF) {
                sum_prices = INF;
                break;
            }
            sum_prices += price;
        }
        if (sum_prices != INF) {
            min_h_and_price = min(min_h_and_price, h + sum_prices);
        }
    }
    return min_h_and_price;
}

int MergeAndShrinkRepresentation::compute_decoupled_value_approx_precomputed(
    const GlobalState &state) const {
    const ExplicitStateCPG *prices = dynamic_cast<const ExplicitStateCPG*>(CPGStorage::storage->get_cpg(state));
    vector<int> center_hvals = get_decoupled_values_approx_precomputed(state);
    // we filtered all pruned states and states with h-val of infinity.
    int min_h_and_price = INF;
    for (int h_val : center_hvals) {
        if (h_val == PRUNED_STATE || h_val == INF) {
            continue;
        }
        int sum_prices = 0;
        for (LeafFactorID leaf(0); leaf < leaf_state_ids_by_leaf_id_by_hval[h_val].size(); ++leaf) {
            const auto &leaf_state_ids = leaf_state_ids_by_leaf_id_by_hval[h_val][leaf];
            int min_price = INF;
            for (LeafStateHash leaf_state_id : leaf_state_ids) {
                if (prices->has_leaf_state(leaf_state_id, leaf)) {
                    int price = prices->get_cost_of_state(leaf_state_id, leaf);
                    min_price = min(min_price, price);
                }
            }

            if (min_price == INF) {
                sum_prices = INF;
                break;
            } else {
                sum_prices += min_price;
            }
        }
        if (sum_prices != INF) {
            min_h_and_price = min(min_h_and_price, h_val + sum_prices);
        }
    }
    return min_h_and_price;
}

static void compute_leaf_states(
    LeafFactorID leaf_id,
    int pos,
    vector<int> &leaf_state,
    const vector<vector<int>> &values_by_var,
    vector<vector<LeafStateHash>> &leaf_state_ids_by_leaf_id) {
    const vector<int> &leaf_vars = g_leaves[leaf_id];
    int var = leaf_vars[pos];
    for (int value : values_by_var[var]) {
        leaf_state[pos] = value;
        if (pos < static_cast<int>(leaf_vars.size()) - 1) {
            compute_leaf_states(
                leaf_id,
                pos + 1,
                leaf_state,
                values_by_var,
                leaf_state_ids_by_leaf_id);
        } else {
            LeafStateHash leaf_state_id = g_state_registry->get_leaf_state_hash(leaf_state, leaf_id);
            leaf_state_ids_by_leaf_id[leaf_id].push_back(leaf_state_id);
        }
    }
}

void MergeAndShrinkRepresentation::store_leaf_state_ids_by_leaf_id_by_hval() {
    int num_leaves = g_leaves.size();
    vector<vector<vector<int>>> values_by_var_by_hval = get_leaf_state_ids();
    int num_h_values = values_by_var_by_hval.size();
    leaf_state_ids_by_leaf_id_by_hval.resize(num_h_values, vector<vector<LeafStateHash>>(num_leaves));
    for (int h = 0; h < num_h_values; ++h) {
        vector<vector<int>> &values_by_var = values_by_var_by_hval[h];
        if (h == INF || h == PRUNED_STATE) {
            // TODO this can never happen!
            continue;
        }
        for (LeafFactorID leaf(0); leaf < num_leaves; ++leaf) {
            const vector<int> &leaf_vars = g_leaves[leaf];
            // Use all values for a leaf var if it is not covered by the M&S tree.
            for (int leaf_var : leaf_vars) {
                if (values_by_var[leaf_var].empty()) {
                    for (int value = 0; value < g_variable_domain[leaf_var]; ++value) {
                        values_by_var[leaf_var].push_back(value);
                    }
                }
            }
            vector<int> leaf_state(leaf_vars.size());
            compute_leaf_states(
                leaf,
                0,
                leaf_state,
                values_by_var,
                leaf_state_ids_by_leaf_id_by_hval[h]);
        }
    }
}

int MergeAndShrinkRepresentation::get_domain_size() const {
    return domain_size;
}


MergeAndShrinkRepresentationLeaf::MergeAndShrinkRepresentationLeaf(
    int var_id, int domain_size)
    : MergeAndShrinkRepresentation(domain_size),
      var_id(var_id),
      lookup_table(domain_size) {
    iota(lookup_table.begin(), lookup_table.end(), 0);
}

void MergeAndShrinkRepresentationLeaf::set_leaf_id(vector<bool> &contained_vars) {
    if (g_belongs_to_factor[var_id] != LeafFactorID::CENTER && g_leaves[g_belongs_to_factor[var_id]].size() == 1) {
        leaf_id = g_belongs_to_factor[var_id];
    } else {
        contained_vars[var_id] = true;
    }
}

void MergeAndShrinkRepresentationLeaf::set_distances(
    const Distances &distances) {
    assert(distances.are_goal_distances_computed());
    for (int &entry : lookup_table) {
        if (entry != PRUNED_STATE) {
            entry = distances.get_goal_distance(entry);
        }
    }
}

void MergeAndShrinkRepresentationLeaf::apply_abstraction_to_lookup_table(
    const vector<int> &abstraction_mapping) {
    int new_domain_size = 0;
    for (int &entry : lookup_table) {
        if (entry != PRUNED_STATE) {
            entry = abstraction_mapping[entry];
            new_domain_size = max(new_domain_size, entry + 1);
        }
    }
    domain_size = new_domain_size;
}

int MergeAndShrinkRepresentationLeaf::get_value(const GlobalState &state) const {
    int value = state[var_id];
    return lookup_table[value];
}

vector<int> MergeAndShrinkRepresentationLeaf::get_decoupled_values_approx_precomputed(
    const GlobalState &state) const {
    if (g_belongs_to_factor[var_id] == LeafFactorID::CENTER) {
        int value = state[var_id];
        return {lookup_table[value]};
    }
    vector<int> all_values(lookup_table);
    utils::sort_unique(all_values);
    return all_values;
}

unordered_map<int, vector<vector<int>>> MergeAndShrinkRepresentationLeaf::get_decoupled_value_approx(
    const ExplicitStateCPG *prices,
    const vector<vector<int>> &reached_values_by_var) const {
    unordered_map<int, vector<vector<int>>> index_to_reached_leaf_values_by_var;
    for (int value : reached_values_by_var[var_id]) {
        int index = lookup_table[value];
        if (index != PRUNED_STATE) {
            if (index_to_reached_leaf_values_by_var[index].empty()) {
                index_to_reached_leaf_values_by_var[index].resize(g_variable_domain.size());
            }
            index_to_reached_leaf_values_by_var[index][var_id].push_back(value);
        }
    }
    return index_to_reached_leaf_values_by_var;
}

unordered_map<int, pair<int, vector<vector<pair<int, int>>>>> MergeAndShrinkRepresentationLeaf::get_decoupled_value_exact(
    const ExplicitStateCPG *prices,
    const vector<vector<int>> &reached_values_by_var) const {
    unordered_map<int, pair<int, vector<vector<pair<int, int>>>>> index_to_price_and_reached_leaf_facts;
    for (int value : reached_values_by_var[var_id]) {
        int index = lookup_table[value];
        if (index != PRUNED_STATE) {
            if (!index_to_price_and_reached_leaf_facts.count(index)) {
                index_to_price_and_reached_leaf_facts[index] = make_pair<int, vector<vector<pair<int, int>>>>(-1, {});
            }
            index_to_price_and_reached_leaf_facts[index].second.push_back(vector<pair<int, int>>(1, {var_id, value}));
        }
    }
    return index_to_price_and_reached_leaf_facts;
}

vector<vector<vector<int>>> MergeAndShrinkRepresentationLeaf::get_leaf_state_ids() const {
    vector<vector<vector<int>>> values_by_var_by_index(domain_size, vector<vector<int>>(g_variable_domain.size()));
    for (size_t value = 0; value < lookup_table.size(); ++value) {
        int entry = lookup_table[value];
        if (entry != PRUNED_STATE) {
            values_by_var_by_index[entry][var_id].push_back(value);
        }
    }
    return values_by_var_by_index;
}

bool MergeAndShrinkRepresentationLeaf::is_total() const {
    for (int entry : lookup_table) {
        if (entry == PRUNED_STATE) {
            return false;
        }
    }
    return true;
}

void MergeAndShrinkRepresentationLeaf::dump(utils::LogProxy &log) const {
    if (log.is_at_least_debug()) {
        log << "lookup table (leaf): ";
        for (const auto &value : lookup_table) {
            log << value << ", ";
        }
        log << endl;
    }
}


MergeAndShrinkRepresentationMerge::MergeAndShrinkRepresentationMerge(
    unique_ptr<MergeAndShrinkRepresentation> left_child_,
    unique_ptr<MergeAndShrinkRepresentation> right_child_)
    : MergeAndShrinkRepresentation(left_child_->get_domain_size() *
                                   right_child_->get_domain_size()),
      left_child(move(left_child_)),
      right_child(move(right_child_)),
      lookup_table(left_child->get_domain_size(),
                   vector<int>(right_child->get_domain_size())) {
    int counter = 0;
    for (vector<int> &row : lookup_table) {
        for (int &entry : row) {
            entry = counter;
            ++counter;
        }
    }
}

void MergeAndShrinkRepresentationMerge::set_leaf_id(vector<bool> &contained_vars) {
    left_child->set_leaf_id(contained_vars);
    right_child->set_leaf_id(contained_vars);
    for (const auto &leaf : g_leaves) {
        bool is_subset = true;
        for (int var : leaf) {
            if (!contained_vars[var]) {
                is_subset = false;
                break;
            }
        }
        if (is_subset) {
            leaf_id = g_belongs_to_factor[leaf[0]];
            // Reset all leaf variables so that we only once record a leaf as "done".
            for (int var : leaf) {
                contained_vars[var] = false;
            }
            break;
        }
    }
}

void MergeAndShrinkRepresentationMerge::set_distances(
    const Distances &distances) {
    assert(distances.are_goal_distances_computed());
    for (vector<int> &row : lookup_table) {
        for (int &entry : row) {
            if (entry != PRUNED_STATE) {
                entry = distances.get_goal_distance(entry);
            }
        }
    }
}

void MergeAndShrinkRepresentationMerge::apply_abstraction_to_lookup_table(
    const vector<int> &abstraction_mapping) {
    int new_domain_size = 0;
    for (vector<int> &row : lookup_table) {
        for (int &entry : row) {
            if (entry != PRUNED_STATE) {
                entry = abstraction_mapping[entry];
                new_domain_size = max(new_domain_size, entry + 1);
            }
        }
    }
    domain_size = new_domain_size;
}

int MergeAndShrinkRepresentationMerge::get_value(
    const GlobalState &state) const {
    int state1 = left_child->get_value(state);
    int state2 = right_child->get_value(state);
    if (state1 == PRUNED_STATE || state2 == PRUNED_STATE)
        return PRUNED_STATE;
    return lookup_table[state1][state2];
}

vector<int> MergeAndShrinkRepresentationMerge::get_decoupled_values_approx_precomputed(
    const GlobalState &state) const {
    vector<int> values_left = left_child->get_decoupled_values_approx_precomputed(state);
    vector<int> values_right = right_child->get_decoupled_values_approx_precomputed(state);
    vector<int> values_merge;
    values_merge.reserve(lookup_table.size());
    for (int val_left : values_left) {
        if (val_left == PRUNED_STATE) {
            continue;
        }
        for (int val_right : values_right) {
            if (val_right == PRUNED_STATE) {
                continue;
            }
            int merge_index = lookup_table[val_left][val_right];
            if (merge_index != PRUNED_STATE) {
                values_merge.push_back(merge_index);
            }
        }
    }
    if (values_merge.empty()) {
        return {PRUNED_STATE};
    }
    return values_merge;
}

unordered_map<int, vector<vector<int>>> MergeAndShrinkRepresentationMerge::get_decoupled_value_approx(
    const ExplicitStateCPG *prices,
    const vector<vector<int>> &reached_values_by_var) const {
    unordered_map<int, vector<vector<int>>> index_to_reached_leaf_values_by_var_left = left_child->get_decoupled_value_approx(prices, reached_values_by_var);
    unordered_map<int, vector<vector<int>>> index_to_reached_leaf_values_by_var_right = right_child->get_decoupled_value_approx(prices, reached_values_by_var);
    if (leaf_id != LeafFactorID::CENTER) {
        // TODO: compute min price for leaf; abuse return type?
        // TODO: enumerate leaf states -> use extra method that we re-use at root node in case the leaf if not covered by the m&s representation?
    }
    unordered_map<int, vector<vector<int>>> index_to_reached_leaf_values_by_var_merge;
    for (auto left : index_to_reached_leaf_values_by_var_left) {
        for (auto right : index_to_reached_leaf_values_by_var_right) {
            int left_index = left.first;
            int right_index = right.first;
            int merge_index = lookup_table[left_index][right_index];
            if (merge_index != PRUNED_STATE) {
                auto &merge_values_by_var = index_to_reached_leaf_values_by_var_merge[merge_index];
                merge_values_by_var.resize(g_variable_domain.size());
                // TODO: if leaf_id != LeafFactorID::CENTER: do not add facts of handled leaf vars.
                for (size_t var_id = 0; var_id < g_variable_domain.size(); ++var_id) {
                    merge_values_by_var[var_id].insert(merge_values_by_var[var_id].end(), left.second[var_id].begin(), left.second[var_id].end());
                    merge_values_by_var[var_id].insert(merge_values_by_var[var_id].end(), right.second[var_id].begin(), right.second[var_id].end());
                }
            }
        }
    }
    return index_to_reached_leaf_values_by_var_merge;
}

unordered_map<int, pair<int, vector<vector<pair<int, int>>>>> MergeAndShrinkRepresentationMerge::get_decoupled_value_exact(
    const ExplicitStateCPG *prices,
    const vector<vector<int>> &reached_values_by_var) const {
    unordered_map<int, pair<int, vector<vector<pair<int, int>>>>> index_to_price_and_reached_facts_left = left_child->get_decoupled_value_exact(prices, reached_values_by_var);
    unordered_map<int, pair<int, vector<vector<pair<int, int>>>>> index_to_price_and_reached_facts_right = right_child->get_decoupled_value_exact(prices, reached_values_by_var);
    unordered_map<int, pair<int, vector<vector<pair<int, int>>>>> index_to_price_and_reached_facts_merge;
    for (const auto &left : index_to_price_and_reached_facts_left) {
        for (const auto &right : index_to_price_and_reached_facts_right) {
            int left_index = left.first;
            int right_index = right.first;
            int merge_index = lookup_table[left_index][right_index];
            if (merge_index != PRUNED_STATE) {
                int min_price = min(left.second.first, right.second.first);
                index_to_price_and_reached_facts_merge[merge_index].first = min_price;
                index_to_price_and_reached_facts_merge[merge_index].second = vector<vector<pair<int, int>>>();
                vector<vector<pair<int, int>>> &merge_reached_facts = index_to_price_and_reached_facts_merge[merge_index].second;
                for (const vector<pair<int, int>> &reached_facts_left : left.second.second) {
                    for (const vector<pair<int, int>> &reached_facts_right : right.second.second) {
                        vector<pair<int, int>> temp(reached_facts_left);
                        temp.insert(temp.end(), reached_facts_right.begin(), reached_facts_right.end());
                        merge_reached_facts.push_back(move(temp));
                    }
                }
            }
        }
    }
    return index_to_price_and_reached_facts_merge;
}

vector<vector<vector<int>>> MergeAndShrinkRepresentationMerge::get_leaf_state_ids() const {
    vector<vector<vector<int>>> values_by_var_by_index_left = left_child->get_leaf_state_ids();
    vector<vector<vector<int>>> values_by_var_by_index_right = right_child->get_leaf_state_ids();
    vector<vector<vector<int>>> leaf_state_ids_by_var_by_index_merge(domain_size, vector<vector<int>>(g_variable_domain.size()));
    for (size_t left_index = 0; left_index < values_by_var_by_index_left.size(); ++left_index) {
        for (size_t right_index = 0; right_index < values_by_var_by_index_right.size(); ++right_index) {
            int merge_index = lookup_table[left_index][right_index];
            if (merge_index != PRUNED_STATE && merge_index != INF) {
                auto &merge_values_by_var = leaf_state_ids_by_var_by_index_merge[merge_index];
                for (size_t var_id = 0; var_id < g_variable_domain.size(); ++var_id) {
                    merge_values_by_var[var_id].insert(
                        merge_values_by_var[var_id].end(),
                        values_by_var_by_index_left[left_index][var_id].begin(),
                        values_by_var_by_index_left[left_index][var_id].end());
                    merge_values_by_var[var_id].insert(
                        merge_values_by_var[var_id].end(),
                        values_by_var_by_index_right[right_index][var_id].begin(),
                        values_by_var_by_index_right[right_index][var_id].end());
                }
            }
        }
    }
    return leaf_state_ids_by_var_by_index_merge;
}

bool MergeAndShrinkRepresentationMerge::is_total() const {
    for (const vector<int> &row : lookup_table) {
        for (int entry : row) {
            if (entry == PRUNED_STATE) {
                return false;
            }
        }
    }
    return left_child->is_total() && right_child->is_total();
}

void MergeAndShrinkRepresentationMerge::dump(utils::LogProxy &log) const {
    if (log.is_at_least_debug()) {
        log << "lookup table (merge): " << endl;
        for (const auto &row : lookup_table) {
            for (const auto &value : row) {
                log << value << ", ";
            }
            log << endl;
        }
        log << "left child:" << endl;
        left_child->dump(log);
        log << "right child:" << endl;
        right_child->dump(log);
    }
}
}
