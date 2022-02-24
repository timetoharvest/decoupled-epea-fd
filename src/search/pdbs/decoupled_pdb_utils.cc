#include "decoupled_pdb_utils.h"

#include "pattern_database.h"

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

static void compute_min_distance(
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

int compute_price() {
    // TODO
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
            int min_h = numeric_limits<int>::max();
            for (int index = 0; index < pdb.get_size(); ++index) {
                vector<int> abs_state = unrank(pdb, index);
                int price = compute_price();
                int h = pdb.get_value(index);
                min_h = min(min_h, h + price);
            }
        } else {
            // used to call get_value(vector<int>) for all member states
            vector<int> member_state(g_variable_domain.size(), -1);
            set_center_facts(state, member_state);
            compute_min_distance(pdb, prices, affected_leaves, member_state, 0, 0, min_h);
        }
    }
    return min_h;
}
}
