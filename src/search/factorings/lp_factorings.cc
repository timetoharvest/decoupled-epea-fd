#include "lp_factorings.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../factoring.h"

#include "../lp/lp_solver.h"

#include <iostream>


using namespace std;



LPFactoring::LPFactoring(const Options &opts) : Factoring(opts) {
     profile = STAR;
     cout << "using LP factoring strategy" << endl;
}


Factoring::FactoredVars LPFactoring::get_factoring() {

    // TODO implement factoring here
    return FactoredVars();
}



void LPFactoring::add_options_to_parser(OptionParser &parser) {
    parser.add_option<bool>(
            "some_option_name",
            "TODO",
            "false"
    );
}

static Factoring *_parse_lp(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    LPFactoring::add_options_to_parser(parser);

    Options opts = parser.parse();

    LPFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new LPFactoring(opts);
    }

    return factoring;
}


static Plugin<Factoring> _plugin_lp("lp_factoring", _parse_lp);
