#include "lp_factorings.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../factoring.h"
#include "../lp/mip_solver.h"
#include "../task_utils/causal_graph.h"

#include <iostream>


using namespace std;


LPFactoring::LPFactoring(const Options &opts) : Factoring(opts) {
     profile = STAR;
     strategy = STRATEGY(opts.get_enum("strategy"));
     cout << "using LP factoring strategy" << endl;
}

Factoring::FactoredVars LPFactoring::get_factoring() {

    // TODO implement factoring here
    FactoredVars factoring;

    switch (strategy) {
      case MIS: factoring = get_mip_factoring(); break;
      default: exit_with(utils::ExitCode::CRITICAL_ERROR);
    }

    return factoring;

}

Factoring::FactoredVars LPFactoring::get_mip_factoring() {

    FactoredVars factoring;

    mip::MIPSolver mip_solver (mip::MIPSolverType::CPLEX);
    vector<mip::MIPVariable> variables;
    vector<mip::MIPConstraint> constraints;

    for (size_t var = 0; var < g_variable_domain.size(); var++) {
        variables.push_back(mip::MIPVariable(0.0,1.0,1.0,mip::MIPVariableType::INT));
        for (int suc : causal_graph::get_causal_graph().get_successors(var)) {
            mip::MIPConstraint constraint (0.0,1.0);
            constraint.insert(var,1.0);
            constraint.insert(suc,1.0);
            constraints.push_back(constraint);
        }
    }

    mip_solver.load_problem(mip::MIPObjectiveSense::MAXIMIZE,variables,constraints);
    mip_solver.solve();

    vector<double> solution = mip_solver.extract_solution();
    for (size_t var = 0; var < solution.size(); var++) {
        if (solution.at(var) == 1.0) {
            set<int> leaf;
            leaf.insert(var);
            factoring.push_back(leaf);
        }
    }

    return factoring;

}



void LPFactoring::add_options_to_parser(OptionParser &parser) {
    parser.add_option<bool>(
            "some_option_name",
            "TODO",
            "false"
    );

    vector<string> strategies;
    strategies.push_back("mis");
    parser.add_enum_option(
        "strategy",
        strategies,
        "TODO",
        "mis",
        vector<string>(),
        OptionFlags(false));
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
