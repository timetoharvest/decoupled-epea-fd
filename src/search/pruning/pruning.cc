#include "pruning.h"

#include "../compliant_paths/compliant_path_graph.h"
#include "../factoring.h"
#include "../plugin.h"
#include "simulation_relation.h"
#include "../state_registry.h"


using namespace std;


Pruning::Pruning(const Options &opts) : irrelevance(IRRELEVANCE(opts.get_enum("irrelevance"))),
                                        do_simulation(opts.get<bool>("simulation")),
                                        goal_price_propagation(opts.get<bool>("goal_price_propagation")) {

    if (goal_price_propagation || do_simulation || irrelevance != NO){        
        CompliantPathGraph::set_compute_leaf_backwards_graph();
        
        if (goal_price_propagation){
            CompliantPathGraph::set_propagate_goal_prices();
            cout << "using goal price propagation" << endl;
        }
        
        if (do_simulation){
            cout << "using simulation within leaf factors" << endl;
        }

        if (irrelevance != NO){
            cout << "performing irrelevance analysis in leaf factors" << endl;
        }
    }    
}

void Pruning::init() {
    if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
        if (goal_price_propagation){
            cout << "goal price propagation is not (yet) supported for satisficing search" << endl;
            exit_with(EXIT_INPUT_ERROR);
        }        
        if (do_simulation){
            cout << "simulation is not (yet) supported for satisficing search" << endl;
            exit_with(EXIT_INPUT_ERROR);
        }
    }
    if (do_simulation || irrelevance != NO){
        assert(g_factoring->get_profile() == FORK);

        SimulationRelation rel; 
        rel.init();
        rel.statistics();
        if (irrelevance != NO) {
            rel.perform_leaf_irrelevance_pruning(irrelevance == STATES);
        }

        if (do_simulation) {
            rel.set_list_dominated_states(); 
        }
    }
}

DOMINATION BasicPruning::check_dominance(const State &, CompliantPathGraph *check, CompliantPathGraph *reference, DOMINATION needed) const {
    
    bool sizes_equal = true;
    bool new_is_larger = false;
    bool new_is_smaller = false;
    bool all_leaves_max_size = g_factoring->get_profile() != FORK &&
                               g_factoring->get_search_type() == SAT;
    bool all_goals_reached = g_factoring->get_profile() == FORK &&
                             g_factoring->get_search_type() == SAT;
    
    vector<bool> skip_leaf(g_factors.size(), false);
    
    // TODO fork-leaves that have a goal actually *can* be compared, i.e. if they are identical
    
    // check number of reachable leaf states
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (g_factoring->get_search_type() == SAT && !g_factoring->has_successors(factor)) {
            // in satisficing search, updating PriceTags stops if a goal 
            // is reached in fork leaves. thus cannot compare number of states 
            // in this case
            if (check->get_goal_cost(factor) >= 0){
                if (reference->get_goal_cost(factor) >= 0){
                    // don't care about size/reachability
                } else {
                    all_goals_reached = false;
                    new_is_larger = true;
                    new_is_smaller = true;
                    sizes_equal = false;
                }
                skip_leaf[factor] = true;
                continue;
            } else if (reference->get_goal_cost(factor) >= 0){
                all_goals_reached = false;
                new_is_smaller = true;
                new_is_larger = true;
                sizes_equal = false;
                skip_leaf[factor] = true;
                continue;
            }
            all_goals_reached = false;
        }
        if (reference->get_number_effective_states(factor) > check->get_number_states(factor)){
            new_is_smaller = true;
            sizes_equal = false;
        } else if (reference->get_number_effective_states(factor) < check->get_number_states(factor)){
            new_is_larger = true;
            sizes_equal = false;
        }
        if (check->get_number_states(factor) < g_state_registry->size(factor) || !CompliantPathGraph::precompute_leaf_state_space(factor)){
            all_leaves_max_size = false;
        }
    }
    
    if (new_is_smaller && new_is_larger){
        return NONE;
    } else if (new_is_smaller && needed == DOMINATES){
        return NONE;
    } else if (new_is_larger && needed == DOMINATED){
        return NONE;
    }
    
    if (all_goals_reached){
        // this is only relevant in satisficing fork-decoupled search when the center
        // has not reached its goal yet, but all leaves do.
        return EQUAL;
    }
    
    if (sizes_equal && all_leaves_max_size){
        return EQUAL;
    }
    
    bool dominated = (new_is_smaller && !new_is_larger) || sizes_equal;
    bool dominates = (new_is_larger && !new_is_smaller) || sizes_equal;
    
    // compare prices / reachability
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (skip_leaf[factor]) { // is fork-leaf that reached its goal
            continue;
        }
        size_t number_new_states = check->get_number_effective_states(factor);
        size_t number_old_states = reference->get_number_effective_states(factor);
        size_t id = 0;
        while (number_new_states + number_old_states > 0){
            assert(id < g_state_registry->size(factor));
            // TODO probably introduce some kind of hash to compare leaves
            // against each other in satisficing search
            if (check->has_effective_leaf_state(LeafStateID(id, factor))){
                --number_new_states;
                if (reference->has_effective_leaf_state(LeafStateID(id, factor))){
                    --number_old_states;
                    if (g_factoring->get_search_type() != SAT){
                        int old_cost = reference->get_effective_cost_of_state(LeafStateID(id, factor));
                        int new_cost = check->get_effective_cost_of_state(LeafStateID(id, factor));
                        assert(old_cost != -1);
                        assert(new_cost != -1);
                        
                        if (old_cost > new_cost){
                            dominated = false;
                        } else if (old_cost < new_cost){
                            dominates = false;
                        }
                    }
                } else {
                    dominated = false;
                }                
            } else if (reference->has_effective_leaf_state(LeafStateID(id, factor))){
                --number_old_states;
                dominates = false;
            }
            if (!dominated && !dominates){
                return NONE;
            } else if (!dominated && needed == DOMINATED){
                return NONE;
            } else if (!dominates && needed == DOMINATES){
                return NONE;
            } else if (number_new_states == 0 && number_old_states > 0){
                dominates = false;
                break;
            } else if (number_old_states == 0 && number_new_states > 0){
                dominated = false;
                break;
            }
            ++id;
        }
    }
    if (dominated && dominates){
#ifdef DEBUG_PRUNING
        cout << "old tag is equal to new one" << endl;
#endif
        return EQUAL;
    } else if (dominated){
#ifdef DEBUG_PRUNING
        cout << "new tag is dominated by old one" << endl;
#endif
        return DOMINATED;
    } else if (dominates){
#ifdef DEBUG_PRUNING
        cout << "new tag dominates old one" << endl;
#endif
        return DOMINATES;
    }
    return NONE;
}


void Pruning::add_options_to_parser(OptionParser &parser){
    parser.add_option<bool>("goal_price_propagation",
                            "perform goal price propagation", "false");
    
    parser.add_option<bool>("simulation",
                            "perform simulation dominance pruning", "false");

    vector<string> irr_options({"NO", "STATES", "TRANSITIONS"});
    parser.add_enum_option("irrelevance",
                           irr_options,
                           "perform simulation irrelevance pruning", "NO");
}

BasicPruning::BasicPruning (const Options&opts) : Pruning(opts) {
    cout << "using BasicPruning method" << endl;
}

static Pruning *_parse_basic(OptionParser &parser) {
    Pruning::add_options_to_parser(parser);
    Options opts = parser.parse();

    BasicPruning *pruning = 0;
    if (!parser.dry_run()) {
        pruning = new BasicPruning(opts);
    }

    return pruning;
}

static Plugin<Pruning> _plugin("basic", _parse_basic);

