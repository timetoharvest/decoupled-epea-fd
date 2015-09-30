#include "factoring.h" 

#include "causal_graph.h"
#include "globals.h"
#include "operator.h"
#include "option_parser.h"
#include "plugin.h"
#include "pruning.h"
#include "../preprocess/scc.h"
#include "successor_generator.h"
#include "timer.h"
#include "utilities.h"
#include "price_tag.h"

#include <iostream>
#include <limits>
#include <map>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <time.h>


using namespace std;

// TODO add a command line option for abstaining


void InteractionGraph::add_dependency(int from, int to) {
    if (find(successors[from+1].begin(), successors[from+1].end(), to) == successors[from+1].end()){
        successors[from+1].push_back(to);
    }
    if (find(predecessors[to+1].begin(), predecessors[to+1].end(), from) == predecessors[to+1].end()){
        predecessors[to+1].push_back(from);
    }
}

Factoring::Factoring(const Options &opts) :
            reopen_closed_nodes(false),
            search_type(SEARCH_TYPE(opts.get_enum("search_type"))) {
    switch(search_type){
        case FRA : cout << "performing Fork-Root A* search" << endl; break;
        case ASDA : cout << "performing Anytime Star Decoupled A* search" << endl; break;
        case SDA : cout << "performing Star Decoupled A* search" << endl; break;
        case SAT : cout << "performing decoupled satisficing search" << endl; break;
    }
    if (opts.contains("max_leaf_size")){
        max_leaf_size = opts.get<int>("max_leaf_size");
        if (max_leaf_size == 0){
            cout << "no bound on leaf factor size" << endl;
            // TODO probably change command line to -1 => no bound
        } else {
            cout << "maximum leaf factor size " << max_leaf_size << endl;
        }
    } else {
        max_leaf_size = numeric_limits<size_t>::max();
    }
    if (opts.contains("pruning")){
        pruning = opts.get<Pruning*>("pruning");
    } else {
        pruning = new BasicPruning();
        cout << "using default pruning method: BasicPruning" << endl;
    }
    min_number_leaves = 2;
}

class CenterFactoring : public Factoring {
    
public:
    
    CenterFactoring(const Options &opts)
            : Factoring(opts) {
        profile = CENTER;
        cout << "using random 2-factor factoring profile" << endl;
    };
  
    virtual FactoredVars get_factoring();
    
};

class ForkFactoring : public Factoring {
    
protected:
    
    bool inverted;
    
    bool maximize_mobility;
    
public:
    
    ForkFactoring(const Options &opts, bool inverted = false)
            : Factoring(opts), inverted(inverted),
              maximize_mobility(opts.get<bool>("maximize_mobility")) {
        if (inverted){
            profile = I_FORK;
            cout << "using inverted-fork factoring profile" << endl;
        } else {
            profile = FORK;
            cout << "using fork factoring profile" << endl;
        }
    };
    
    virtual FactoredVars get_factoring();
    
    static void add_options_to_parser(OptionParser &parser);
    
};

class XShapeFactoring : public Factoring {
    
    bool maximize_mobility;
    
public:
    
    XShapeFactoring(const Options &opts)
            : Factoring(opts), maximize_mobility(opts.get<bool>("maximize_mobility")) {
        profile = X_SHAPE;
        cout << "using X-shape factoring profile" << endl;
        min_number_leaves = 1;
    };
    
    virtual FactoredVars get_factoring();
    
};

class IncidentArcsFactoring : public Factoring {
  
public:
    
    IncidentArcsFactoring(const Options &opts)
            : Factoring(opts) {
        profile = INC_ARCS;
        cout << "using incident-arcs factoring profile" << endl;
    };
    
    virtual FactoredVars get_factoring();
  
};

class SokobanFactoring : public Factoring {
  
public:
    
    SokobanFactoring(const Options &opts)
            : Factoring(opts) {
        profile = SOKOBAN;
        cout << "using sokoban factoring profile" << endl;
    };
    
    virtual FactoredVars get_factoring();
  
};

class OpenstacksFactoring : public Factoring {
  
public:
    
    OpenstacksFactoring(const Options &opts)
            : Factoring(opts) {
        profile = OPENSTACKS;
        cout << "using openstacks factoring profile" << endl;
    };
    
    virtual FactoredVars get_factoring();
  
};

class TPPConstrainedFactoring : public Factoring {
    
    bool inverted;
  
public:
    
    TPPConstrainedFactoring(const Options &opts)
            : Factoring(opts), inverted(opts.get<bool>("inverted")) {
        profile = OPENSTACKS;
        cout << "using " <<  (inverted ? "inverted-" : "") << "tpp factoring profile" << endl;
    };
    
    virtual FactoredVars get_factoring();
  
};



