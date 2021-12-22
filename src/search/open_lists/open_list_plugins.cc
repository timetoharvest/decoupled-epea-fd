#include "open_list_plugins.h"

#include "standard_scalar_open_list.h"

#include "open_list.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../state_id.h"

using namespace std;


static PluginTypePlugin<OpenList<StateID>> _type_plugin_openlist_stateid(
    "OpenList<StateID>",
    "HACK: this plugin exists only to let the option parser registry"
    "know about the templated open list.");

static shared_ptr<OpenList<StateID>> _parse_standard_scalar_list_stateid(OptionParser &parser) {
    parser.document_synopsis("Standard-scalar open list with StateID",
            "HACK: this plugin exists only to let the option parser registry"
            "know about the templated open list.");

    Options opts = parser.parse();
    if (parser.dry_run())
        return 0;
    else
        return make_shared<StandardScalarOpenList<StateID>>(opts);
}

static Plugin<OpenList<StateID>> _plugin_openlist("never_use_this", _parse_standard_scalar_list_stateid);
