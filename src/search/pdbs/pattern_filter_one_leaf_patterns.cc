#include "pattern_filter_one_leaf_patterns.h"

#include "../globals.h"
#include "../leaf_state_id.h"
#include "../plugin.h"

#include "../utils/logging.h"

#include <memory>
#include <unordered_set>

using namespace std;

namespace pdbs {
unordered_set<unsigned short> get_leaf_factors_of_pattern(const Pattern &pattern) {
    unordered_set<unsigned short> leaf_factors;
    for (int var : pattern) {
        LeafFactorID leaf_factor = g_belongs_to_factor[var];
        if (leaf_factor != LeafFactorID::CENTER) {
            leaf_factors.insert(leaf_factor);
        }
    }
    return leaf_factors;
}

bool PatternFilterOneLeafPatterns::is_pattern_ok(const Pattern &pattern) const {
    return get_leaf_factors_of_pattern(pattern).size() <= 1;
}

// Return the unique leaf factor of the pattern if it contains non-center
// variables and LeafFactorID::CENTER otherwise.
LeafFactorID get_leaf_factor_of_pattern(const Pattern &pattern) {
    unordered_set<unsigned short> leaf_factors = get_leaf_factors_of_pattern(pattern);
    assert(get_leaf_factors_of_pattern(pattern).size() <= 1);
    if (leaf_factors.empty()) {
        return LeafFactorID::CENTER;
    } else {
        return LeafFactorID(*leaf_factors.begin());
    }
}

bool PatternFilterOneLeafPatterns::can_add_var_to_pattern(
    const Pattern &pattern, int var) const {
//    utils::g_log << "can add " << var << " to " << pattern << "? ";
    LeafFactorID var_factor_id = g_belongs_to_factor[var];
    LeafFactorID leaf_factor_id = get_leaf_factor_of_pattern(pattern);
    if (var_factor_id == LeafFactorID::CENTER || leaf_factor_id == LeafFactorID::CENTER) {
//        utils::g_log << "yes" << endl;
        return true;
    } else {
//        utils::g_log << (leaf_factor_id == var_factor_id ? "yes" : "no") << endl;
        return leaf_factor_id == var_factor_id;
    }
}

bool PatternFilterOneLeafPatterns::can_merge_patterns(
    const Pattern &pattern1, const Pattern &pattern2) const {
//    utils::g_log << "can merge " << pattern1 << " and " << pattern2 << "? ";
    LeafFactorID leaf_factor_id_1 = get_leaf_factor_of_pattern(pattern1);
    LeafFactorID leaf_factor_id_2 = get_leaf_factor_of_pattern(pattern2);
    if (leaf_factor_id_1 == LeafFactorID::CENTER || leaf_factor_id_2 == LeafFactorID::CENTER) {
//        utils::g_log << "yes" << endl;
        return true;
    } else {
//        utils::g_log << (leaf_factor_id_1 == leaf_factor_id_2 ? "yes" : "no") << endl;
        return leaf_factor_id_1 == leaf_factor_id_2;
    }
}

static shared_ptr<PatternFilter> _parse(options::OptionParser &parser) {
    if (parser.dry_run()) {
        return nullptr;
    }

    return make_shared<PatternFilterOneLeafPatterns>();
}

static Plugin<PatternFilter> _plugin("one_leaf_patterns", _parse);
}