void Factoring::apply_factoring(FactoredVars factoring) {
    g_belongs_to_factor = vector<int>(g_variable_domain.size(), -1);        // -1 => center factor
    g_new_index = vector<int>(g_variable_domain.size());
    interaction_graph = InteractionGraph(factoring.size());
    g_factors.resize(factoring.size());
    
    for (size_t i = 0; i < factoring.size(); i++){
        vector<int> variables(factoring[i].begin(), factoring[i].end());              
        for (size_t j = 0; j < variables.size(); ++j){
            g_belongs_to_factor[variables[j]] = i;
            g_new_index[variables[j]] = j;
        }
        g_factors[i] = variables;
    }
    
#ifdef DEBUG_FACTORING
    cout << "center factor:" << endl;
#endif
    
    for (size_t i = 0; i < g_variable_domain.size(); ++i){
        int affected_factor = g_belongs_to_factor[i];
        if (affected_factor == -1){
            g_center_vars.push_back(i);
            g_new_index[i] = g_center_vars.size() - 1;
#ifdef DEBUG_FACTORING
            cout << "   variable " << i << " :" << g_fact_names[i][0] << endl;
#endif
        }
    }
   
    // assign operators to their factors
#ifdef DEBUG_FACTORING
    vector<int> num_ops_per_factor(g_factors.size());
#endif
    bool is_fork_factoring = true;
    for (size_t i = 0; i < g_operators.size(); ++i){
        Operator &op = g_operators[i];
        
        set<int> pre_factors;
        set<int> eff_factors;

        for (size_t prev = 0; prev < op.get_preconditions().size(); ++prev){
            const Condition &prevail = op.get_preconditions()[prev];
            pre_factors.insert(g_belongs_to_factor[prevail.var]);
        }
        
        for (size_t post = 0; post < op.get_effects().size(); ++post){
            const Effect &prepost = op.get_effects()[post];
            eff_factors.insert(g_belongs_to_factor[prepost.var]);
        }
        
        for (set<int>::iterator pre_it = pre_factors.begin(); pre_it != pre_factors.end(); ++pre_it){
            for (set<int>::iterator eff_it = eff_factors.begin(); eff_it != eff_factors.end(); ++eff_it){
                if (*pre_it != *eff_it){
                    interaction_graph.add_dependency(*pre_it, *eff_it);
                }
            }
        }
        for (set<int>::iterator eff_it1 = eff_factors.begin(); eff_it1 != eff_factors.end(); ++eff_it1){
            for (set<int>::iterator eff_it2 = eff_factors.begin(); eff_it2 != eff_factors.end(); ++eff_it2){
                if (*eff_it1 != *eff_it2){
                    interaction_graph.add_dependency(*eff_it1, *eff_it2);
                }
            }
        }
        if (eff_factors.find(-1) == eff_factors.end()){
            assert(eff_factors.size() == 1);
            int factor = *eff_factors.begin();
#ifdef DEBUG_FACTORING
            num_ops_per_factor[factor]++;
#endif
            op.set_affected_factor(factor);
        } else {
            pre_factors.erase(-1);
            if (eff_factors.size() > 1 || !pre_factors.empty()){
                is_fork_factoring = false;
            }
            op.set_affected_factor(-1);
        }
    }

    if (is_fork_factoring && profile != FORK){
        // no leaf precondition on center => fork factoring
        profile = FORK;
        cout << "factoring type set to fork, no non-fork leaf factors" << endl;
    }
    
#ifdef DEBUG_FACTORING
    for (size_t i = 0; i < num_ops_per_factor.size(); i++) {
        cout << num_ops_per_factor[i] << " operators used in RPG of factor " << i << endl;
    }
#endif

    
    g_leaf_state_successors.resize(g_factors.size());
    
    g_center_successors.resize(g_factors.size());
    
    g_is_leaf_goal_state.resize(g_factors.size());
    
    PriceTag::initialize_leaf_cache();
    
    // setup g_goals_per_factor
    // TODO check if each factor contains a goal! what to do if not??
    // this should probably be assured by the get_factoring methods?!
    g_goals_per_factor = vector<vector<pair<int, int> > >(g_factors.size());
    vector<int> indeces_to_remove;
    for (size_t g = 0; g < g_goal.size(); ++g){
        int affected_factor = g_belongs_to_factor[g_goal[g].first];
        if (affected_factor != -1){
            g_goals_per_factor[affected_factor].push_back(g_goal[g]);
            indeces_to_remove.push_back(g);
#ifdef DEBUG_FACTORING
            cout << "added fact to goal of factor " << affected_factor << endl;
            cout << g_fact_names[g_goal[g].first][g_goal[g].second] << endl;
#endif
        }
    }
    vector<pair<int, int> >::iterator it = g_goal.begin();
    for (int i = indeces_to_remove.size() - 1; i >= 0; --i){
        g_goal.erase(it + indeces_to_remove[i]);
    }
#ifdef DEBUG_FACTORING
    cout << "center goals:" << endl;
    for (size_t i = 0; i < g_goal.size(); ++i){
        cout << g_fact_names[g_goal[i].first][g_goal[i].second] << endl;
    }
#endif
}

void Factoring::print_factoring() const {
    cout << "number leaf factors: " << g_factors.size() << endl;
    
    cout << g_center_vars.size() << " variables in center factor" << endl;
    
    for (size_t i = 0; i < g_factors.size(); i++) {
        cout << g_factors[i].size() << " variables in leaf factor " << i << endl;
    }
    
    uint64_t min_size = numeric_limits<uint64_t>::max();
    double avg_size = 0;
    uint64_t max_size = 0;
    for (size_t i = 0; i < g_factors.size(); i++){
        uint64_t f_size = 1;
        for (size_t j = 0; j < g_factors[i].size(); j++){
            f_size *= g_variable_domain[g_factors[i][j]];
        }
        avg_size += f_size;
        min_size = min(min_size, f_size);
        max_size = max(max_size, f_size);
    }
    uint64_t avg = 0;
    if (g_factors.size() > 0){
        avg = avg_size / g_factors.size();
    } else {
        min_size = 0;
    }
    
    cout << "min leaf factor size: " << min_size << endl;
    cout << "average leaf factor size: " << avg << endl;
    cout << "max leaf factor size: " << max_size << endl;
}

void Factoring::exit_error(string message, int exit_code) const {
    cout << "Error while factoring planning task!" << endl;
    cout << message << endl;
    exit(exit_code);
}

