#include "graph_creator.h"
#include <vector>
#include "../operator.h"
#include "permutation.h"
#include "../timer.h"

using namespace std;

GraphCreator::GraphCreator(const Options &opts) : group(opts) {
    initialized = false;
    time_bound = opts.get<int>("time_bound");
    generators_bound =  opts.get<int>("generators_bound");
}


void GraphCreator::initialize() {
#ifdef USE_BLISS
    if (initialized)
        return;

    initialized = true;
    cout << "Initializing symmetry " << endl;
    group.initialize();

    bliss::AbstractGraph* graph;
    graph = create_bliss_directed_graph();
    ((bliss::Digraph*)graph)->set_splitting_heuristic(bliss::Digraph::shs_flm);

    // Added in version 0.5. Needs to be removed in version 0.72
#ifdef USE_BLISS05
    graph->set_time_bound(time_bound);
    graph->set_generators_bound(generators_bound);
#else
    if (time_bound > 0) {
        cerr << "Warning! Time bound is implemented only in Bliss 0.5" << endl;
        exit_with(EXIT_UNSUPPORTED);
    }
    if (generators_bound > 0) {
        cerr << "Warning! Generators bound is implemented only in Bliss 0.5" << endl;
        exit_with(EXIT_UNSUPPORTED);
    }
#endif

    bliss::Stats stats1;
    cout << "Using Bliss to find group generators" << endl;
    graph->canonical_form(stats1,&(Group::add_permutation),&group);
    cout << "Got " << group.get_num_generators() << " group generators, time step: [t=" << g_timer << "]" << endl;

    group.default_direct_product();
    cout<<"Number of generators: "<< group.get_num_generators()<<endl;

    // Deleting the graph
    delete graph;

    cout << "Done initializing symmetries [t=" << g_timer << "]" << endl;
#else
    cerr << "You must build the planner with the USE_BLISS symbol defined" << endl;
    exit_with(EXIT_CRITICAL_ERROR);
#endif
}



GraphCreator::~GraphCreator() {
    // Freeing the group
    free_memory();
}

#ifdef USE_BLISS
bliss::Digraph* GraphCreator::create_bliss_directed_graph() const {
    // Differ from create_bliss_graph() in (a) having one node per action (incoming arcs from pre, outgoing to eff),
    //                                 and (b) not having a node for goal, recoloring the respective values.
   // initialization

    int num_of_vertex = g_variable_domain.size();
    for (int num_of_variable = 0; num_of_variable < g_variable_domain.size(); num_of_variable++){
        Permutation::dom_sum_by_var.push_back(num_of_vertex);
        num_of_vertex+=g_variable_domain[num_of_variable];
        for(int num_of_value = 0; num_of_value < g_variable_domain[num_of_variable]; num_of_value++){
            Permutation::var_by_val.push_back(num_of_variable);
        }
    }

    Permutation::length = num_of_vertex;

    bliss::Digraph* g = new bliss::Digraph();
    int idx = 0;
    // add vertex for each varaible
    for (int i = 0; i < g_variable_domain.size(); i++){
       idx = g->add_vertex(PREDICATE_VERTEX);
    }
    // now add values vertices for each predicate
    for (int i = 0; i < g_variable_domain.size(); i++){
       for (int j = 0; j < g_variable_domain[i]; j++){
          idx = g->add_vertex(VALUE_VERTEX);
          g->add_edge(idx,i);
       }
    }

    // now add vertices for operators
    for (int i = 0; i < g_operators.size(); i++){
        const Operator& op = g_operators[i];
        add_operator_directed_graph(g, op);
    }

    // now add vertices for axioms
    for (int i = 0; i < g_axioms.size(); i++){
        const Operator& op = g_axioms[i];
        add_operator_directed_graph(g, op);
    }

    // Recoloring the goal values
    for (int i = 0; i < g_goal.size(); i++){
        int var = g_goal[i].first;
        int val = g_goal[i].second;
        int goal_idx = Permutation::dom_sum_by_var[var] + val;
        g->change_color(goal_idx, GOAL_VERTEX);
    }

    return g;
}

