#include "pruning_options.h"

#include "compliant_path_graph.h"
#include "../factoring.h"
#include "../plugin.h"
#include "simulation_relation.h"


using namespace std;


bool PruningOptions::ignore_current_search_state = false;

bool PruningOptions::reopen_closed_nodes = false;



PruningOptions::PruningOptions(const Options &opts)
                    : pruning_type(PRUNING_TYPE(opts.get_enum("pruning_type"))),
                      hypercube_pruning(opts.get<bool>("hypercube_pruning")),
                      min_price_in_g(opts.get<bool>("include_price_in_g")),
                      g_in_dominance(opts.get<bool>("include_g_in_dominance")),
                      prices_in_dominance(opts.get<bool>("trade_prices_in_dominance")),
                      irrelevance(IRRELEVANCE(opts.get_enum("irrelevance"))),
                      do_simulation(opts.get<bool>("simulation")),
                      goal_price_propagation(opts.get<bool>("goal_price_propagation")) {

    switch (pruning_type){
    case PRUNING_TYPE::DOMINANCE: cout << "using dominance pruning" << endl; break;
    case PRUNING_TYPE::DUPLICATE: cout << "using exact duplicate checking" << endl; break;
    case PRUNING_TYPE::FRONTIER: cout << "using frontier pruning method" << endl; break;
    }                                            
                                            
    if (goal_price_propagation || do_simulation || irrelevance != IRRELEVANCE::NO){
        CompliantPathGraph::set_compute_leaf_backwards_graph();
        
        if (goal_price_propagation){
            cout << "using goal price propagation" << endl;
        }
        
        if (do_simulation){
            cout << "using simulation within leaf factors" << endl;
        }

        if (irrelevance != IRRELEVANCE::NO){
            cout << "performing irrelevance analysis in leaf factors" << endl;
        }
    }
    if (pruning_type == PRUNING_TYPE::FRONTIER &&
            (min_price_in_g ||
                    g_in_dominance ||
                    prices_in_dominance)){
        cout << "WARNING: include_price_in_g, include_g_in_dominance, or trade_prices_in_dominance are not"
                "supported for cost_frontier pruning, disabling these options in favor of frontier pruning." << endl;
        min_price_in_g = false;
        g_in_dominance = false;
        prices_in_dominance = false;
        // TODO is there any reason to have this check here and not in verify_options?
    }
}

void PruningOptions::verify_options() const {
    if (g_factoring->get_profile() != FORK){
        if (pruning_type == PRUNING_TYPE::FRONTIER ||
                goal_price_propagation ||
                do_simulation ||
                irrelevance != IRRELEVANCE::NO){

            cerr << "Advanced pruning methods are only supported for fork factorings!" << endl;
            exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }
    }
    if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
        if (goal_price_propagation){
            cout << "goal price propagation is not (yet) supported for satisficing search" << endl;
            exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }        
        if (do_simulation){
            cout << "simulation is not (yet) supported for satisficing search" << endl;
            exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }
    }
    if (hypercube_pruning){
        if (g_factoring->get_search_type() != SAT && g_factoring->get_search_type() != UNSAT){
            cout << "Hypercube pruning is only supported for satisficing planning." << endl;
            exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }
        if (pruning_type == PRUNING_TYPE::FRONTIER){
            cout << "Hypercube pruning is not supported with frontier pruning." << endl;
            exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }
    }
    if (g_symmetry_graph){
        if (irrelevance != IRRELEVANCE::NO){
            cout << "Irrelevance pruning is not supported for decoupled orbit search!" << endl;
            exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }
        if (pruning_type == PRUNING_TYPE::FRONTIER && 
               (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT)){
            cout << "Satisficing search with frontier pruning is not supported for decoupled orbit search!" << endl;
            exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
        }
    }
}

void PruningOptions::initialize() const {
    verify_options();
    if (do_simulation || irrelevance != IRRELEVANCE::NO){
        SimulationRelation rel(CompliantPathGraph::leaf_state_successors, CompliantPathGraph::leaf_state_predecessors);
        rel.init();
        rel.statistics();
        if (irrelevance != IRRELEVANCE::NO) {
            rel.perform_leaf_irrelevance_pruning(CompliantPathGraph::compute_leaf_backwards_graph, irrelevance == STATES, true);
        }

        if (do_simulation) {
            rel.set_list_dominated_states(); 
        }
    }
}


void PruningOptions::add_options_to_parser(OptionParser &parser){
    parser.add_option<bool>("hypercube_pruning",
                            "perform hypercube pruning",
                            "false");

    parser.add_option<bool>("include_price_in_g",
                            "increase the g-value of decoupled states by the sum of minimum prices",
                            "true");

    parser.add_option<bool>("include_g_in_dominance",
                            "incorporate the difference in g-values of decoupled states into the dominance checking",
                            "true");

    parser.add_option<bool>("trade_prices_in_dominance",
                            "trade prices across leaves in dominance checking",
                            "true");

    parser.add_option<bool>("goal_price_propagation",
                            "perform goal price propagation", "false");
    
    parser.add_option<bool>("simulation",
                            "perform simulation dominance pruning", "false");

    vector<string> irr_options({"NO", "STATES", "TRANSITIONS"});
    parser.add_enum_option("irrelevance",
                           irr_options,
                           "perform simulation irrelevance pruning", "NO");
}

PruningOptions *PruningOptions::_parse_dominance(OptionParser &parser) {
    PruningOptions::add_options_to_parser(parser);
    Options opts = parser.parse();
    
    opts.set<int>("pruning_type", PruningOptions::PRUNING_TYPE::DOMINANCE);

    PruningOptions *pruning = 0;
    if (!parser.dry_run()) {
        pruning = new PruningOptions(opts);
    }

    return pruning;
}

PruningOptions *PruningOptions::_parse_duplicate(OptionParser &parser) {
    PruningOptions::add_options_to_parser(parser);
    Options opts = parser.parse();

    opts.set<int>("pruning_type", PruningOptions::PRUNING_TYPE::DUPLICATE);

    PruningOptions *pruning = 0;
    if (!parser.dry_run()) {
        pruning = new PruningOptions(opts);
    }

    return pruning;
}

PruningOptions *PruningOptions::_parse_frontier(OptionParser &parser) {
    PruningOptions::add_options_to_parser(parser);
    Options opts = parser.parse();
    
    opts.set<int>("pruning_type", PruningOptions::PRUNING_TYPE::FRONTIER);

    PruningOptions *pruning = 0;
    if (!parser.dry_run()) {
        pruning = new PruningOptions(opts);
    }

    return pruning;
}

static Plugin<PruningOptions> _plugin_dominance("dominance", PruningOptions::_parse_dominance);
static Plugin<PruningOptions> _plugin_duplicate("duplicate", PruningOptions::_parse_duplicate);
static Plugin<PruningOptions> _plugin_frontier("cost_frontier", PruningOptions::_parse_frontier);