void Factoring::do_factoring_or_abstain() {

    // for calculating the factoring time
    Timer t;
    
    FactoredVars best_factoring = get_factoring();
    
    cout << "factoring time " << t << endl;
    
    if (best_factoring.empty()){
        exit_error("No factoring found!", EXIT_UNSOLVED_INCOMPLETE);
    }
    
    apply_factoring(best_factoring);    
    
    print_factoring();
}

vector<set<int> > XShapeFactoring::get_factoring() {    ///////////////////////////  X-SHAPE
    
    size_t min_number_x_leaves = 2;
    
    FactoredVars best_factoring;
    if (maximize_mobility){
        best_factoring = get_fork_maximize_mobility();
    } else {
        best_factoring = get_fork_maximize_leaves();
    }
    
    vector<int> variables = vector<int>();
    
    for (size_t var = 0; var < g_variable_domain.size(); var++){
        bool is_leaf = false;
        for (size_t i = 0; i < best_factoring.size(); i++){
            if (best_factoring[i].find(var) != best_factoring[i].end()){
                is_leaf = true;
                break;
            }
        }
        if (!is_leaf){
            variables.push_back(var);
        }
    }
    
    cout << "found " << best_factoring.size() << " leaves" << endl;
    
    FactoredVars inv_fork;
    
    if (variables.size() > 0){
        // this happens e.g. in Movie, where the CG is not connected
        if (maximize_mobility){
            inv_fork = get_fork_maximize_mobility(variables, true);
        } else {
            inv_fork = get_fork_maximize_leaves(variables, true);    
        }
    }
                
    int added = 0;
    
    // eliminate leaf -> leaf connections
    for (size_t i = 0; i < inv_fork.size(); ++i){
        bool is_connected = false;
        for (set<int>::iterator var = inv_fork[i].begin(); var != inv_fork[i].end(); ++var){
            vector<int> successors = g_causal_graph->get_successors(*var);
            for (size_t succ = 0; succ < successors.size(); succ++){
                for (size_t j = 0; j < best_factoring.size(); j++){
                    if (best_factoring[j].find(successors[succ]) != best_factoring[j].end()){
                        is_connected = true;
                    }
                }
                if (is_connected){
                    break;
                }
            }
            if (is_connected){
                break;
            }
        }
        if (!is_connected){
            best_factoring.push_back(inv_fork[i]);
            added++;
        }
    }
    
    cout << "added " << added << " roots" << endl;
    
    if (best_factoring.size() < min_number_x_leaves){
        
        if (maximize_mobility){
            best_factoring = get_fork_maximize_mobility(vector<int>(), true);
        } else {
            best_factoring = get_fork_maximize_leaves(vector<int>(), true);
        }
        
        cout << "found inv-fork factoring with " << best_factoring.size() << " leaves" << endl;
        
        if (best_factoring.size() < min_number_x_leaves){
            return FactoredVars();
        }
    }
    
    return best_factoring;
}

vector<set<int> > CenterFactoring::get_factoring() { //////////////////////////////  CENTER
    // TODO implement factoring
    FactoredVars best_factoring = vector<set<int> >(1);

    srand(time(NULL));
    
    uint64_t current_size = 1;
    
    for (size_t var = 0; var < g_variable_domain.size(); var++){
        if (rand() % 2 == 0){
            best_factoring[0].insert(var);
	    current_size *= g_variable_domain[var];
        }
        if (current_size > max_leaf_size){
            best_factoring[0].erase(var);
	    break;
	}
    }
    
    if (best_factoring[0].size() == g_variable_domain.size()){
        best_factoring[0].erase(rand() % g_variable_name.size());
    }
    if (best_factoring[0].size() == 0){
        best_factoring[0].insert(rand() % g_variable_name.size());
    }
    
        
//     best_factoring[0].insert(0);
//     best_factoring[0].insert(1);
//     best_factoring[0].insert(2);    
//     best_factoring[0].insert(3);
//     best_factoring[0].insert(4);
//     best_factoring[0].insert(5);
//     best_factoring[0].insert(6);
//     best_factoring[0].insert(7);
//     best_factoring[0].insert(8);
//     best_factoring[0].insert(9);
//     best_factoring[0].insert(10);
//     best_factoring[0].insert(11);
//     best_factoring[0].insert(12);
//     best_factoring[0].insert(13);
//     best_factoring[0].insert(14);
//     best_factoring[0].insert(15);
//     best_factoring[0].insert(16);
//     best_factoring[0].insert(17);
//     best_factoring[0].insert(18);
//     best_factoring[0].insert(19);
//     best_factoring[0].insert(20);
//     best_factoring[0].insert(21);
//     best_factoring[0].insert(22);
//     best_factoring[0].insert(23);
//      best_factoring[0].insert(24);
//     best_factoring[0].insert(25);
//     best_factoring[0].insert(26);
//     best_factoring[0].insert(27);
//     best_factoring[0].insert(28);
//     best_factoring[0].insert(29);
//     best_factoring[0].insert(30);
//     best_factoring[0].insert(31);
    
    cout << endl;
    for (size_t var = 0; var < g_variable_domain.size(); var++){
        bool is_leaf = false;
        for (size_t l = 0; l < best_factoring.size(); l++){
            if (best_factoring[l].find(var) != best_factoring[l].end()){
                cout << "leaf " << l << ":    " << g_fact_names[var][0] << endl;
                is_leaf = true;
            }
        }
        if (!is_leaf){
            cout << "center:    " << g_fact_names[var][0] << endl;
        }
    }
    cout << endl;
        
    return best_factoring;
}

vector<set<int> > ForkFactoring::get_factoring() { ////////////////////////////////  (INVERTED) FORK
    
    FactoredVars factoring;
    if (maximize_mobility){
        factoring = get_fork_maximize_mobility(vector<int>(), inverted);
    } else {
        factoring = get_fork_maximize_leaves(vector<int>(), inverted);
    }

    if (factoring.size() < min_number_leaves){
        return FactoredVars();
    }

    return factoring;
}