//TODO: Use separate color for axioms
//TODO: Change the order of vertices creation to support keeping actions in the permutation (no need for keeping conditional effect vertices).
void GraphCreator::add_operator_directed_graph(bliss::Digraph* g, const Operator& op) const {
    int OP_COLOR = MAX_VALUE + op.get_cost();

    int op_idx = g->add_vertex(OP_COLOR);

    const std::vector<Prevail>& prevail = op.get_prevail();
    for (int idx1 = 0; idx1 < prevail.size(); idx1++){
        int var = prevail[idx1].var;
        int val = prevail[idx1].prev;
        int prv_idx = Permutation::dom_sum_by_var[var] + val;
        g->add_edge(prv_idx, op_idx);
    }
    const std::vector<PrePost>& prepost = op.get_pre_post();
    for (int idx1 = 0; idx1 < prepost.size(); idx1++){
        int var = prepost[idx1].var;
        int pre_val = prepost[idx1].pre;

        if (pre_val!= -1){
            int pre_idx = Permutation::dom_sum_by_var[var] + pre_val;
            g->add_edge(pre_idx, op_idx);
        }

        //TODO: Add support for conditional effects
        int eff_val = prepost[idx1].post;
        int eff_idx = Permutation::dom_sum_by_var[var] + eff_val;

        if (prepost[idx1].cond.size() == 0) {
            g->add_edge(op_idx, eff_idx);
        } else {
//            	cout << "Adding a node for conditional effect" << endl;
            // Adding a node for each condition. An edge from op to node, an edge from node to eff,
            // for each cond, an edge from cond to node.
            color_t effect_color = CONDITIONAL_EFFECT_VERTEX;
            if (effect_can_be_overwritten(idx1, prepost)) {
                effect_color = CONDITIONAL_DELETE_EFFECT_VERTEX;
            }
            int cond_op_idx = g->add_vertex(effect_color);
            g->add_edge(op_idx, cond_op_idx); // Edge from operator to conditional effect
            g->add_edge(cond_op_idx, eff_idx); // Edge from conditional effect to effect
            // Adding edges for conds
            for (int c = 0; c < prepost[idx1].cond.size(); c++){
                int c_var = prepost[idx1].cond[c].var;
                int c_val = prepost[idx1].cond[c].prev;
                int c_idx = Permutation::dom_sum_by_var[c_var] + c_val;

                g->add_edge(c_idx, cond_op_idx); // Edge from condition to conditional effect
            }
        }
    }

}

bool GraphCreator::effect_can_be_overwritten(int ind, const std::vector<PrePost>& prepost) const {
    // Checking whether the effect is a delete effect that can be overwritten by an add effect
    assert(ind < prepost.size());
    int var = prepost[ind].var;
    int eff_val = prepost[ind].post;
    if (eff_val != g_variable_domain[var] - 1) // the value is not none_of_those
        return false;

    // Go over the next effects of the same variable, skipping the none_of_those
    for (int i=ind+1; i < prepost.size(); i++) {
        if (var != prepost[i].var) // Next variable
            return false;
        if (prepost[i].post == g_variable_domain[var] - 1)
            continue;
        // Found effect on the same variable which is not none_of_those
        return true;
    }
    return false;
}
#endif


Permutation GraphCreator::create_permutation_from_state_to_state(const State& from_state, const State& to_state) {
    Trace new_trace;
    Trace curr_trace;
    get_trace(from_state, curr_trace);
    get_trace(to_state, new_trace);

    Permutation p1(create_permutation_from_trace(new_trace), true);  //inverse
    Permutation p2 = create_permutation_from_trace(curr_trace);
    return  Permutation(p2, p1);
}


void GraphCreator::add_options_to_parser(OptionParser &parser) {
    parser.add_option<int>("stop_after_false_generated",
                           "Stopping after the Bliss software generated too many false generators",
                           OptionParser::to_str(numeric_limits<int>::max()));
    parser.add_option<int>("time_bound",
                           "Stopping after the Bliss software reached the time bound",
                           "0");
    parser.add_option<int>("generators_bound",
                           "Stopping after the Bliss software reached the bound on the number of generators",
                           "0");
}


static GraphCreator *_parse(OptionParser &parser) {
    GraphCreator::add_options_to_parser(parser);
    Options opts = parser.parse();

    if (!parser.dry_run()) {
        cout << "Creating symmetry graph stabilizing goal only and using ";
#ifdef CANONICAL
        cout << "regular ";
#else
        cout << "orbit ";
#endif
        cout << "search" << endl;
        return new GraphCreator(opts);
    } else {
        return 0;
    }
}

static Plugin<GraphCreator> _plugin("symmetry_state_pruning", _parse);
