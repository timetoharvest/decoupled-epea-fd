#include "pattern_filter.h"

#include "../globals.h"
#include "../leaf_state_id.h"
#include "../plugin.h"

#include "../options/option_parser.h"

using namespace std;

namespace pdbs {
unordered_set<unsigned short> get_leaf_factors_of_pattern(const Pattern &pattern) {
    assert(g_factoring);
    unordered_set<unsigned short> leaf_factors;
    for (int var : pattern) {
        LeafFactorID leaf_factor = g_belongs_to_factor[var];
        if (leaf_factor != LeafFactorID::CENTER) {
            leaf_factors.insert(leaf_factor);
        }
    }
    return leaf_factors;
}

void add_pattern_filter_option_to_parser(options::OptionParser &parser) {
    parser.add_option<shared_ptr<PatternFilter>>(
    "pattern_filter",
    "restrict patterns to certain variables",
    options::OptionParser::NONE);
}

shared_ptr<PatternFilter> get_pattern_filter_from_options(
    const options::Options &opts) {
    if (opts.contains("pattern_filter")) {
        return opts.get<shared_ptr<PatternFilter>>("pattern_filter");
    }
    return nullptr;
}

static PluginTypePlugin<PatternFilter> _type_plugin(
    "PatternFilter",
    "Methods for filtering patterns");
}