vector<set<int> > IncidentArcsFactoring::get_factoring() {  ///////////////////// INCIDENT ARCS
    
    // TODO factoring has some bugs in several tasks,
    // e.g. scanalyzer-08-strips, p22-25; rover p16/19
    // takes ages to find solution e.g. in zenotravel pfile6 when not
    // using lmcut
    
    // all variables sorted descendingly by number of incident arcs
    vector<vector<int> > variables;
    
    for (size_t var = 0; var < g_variable_domain.size(); ++var){
        size_t arcs = g_causal_graph->get_successors(var).size();
        arcs += g_causal_graph->get_predecessors(var).size();
        if (arcs >= variables.size()){
            variables.resize(arcs + 1);
        }
        variables[arcs].push_back(var);
    }
    
    FactoredVars best_factoring;
    int best_number_leaves = 0;
    
    vector<bool> discarded_leaves;
    
    set<int> leaf_vars;
    for (size_t var = 0; var < g_variable_domain.size(); ++var){
        leaf_vars.insert(var);
    }
    
    for (int arcs = variables.size() -1 ; arcs >= 0; --arcs){
        for (size_t cand = 0; cand < variables[arcs].size(); cand++){
            //center.push_back(variables[arcs][cand]);
            
            leaf_vars.erase(variables[arcs][cand]);            
            
            FactoredVars factoring = get_connected_components(vector<int>(leaf_vars.begin(), leaf_vars.end()));
            
            vector<bool> discard_leaf = get_frozen_leaves(factoring);
            
            int number_leaves = factoring.size();
            for (size_t i = 0; i < factoring.size(); ++i){
                if (discard_leaf[i]){
                    number_leaves--;
                    continue;
                }
                uint64_t size = 1;
                for (set<int>::iterator it = factoring[i].begin(); it != factoring[i].end(); ++it){
                    size *= g_variable_domain[*it];
                    if (max_leaf_size != 0 && size > max_leaf_size){
                        break;
                    }
                }
                if (max_leaf_size != 0 && size > max_leaf_size){
                    --number_leaves;
                    discard_leaf[i] = true;
                }
            }
            
            if (number_leaves >= best_number_leaves){
                best_factoring = factoring;
                discarded_leaves = discard_leaf;
                best_number_leaves = number_leaves;
            }
        }
    }
    
    bool change = true;         // put frozen leaves back to center
    int ind = best_factoring.size();
    while (change){
        change = false;
        for (int i = ind - 1; i >= 0; --i){
            if (discarded_leaves[i]){
                best_factoring.erase(best_factoring.begin() + i);
                
                change = true;
                ind = i;
                break;
            }
        }
    }
    
    if (best_factoring.size() < min_number_leaves){
        return FactoredVars();
    }
    
#ifdef DEBUG_FACTORING
    cout << endl;                      // output factoring
    for (size_t l = 0; l < best_factoring.size(); ++l){
        cout << "leaf factor " << l << ":" << endl;
        for (set<int>::iterator it = best_factoring[l].begin(); it != best_factoring[l].end(); ++it){
            cout << "   " << g_fact_names[*it][0] << endl;
        }        
    }
    cout << endl;
#endif
    
    return best_factoring;
}

Factoring::FactoredVars Factoring::get_fork_maximize_leaves(vector<int> used_vars, bool inverted) const {             ///////////////////////// HELPER FUNCTIONS
    
    FactoredVars sccs = get_sccs(used_vars);
    
    FactoredVars best_factoring;

    if (sccs.size() == 1){
#ifdef DEBUG_FACTORING
        cout << "CG is a single SCC" << endl;
#endif
        return best_factoring;
    }
    
    for (size_t i = 0; i < sccs.size(); ++i){
        uint64_t current_size = 1;
        bool is_leaf = true;
        
        for (set<int>::iterator it = sccs[i].begin(); it != sccs[i].end(); ++it){
            int var = *it;
            if (current_size <= max_leaf_size){
                current_size *= g_variable_domain[var];
            }

            size_t bound = g_causal_graph->get_successors(var).size();
            if (inverted){
                bound = g_causal_graph->get_predecessors(var).size();
            }
            for (size_t succ = 0; succ < bound; ++succ){
                int succ_pred;
                if (inverted){
                    succ_pred = g_causal_graph->get_predecessors(var)[succ];
                } else {
                    succ_pred = g_causal_graph->get_successors(var)[succ];
                }

                if (used_vars.empty() || used_vars.size() == g_variable_domain.size() 
                    || find(used_vars.begin(), used_vars.end(), succ_pred) != used_vars.end()){
                    if (sccs[i].find(succ_pred) == sccs[i].end()){
                        // SCC has successor/predecessor SCC
                        is_leaf = false;
                    }
                }
            }
        }

#ifdef DEBUG_FACTORING
        cout << "=========      SCC with " << sccs[i].size() << " variables" << endl;
        for (set<int>::iterator it = sccs[i].begin(); it != sccs[i].end(); ++it){
            cout << "   " << g_fact_names[*it][0] << endl;
        }
        cout << "=========      end SCC; has size " << current_size << endl;
#endif
        if (is_leaf && (max_leaf_size == 0 || current_size <= max_leaf_size)){
            best_factoring.push_back(sccs[i]);
        }
    }
    return best_factoring;
}

