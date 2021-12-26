#include "pattern_filter.h"

#include "../plugin.h"

namespace pdbs {
static PluginTypePlugin<PatternFilter> _type_plugin(
    "PatternFilter",
    "Methods for filtering patterns");
}
