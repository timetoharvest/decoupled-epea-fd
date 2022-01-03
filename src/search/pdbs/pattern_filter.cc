#include "pattern_filter.h"

#include "../options/option_parser.h"
#include "../plugin.h"

using namespace std;

namespace pdbs {
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