Factoring::FactoredVars Factoring::get_fork_maximize_mobility(vector<int> used_vars, bool inverted) const {
    // perform the actual factoring
    
    typedef multimap<uint64_t, pair<vector<int>, set<int> > > CandSCCs;
    
#ifdef DEBUG_FACTORING
    cout << "maximizing leaf mobility" << endl;
#endif
    
    FactoredVars sccs = get_sccs(used_vars);
    
    FactoredVars best_factoring;

    if (sccs.size() == 1){
#ifdef DEBUG_FACTORING
    cout << "CG is a single SCC" << endl;
#endif
        return best_factoring;
    }
    
    vector<size_t> mobility(sccs.size(), 0);
    
    for (size_t i = 0; i < g_operators.size(); ++i){
        // only need to consider first effect, because each action
        // can only affect a single SCC, otherwise, eff-eff cycles between
        // another SCC => contradiction
        int var = g_operators[i].get_effects()[0].var;
        for (size_t j = 0; j < sccs.size(); ++j){
            if (sccs[j].find(var) != sccs[j].end()){
                ++mobility[j];
            }
        }
    }
    
    vector<int> vars_to_sccs(g_variable_domain.size());
    
    bool all_sscs_fit_bound = true;
    
    uint64_t best_mobility = 1;
    
    uint64_t current_size = 1;
    
    // contains all leaf SCCs sorted by size
    CandSCCs canditate_sccs;

    for (size_t i = 0; i < sccs.size(); ++i){
        assert(mobility[i] > 0);
#ifdef DEBUG_FACTORING
        cout << "mobility of SCC " << i << " = " << mobility[i] << endl;
#endif
        
        set<int> succ_pred_sccs;
        current_size = 1;
        bool is_leaf = true;
        
        for (set<int>::iterator it = sccs[i].begin(); it != sccs[i].end(); ++it){
            int var = *it;
            vars_to_sccs[var] = i;
            if (current_size <= max_leaf_size){
                current_size *= g_variable_domain[var];
            }

            size_t bound = g_causal_graph->get_successors(var).size();
            if (inverted){
                bound = g_causal_graph->get_predecessors(var).size();
            }
            for (size_t succ = 0; succ < bound; ++succ){
                int succ_pred;
                if (inverted){
                    succ_pred = g_causal_graph->get_predecessors(var)[succ];
                } else {
                    succ_pred = g_causal_graph->get_successors(var)[succ];
                }

                if (used_vars.empty() || used_vars.size() == g_variable_domain.size() 
                    || find(used_vars.begin(), used_vars.end(), succ_pred) != used_vars.end()){
                    if (sccs[i].find(succ_pred) == sccs[i].end()){
                        // SCC has successor/predecessor SCC
                        succ_pred_sccs.insert(-1);
                        is_leaf = false;
                    }
                }
            }
        }

#ifdef DEBUG_FACTORING
        cout << "=========      SCC with " << sccs[i].size() << " variables" << endl;
        for (set<int>::iterator it = sccs[i].begin(); it != sccs[i].end(); ++it){
            cout << "   " << g_fact_names[*it][0] << endl;
        }
        cout << "=========      end SCC; has size " << current_size << endl;
#endif
        if (max_leaf_size == 0 || current_size <= max_leaf_size){
            if (is_leaf){
                best_factoring.push_back(sccs[i]);
                best_mobility *= mobility[i];
            } else {
                canditate_sccs.insert(make_pair(current_size, make_pair(vector<int>(sccs[i].begin(), sccs[i].end()), succ_pred_sccs)));
            }
        } else {
            all_sscs_fit_bound = false;
#ifdef DEBUG_FACTORING
            cout << " => too large" << endl;
#endif
        }
    }
    
#ifdef DEBUG_FACTORING
    cout << "number leaf factors with leaf-SCCs only: " << best_factoring.size() << " mobility = " << best_mobility << endl;
    cout << "number canditate sccs: " << canditate_sccs.size() << endl;
#endif
    
    FactoredVars current_factoring = best_factoring;
    
    bool change = !all_sscs_fit_bound || canditate_sccs.size() > 1;
    while (change){

        change = false;
        
        // update interaction graph of remaining SCCs
        for (CandSCCs::iterator it = canditate_sccs.begin(); it != canditate_sccs.end(); ++it){
            set<int> &succ_pred_facts = it->second.second;
            succ_pred_facts.clear();
            
            const vector<int> &vars = it->second.first;
            
            for (size_t i = 0; i < vars.size(); ++i){
                bool found = true;
                size_t bound = g_causal_graph->get_successors(vars[i]).size();
                if (inverted){
                    bound = g_causal_graph->get_predecessors(vars[i]).size();
                }
                for (size_t var = 0; var < bound; ++var){
                    found = false;
                    int succ_pred;
                    if (inverted){
                        succ_pred = g_causal_graph->get_predecessors(vars[i])[var];
                    } else {
                        succ_pred = g_causal_graph->get_successors(vars[i])[var];
                    }
                    if (used_vars.empty() || used_vars.size() == g_variable_domain.size() 
                        || find(used_vars.begin(), used_vars.end(), succ_pred) != used_vars.end()){
                        if (find(vars.begin(), vars.end(), succ_pred) != vars.end()){
                            found = true;
                            continue;
                        }
                        for (size_t j = 0; j < current_factoring.size(); ++j){
                            if (current_factoring[j].find(succ_pred) != current_factoring[j].end()){
                                succ_pred_facts.insert(j);
                                found = true; 
                                break;
                            }
                        }
                        if (!found){
                            break;
                        }
                    }
                }
                if (!found){
                    succ_pred_facts.insert(-1);
                    break;
                }
            }
        }
        
        
        // find best SCC to add to leaves
        CandSCCs::iterator best_successor_scc;
        uint64_t successor_mobility = 0;

        for (CandSCCs::iterator it = canditate_sccs.begin(); it != canditate_sccs.end(); ++it){
            bool can_add = true;
            
            set<int> &succ_pred_sccs = it->second.second;
            
#ifdef DEBUG_FACTORING
            const vector<int> &variables = it->second.first;
            cout << "current SCC: " << variables.size() << " vars, size = " << it->first << endl;
            for (vector<int>::const_iterator d_it = variables.begin(); d_it != variables.end(); ++d_it){
                cout << "       " << g_fact_names[*d_it][0] << endl;
            }
#endif

            if (succ_pred_sccs.find(-1) != succ_pred_sccs.end()){
#ifdef DEBUG_FACTORING
                cout << "has non leaf-factor successors => drop for the moment" << endl;
#endif
                can_add = false;
            } else {
#ifdef DEBUG_FACTORING
                cout << "all successors are leaf factors => combining to new leaf factor" << endl;
#endif
                current_size = it->first;
                
                for (set<int>::iterator succ_it = succ_pred_sccs.begin(); succ_it != succ_pred_sccs.end(); ++succ_it){
                    const set<int> &successor = current_factoring[*succ_it];
                    for (set<int>::iterator succ_pred_vars = successor.begin(); succ_pred_vars != successor.end(); ++succ_pred_vars){
                        current_size *= g_variable_domain[*succ_pred_vars];
                        if (max_leaf_size != 0 && current_size > max_leaf_size){
                            break;
                        }
                    }
                    if (max_leaf_size != 0 && current_size > max_leaf_size){
                        break;
                    }
                }
                
                if (max_leaf_size != 0 && current_size > max_leaf_size){
                    can_add = false;
#ifdef DEBUG_FACTORING
                    cout << "resulting leaf factor would be too large" << endl;
#endif
                }
            }
            
            if (can_add){
                // reconstruct of which SCCs the current leaf factors consist
                uint64_t new_mobility = 1;
                vector<set<int> > sccs(current_factoring.size() + 1);
                for (size_t i = 0; i < current_factoring.size(); ++i){
                    for (set<int>::iterator v_it = current_factoring[i].begin(); v_it != current_factoring[i].end(); ++v_it){
                        sccs[i].insert(vars_to_sccs[*v_it]);
                    }
                }
                const set<int> &succ_pred_sccs = it->second.second;
                set<int> factors_to_remove;
                for (set<int>::iterator succ_it = succ_pred_sccs.begin(); succ_it != succ_pred_sccs.end(); ++succ_it){
                    const set<int> &successors = current_factoring[*succ_it];
                    for (set<int>::iterator scc_it = successors.begin(); scc_it != successors.end(); ++scc_it){
                        sccs.back().insert(vars_to_sccs[*scc_it]);
                    }
                    factors_to_remove.insert(*succ_it);
                }
                sccs.back().insert(vars_to_sccs[it->second.first.front()]);// the new SCC
                
                for (set<int>::reverse_iterator rem_it = factors_to_remove.rbegin(); rem_it != factors_to_remove.rend(); ++rem_it){
                    FactoredVars::iterator r_it = sccs.begin() + *rem_it;
                    sccs.erase(r_it);
                }                
                
                // compute mobility of potentially new factoring
                for (size_t i = 0; i < sccs.size(); ++i){
                    size_t mob = 0;
                    for (set<int>::iterator scc_it = sccs[i].begin(); scc_it != sccs[i].end(); ++scc_it){
                        mob += mobility[*scc_it];
                    }
                    new_mobility *= mob;
                }
#ifdef DEBUG_FACTORING
                cout << "new factoring would have mobility " << new_mobility << " and " << sccs.size() << " leaf factors" << endl;
#endif
                
                if (new_mobility > successor_mobility && sccs.size() > min_number_leaves){
#ifdef DEBUG_FACTORING
                    cout << "better than current best successor factoring => replace" << endl;
#endif                    
                    best_successor_scc = it;
                    successor_mobility = new_mobility;
                    change = true;
                }
            }
        }
        
        // update best factoring and check if any candidates left
        if (change){
            const set<int> &succ_pred_sccs = best_successor_scc->second.second;
            
            current_factoring.push_back(set<int>());
            set<int> &new_factor = current_factoring.back();
            
            set<int> factors_to_remove;
            for (set<int>::iterator succ_it = succ_pred_sccs.begin(); succ_it != succ_pred_sccs.end(); ++succ_it){
                set<int> succ_pred = current_factoring[*succ_it];
                
                factors_to_remove.insert(*succ_it);
                for (set<int>::iterator var_it = succ_pred.begin(); var_it != succ_pred.end(); ++var_it){
                    new_factor.insert(*var_it);
                }
            }
            
            const vector<int> &variables = best_successor_scc->second.first;
            
            for (size_t i = 0; i < variables.size(); ++i){
                new_factor.insert(variables[i]);
            }
            
            for (set<int>::reverse_iterator rem_it = factors_to_remove.rbegin(); rem_it != factors_to_remove.rend(); ++rem_it){
                FactoredVars::iterator r_it = current_factoring.begin() + *rem_it;
                current_factoring.erase(r_it);
            }
                    
            canditate_sccs.erase(best_successor_scc);
            
            if (successor_mobility > best_mobility){
                best_factoring = current_factoring;
                best_mobility = successor_mobility;
#ifdef DEBUG_FACTORING
                cout << "found a new best factoring with " << best_factoring.size() << " leaf factors and mobility " << best_mobility << endl;
#endif
            }
            
            if (canditate_sccs.empty() || (all_sscs_fit_bound && canditate_sccs.size() == 1)){
#ifdef DEBUG_FACTORING
                cout << "no more candidates left, factoring completed" << endl;
#endif
                break;
            }
        }
    }
    
#ifdef DEBUG_FACTORING
    for (size_t i = 0; i < best_factoring.size(); i++){
        cout << "leaf factor " << i << endl;
        for (set<int>::iterator set_it = best_factoring[i].begin(); set_it != best_factoring[i].end(); ++set_it){
            cout << "   variable " << (*set_it) << " :"  << g_fact_names[*set_it][0] << endl;
        }
    }
    cout << "mobility: " << best_mobility << endl;
#endif

    return best_factoring;
}

