#include "operator_selection_function.h"

#include "globals.h"
#include "operator.h"
#include "state.h"

#include "pdbs/pattern_database.h"

#include <cassert>

using namespace std;

DirectPDBOSF::DirectPDBOSF(const pdbs::PatternDatabase &pdb)
    : pdb(pdb) {

    pattern_position.resize(g_variable_domain.size(), -1);
    const vector<int> &pattern = pdb.get_pattern();
    int i = 0;
    for (int var : pattern) {
        pattern_position[var] = i++;
    }
}

int DirectPDBOSF::predict_successor_h(const GlobalState &state,
                                      const Operator &op) const {
    assert(!op.is_axiom());

    const vector<int> &pattern = pdb.get_pattern();
    const vector<int> &multipliers = pdb.get_hash_multipliers();
    int rank = 0;
    for (size_t i = 0; i < pattern.size(); ++i) {
        rank += multipliers[i] * state[pattern[i]];
    }

    for (const Effect &effect : op.get_effects()) {

        assert(effect.conditions.empty());
        int position = pattern_position[effect.var];
        if (position != -1) {
            rank += multipliers[position]
                    * (effect.val - state[effect.var]);
        }
    }

    return pdb.get_value(rank);
}

int DirectPDBOSF::table_value(const GlobalState &state) const {

    return pdb.get_value(state);
}
