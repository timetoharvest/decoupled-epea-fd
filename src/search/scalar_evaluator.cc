#include "scalar_evaluator.h"

#include "plugin.h"

// TODO: this is moved to Evaluator in newer versions of Fast Downward
static PluginTypePlugin<ScalarEvaluator> _type_plugin(
    "ScalarEvaluator",
    "TODO",
    "scalar_evaluator");