Factoring::FactoredVars Factoring::get_sccs(vector<int> variables) const {
    vector<vector<int> > vars(g_variable_domain.size());
    size_t bound = variables.empty() ? g_variable_domain.size() : variables.size();
    for (size_t i = 0; i < bound; i++){
        if (variables.empty() || variables.size() == g_variable_domain.size()){
            vars[i] = g_causal_graph->get_successors(i);
        } else {
            vector<int> successors = g_causal_graph->get_successors(variables[i]);
            for (size_t succ = 0; succ < successors.size(); succ++){
                if (find(variables.begin(), variables.end(), successors[succ]) != variables.end()){
                    vars[variables[i]].push_back(successors[succ]);
                }
            }
        }
    }
    
    SCC scc(vars);
    vector<vector<int> > found_sccs = scc.get_result();
    FactoredVars real_sccs;
    
    for (size_t i = 0; i < found_sccs.size(); i++){
        if (found_sccs[i].size() != 1 || variables.empty() || variables.size() == g_variable_name.size()){
            real_sccs.push_back(set<int>(found_sccs[i].begin(), found_sccs[i].end()));
        } else if (find(variables.begin(), variables.end(), found_sccs[i][0]) != variables.end()){
	    // this can happen if not all variables are in *variables*
	    // the SCC class needs the input vector to be aligned very specifically
            real_sccs.push_back(set<int>(found_sccs[i].begin(), found_sccs[i].end()));
        }
    }
    
#ifdef DEBUG_FACTORING
    cout << "found " << real_sccs.size() << " SCCs" << endl;
#endif
    
    return real_sccs;
}

Factoring::FactoredVars Factoring::get_connected_components(vector<int> variables) const {
    FactoredVars components;
    
    vector<bool> vars(g_variable_domain.size(), false);
    for (size_t i = 0; i < variables.size(); i++){
        vars[variables[i]] = true;
    }
    
    vector<bool> seen_vars(g_variable_domain.size(), false);
    vector<int> var_comp_map(g_variable_domain.size(), -1);
    
    size_t number_seen_vars = 0;
    
    set<int> open_vars;
    
    while(number_seen_vars < variables.size()){
	if (open_vars.empty()){
	    for (size_t i = 0; i < variables.size(); i++){
                int var = variables[i];
	    	if (!seen_vars[var]){
		    open_vars.insert(var);
		    var_comp_map[var] = components.size();
		    components.push_back(set<int>());
		    components[var_comp_map[var]].insert(var);
		    break;
		}
	    }
	    continue;
	}
	
	int curr = *open_vars.begin();
	open_vars.erase(open_vars.begin());
	
	if (seen_vars[curr]){
	    continue;
	}
	
	seen_vars[curr] = true;
	number_seen_vars++;
	
	for (size_t pre = 0; pre < g_causal_graph->get_predecessors(curr).size(); pre++){
	    int var = g_causal_graph->get_predecessors(curr)[pre];
	    if (vars[var]){
		open_vars.insert(var);
		var_comp_map[var] = var_comp_map[curr];
		components[var_comp_map[var]].insert(var);
	    }
	}
	for (size_t succ = 0; succ < g_causal_graph->get_successors(curr).size(); succ++){
	    int var = g_causal_graph->get_successors(curr)[succ];
	    if (vars[var]){
                open_vars.insert(var);
                var_comp_map[var] = var_comp_map[curr];
                components[var_comp_map[var]].insert(var);
            }
	}
    }
    
    return components;
}

vector<bool> Factoring::get_frozen_leaves(FactoredVars components) const {
    vector<bool> is_frozen_factor(components.size(), true);
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);
    
    for (size_t i = 0; i < components.size(); i++){
        for (set<int>::iterator it = components[i].begin(); it != components[i].end(); ++it){
            belongs_to_factor[*it] = i;
        }
    }
    
    for (size_t i = 0; i < g_operators.size(); i++){
        const Operator *op = &g_operators[i];
        int affects_factor = -1;
        for (size_t post = 0; post < op->get_effects().size(); post++){
            const Effect &prepost = op->get_effects()[post];
            affects_factor = belongs_to_factor[prepost.var];
            if (affects_factor == -1){
                break;
            }
        }
        if (affects_factor != -1){
            is_frozen_factor[affects_factor] = false;
        }
    }

    return is_frozen_factor;
}


/////////////////////////////////////////////////////////////////////////////////////////////// domain specific factorings


Factoring::FactoredVars SokobanFactoring::get_factoring() {
    
    FactoredVars best_factoring = vector<set<int> >(1);
    
    cout << "factoring sokoban" << endl;
    
    for (size_t var = 0; var < g_variable_domain.size(); var++){
	if (g_fact_names[var][0].find("player") != string::npos){
	    best_factoring[0].insert(var);
	}
    }
    
    cout << endl;
    for (size_t var = 0; var < g_variable_domain.size(); var++){
        bool is_leaf = false;
        for (size_t l = 0; l < best_factoring.size(); l++){
            if (best_factoring[l].find(var) != best_factoring[l].end()){
                cout << "leaf " << l << ":    " << g_fact_names[var][0] << endl;
                is_leaf = true;
            }
        }
        if (!is_leaf){
            cout << "center:    " << g_fact_names[var][0] << endl;
        }
    }
    cout << endl;
    
    return best_factoring;
}

Factoring::FactoredVars OpenstacksFactoring::get_factoring() {
    
    vector<int> leaf_vars;
    
    for (size_t var = 0; var < g_variable_domain.size(); var++){
	if (g_fact_names[var][0].find("stacks-avail") == string::npos){
	    leaf_vars.push_back(var);
	}
    }
    
    FactoredVars sccs = get_sccs(leaf_vars);
    
    FactoredVars best_factoring;
    
    for (size_t i = 0; i < sccs.size(); i++){
	uint64_t current_size = 1;
	for (set<int>::iterator it = sccs[i].begin(); it != sccs[i].end(); ++it){
	    current_size *= g_variable_domain[*it];
	    if (current_size > max_leaf_size){
		break;
	    }
	}
	if (current_size <= max_leaf_size){
	    best_factoring.push_back(sccs[i]);
	}
    }
    
    
    cout << endl;
    for (size_t var = 0; var < g_variable_domain.size(); var++){
        bool is_leaf = false;
        for (size_t l = 0; l < best_factoring.size(); l++){
            if (best_factoring[l].find(var) != best_factoring[l].end()){
                cout << "leaf " << l << ":    " << g_fact_names[var][0] << endl;
                is_leaf = true;
            }
        }
        if (!is_leaf){
            cout << "center:    " << g_fact_names[var][0] << endl;
        }
    }
    cout << endl;    
    
    
    return best_factoring;
}


Factoring::FactoredVars TPPConstrainedFactoring::get_factoring() {
        
    vector<int> leaf_vars;
    
    for (size_t var = 0; var < g_variable_domain.size(); var++){
        if (g_fact_names[var][0].find("money(level") == string::npos 
            && g_fact_names[var][0].find("at(truck") == string::npos){
            if (!inverted){
                leaf_vars.push_back(var);
            }
        } else if (inverted){
            leaf_vars.push_back(var);
        }
    }
    
    FactoredVars sccs = get_sccs(leaf_vars);
    
    FactoredVars best_factoring;
    
    for (size_t i = 0; i < sccs.size(); i++){
        uint64_t current_size = 1;
        for (set<int>::iterator it = sccs[i].begin(); it != sccs[i].end(); ++it){
            current_size *= g_variable_domain[*it];
            if (current_size > max_leaf_size){
                break;
            }
        }
        if (current_size <= max_leaf_size){
            best_factoring.push_back(sccs[i]);
        }
    }
    
    
    cout << endl;
    for (size_t var = 0; var < g_variable_domain.size(); var++){
        bool is_leaf = false;
        for (size_t l = 0; l < best_factoring.size(); l++){
            if (best_factoring[l].find(var) != best_factoring[l].end()){
                cout << "leaf " << l << ":    " << g_fact_names[var][0] << endl;
                is_leaf = true;
            }
        }
        if (!is_leaf){
            cout << "center:    " << g_fact_names[var][0] << endl;
        }
    }
    cout << endl;    
    
    
    return best_factoring;
}


void Factoring::add_options_to_parser(OptionParser &parser) {
    // TODO add docu
    parser.add_option<Pruning*>(
        "pruning", 
        "TODO", 
        "",
        OptionFlags(false));
    vector<string> search_types;
    search_types.push_back("fra");
    search_types.push_back("asda");
    search_types.push_back("sda");
    search_types.push_back("sat");
    parser.add_enum_option(
        "search_type",
        search_types,
        "TODO",
        "sda",
        vector<string>(),
        OptionFlags(false));
    parser.add_option<int>(
        "max_leaf_size",
        "TODO",
        "",
        OptionFlags(false));
}

void ForkFactoring::add_options_to_parser(OptionParser& parser) {
    // TODO add docu
    parser.add_option<bool>(
        "maximize_mobility",
        "TODO",
        "false");
}

static Factoring *_parse_fork(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    ForkFactoring::add_options_to_parser(parser);

    Options opts = parser.parse();

    ForkFactoring *factoring = 0;
    if (!parser.dry_run()) {        
        factoring = new ForkFactoring(opts);
    }

    return factoring;
}

static Factoring *_parse_ifork(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    ForkFactoring::add_options_to_parser(parser);
    
    Options opts = parser.parse();

    ForkFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new ForkFactoring(opts, true);
    }

    return factoring;
}

static Factoring *_parse_xshape(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    ForkFactoring::add_options_to_parser(parser);
    
    Options opts = parser.parse();

    XShapeFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new XShapeFactoring(opts);
    }

    return factoring;
}

static Factoring *_parse_incarcs(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    
    Options opts = parser.parse();

    IncidentArcsFactoring::Factoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new IncidentArcsFactoring(opts);
    }

    return factoring;
}

static Plugin<Factoring> _plugin_fork("fork", _parse_fork);
static Plugin<Factoring> _plugin_ifork("ifork", _parse_ifork);
static Plugin<Factoring> _plugin_xshape("xshape", _parse_xshape);
static Plugin<Factoring> _plugin_incarcs("incarcs", _parse_incarcs);
// static Plugin<Factoring> _plugin_rand("rand", _parse_rand);
// static Plugin<Factoring> _plugin_sokoban("sokoban", _parse_sokoban);
// static Plugin<Factoring> _plugin_tpp_const("tpp_const", _parse_tpp_const);
// static Plugin<Factoring> _plugin_itpp_const("itpp_const", _parse_itpp_const);
// static Plugin<Factoring> _plugin_openstacks("openstacks", _parse_openstacks);

