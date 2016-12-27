#include "factoring.h" 

#include "causal_graph.h"
#include "compliant_paths/compliant_path_graph.h"
#include "globals.h"
#include "operator.h"
#include "option_parser.h"
#include "plugin.h"
#include "pruning/pruning.h"
#include "../preprocess/scc.h"
#include "successor_generator.h"
#include "timer.h"
#include "utilities.h"

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
            search_type(SEARCH_TYPE(opts.get_enum("search_type"))),
            no_search(opts.get<bool>("no_search")){
    switch(search_type){
        case FRA : cout << "performing Fork-Root A* search" << endl; break;
        case ASDA : cout << "performing Anytime Star Decoupled A* search" << endl; break;
        case SDA : cout << "performing Star Decoupled A* search" << endl; break;
        case SAT : cout << "performing decoupled satisficing search" << endl; break;
        case UNSAT : cout << "trying to prove task unsolvable" << endl; break;
        default : cout << "unknown decoupled search type " << search_type << endl; exit_with(EXIT_CRITICAL_ERROR);
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
        CompliantPathGraph::set_pruning(opts.get<Pruning*>("pruning"));
    } else {
        CompliantPathGraph::set_pruning(new BasicPruning());
        cout << "using default pruning method: BasicPruning" << endl;
    }
    min_number_leaves = 2;
    max_precompute_state_space_size = opts.get<int>("build_state_space_size");
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
    
    bool maximize_mobility;
    
    bool extend_leaves;
    
    bool average_mobility;
    
    FactoredVars get_fork_maximize_leaves(bool inverted = false, std::vector<int> variables = std::vector<int>()) const;
    
    FactoredVars get_fork_maximize_mobility(bool inverted = false, std::vector<int> variables = std::vector<int>()) const;
    
public:
    
    ForkFactoring(const Options &opts, PROFILE profile = FORK)
            : Factoring(opts),
              maximize_mobility(opts.get<bool>("maximize_mobility")),
              extend_leaves(opts.get<bool>("extend_leaves")),
              average_mobility(opts.get<bool>("average_mobility")) {
        if (profile == I_FORK){
            Factoring::profile = I_FORK;
            cout << "using inverted-fork factoring profile" << endl;
        } else if (profile == FORK){
            Factoring::profile = FORK;
            cout << "using fork factoring profile" << endl;
        }
    };
    
    virtual FactoredVars get_factoring();
    
    static void add_options_to_parser(OptionParser &parser);
    
};

class XShapeFactoring : public ForkFactoring {
    
public:
    
    XShapeFactoring(const Options &opts)
            : ForkFactoring(opts, X_SHAPE) {
        profile = X_SHAPE;
        cout << "using X-shape factoring profile" << endl;
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

class MaxIndependentSetFactoring : public Factoring {
    enum FEATURE {
        MOBILITY,
        LEAVES,
        CENTER_AFF_A,
        LEAF_AFF_CENTER_A_AVG,
        LEAF_AFF_LEAF_A_AVG,
        LEAF_SIZE_AVG,
        CENTER_SIZE,
        NONE
    };
protected:
    enum MIS_ALGO arg_mis_algo;
    int arg_minimize_center;
    bool arg_minimize_frozen;
    enum FEATURE arg_feature_1;
    enum FEATURE arg_feature_2;
    int arg_xth_factoring = 0;
    int arg_merge_leaves = 0;
    int arg_find_all;
    int arg_all_rankings;

public:

    MaxIndependentSetFactoring(const Options &opts)
            : Factoring(opts),
                arg_mis_algo(MIS_ALGO(opts.get_enum("mis_algo"))),
                arg_minimize_center(opts.get<int>("minimize_center")),
                arg_minimize_frozen(opts.get<bool>("minimize_frozen")),
                arg_feature_1(FEATURE(opts.get_enum("feature_1"))),
                arg_feature_2(FEATURE(opts.get_enum("feature_2"))),
                arg_xth_factoring(opts.get<int>("xth_fact")),
                arg_merge_leaves(opts.get<int>("merge_leaves")),
                arg_find_all(opts.get<int>("find_all")),
                arg_all_rankings(opts.get<bool>("all_rankings")){
        profile = MAX_IND_SET;
        cout << "using maximum independet set factoring profile - algo: " << arg_mis_algo << " find all MIS: "
             << arg_find_all << " minimize_center " << arg_minimize_center << " optimize feature: " << arg_feature_1
             << ", " << arg_feature_2 << " all rankings: " << arg_all_rankings << " minimize frozen " << arg_minimize_frozen << " select xth factoring: "
             << arg_xth_factoring << " merge leaves level: " << arg_merge_leaves << endl;
    };

    virtual FactoredVars get_factoring();
    FactoredVars minimize_center(FactoredVars, const NGraph&);
    void optimize_feature(FactoredVars &, const NGraph &);
    uint64_t rank_factoring(const FactoredVars &, const FEATURE, bool all_data = false);
    uint64_t rank_mobility(const FactoredVars &, bool);
    uint64_t rank_leaves(const FactoredVars &, bool);
    uint64_t rank_aff_a(const FactoredVars &, bool, bool, bool);
    uint64_t rank_leaf_size_avg(const FactoredVars &, bool);
    uint64_t rank_center_size(const FactoredVars &, bool);
    //int optimize_experimental(const NGraph &, const set<int> &, const vector<bool> &, const map<int,int> &);
    NGraph generate_ngraph();
    bool check_factoring(FactoredVars);

    static void add_options_to_parser(OptionParser &parser);

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

class DocTransferFactoring : public Factoring {
  
public:
    
    DocTransferFactoring(const Options &opts)
            : Factoring(opts) {
        profile = DOC_TRANSFER;
        cout << "using doc transfer factoring profile" << endl;
    };
    
    virtual FactoredVars get_factoring();
  
};

class PegsolFactoring : public Factoring {
  
public:
    
    PegsolFactoring(const Options &opts)
            : Factoring(opts) {
        profile = PEGSOL;
        cout << "using pegsol factoring profile" << endl;
    };
    
    virtual FactoredVars get_factoring();
  
};



void Factoring::apply_factoring(FactoredVars factoring) {
    g_belongs_to_factor = vector<int>(g_variable_domain.size(), -1);        // -1 => center factor
    g_new_index = vector<size_t>(g_variable_domain.size());
    interaction_graph = InteractionGraph(factoring.size());
    g_factors.resize(factoring.size());
    
    for (size_t i = 0; i < factoring.size(); i++){
#ifdef DEBUG_FACTORING
        cout << "leaf factor " << i << ":" << endl;
#endif        
        vector<int> variables(factoring[i].begin(), factoring[i].end());              
        for (size_t j = 0; j < variables.size(); ++j){
            g_belongs_to_factor[variables[j]] = i;
            g_new_index[variables[j]] = j;
#ifdef DEBUG_FACTORING
            cout << "   var: " << variables[j] << " - " << g_fact_names[variables[j]][0] << endl;
#endif
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
        assert(!eff_factors.empty());
        
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

        bool is_center_op = false;
        if (eff_factors.find(-1) != eff_factors.end()){
            // effect on center variable
            is_center_op = true;
        } else if (eff_factors.size() > 1){
            // effect on more than one factor
            is_center_op = true;
        } else {
            pre_factors.erase(-1);
            if (pre_factors.size() > 1){
                // precondition on more than one leaf
                is_center_op = true;
            } else if (!pre_factors.empty() && *pre_factors.begin() != *eff_factors.begin()){
                // precondition on leaf A, but effect on leaf B
                is_center_op = true;
            }
        }
        if (!is_center_op){
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
    
    CompliantPathGraph::initialize();
    
    // setup g_goals_per_factor
    // TODO check if each fork leaf contains a goal! what to do if not??
    // this should probably be assured by the get_factoring methods?!
    g_goals_per_factor = vector<vector<pair<int, int> > >(g_factors.size());
    vector<int> indeces_to_remove;
    for (size_t g = 0; g < g_goal.size(); ++g){
        int affected_factor = g_belongs_to_factor[g_goal[g].first];
        if (affected_factor != -1){
            g_goals_per_factor[affected_factor].push_back(g_goal[g]);
            indeces_to_remove.push_back(g);
#ifdef DEBUG_FACTORING
            cout << "added fact to goal of factor " << affected_factor << ": "
                 << g_fact_names[g_goal[g].first][g_goal[g].second] << endl;
#endif
        }
    }
    vector<pair<int, int> >::iterator it = g_goal.begin();
    for (int i = indeces_to_remove.size() - 1; i >= 0; --i){
        g_goal.erase(it + indeces_to_remove[i]);
    }
#ifdef DEBUG_FACTORING
    if (!g_goal.empty()){
        cout << "center goals:" << endl;
        for (size_t i = 0; i < g_goal.size(); ++i){
            cout << g_fact_names[g_goal[i].first][g_goal[i].second] << endl;
        }
    } else {
        cout << "no center goals" << endl;
    }
#endif
}

void Factoring::print_factoring() const {
    
    cout << "number leaf factors: " << g_factors.size() << endl;
    
    cout << g_center_vars.size() << " variables in center factor" << endl;
    
    for (size_t i = 0; i < g_factors.size(); ++i) {
        cout << g_factors[i].size() << " variables in leaf factor " << i << endl;
    }
    
    vector<bool> precompute_leaf_state_space(g_factors.size(), true);
    
    uint64_t min_size = numeric_limits<uint64_t>::max();
    double avg_size = 0;
    uint64_t max_size = 0;
    for (size_t i = 0; i < g_factors.size(); ++i){
        uint64_t f_size = 1;
        for (size_t j = 0; j < g_factors[i].size(); ++j){
            f_size *= g_variable_domain[g_factors[i][j]];
            if ((search_type == SAT || search_type == UNSAT) && f_size > max_precompute_state_space_size){
                precompute_leaf_state_space[i] = false;
            }
        }
        avg_size += f_size;
        min_size = min(min_size, f_size);
        max_size = max(max_size, f_size);
    }
    CompliantPathGraph::set_precompute_leaf_state_spaces(precompute_leaf_state_space);
    
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

void Factoring::exit_error(string message, ExitCode exit_code) const {
    cout << "Error while factoring planning task!" << endl;
    cout << message << endl;
    exit_with(exit_code);
}

void Factoring::do_factoring_or_abstain() {

    // for calculating the factoring time
    Timer t;
    
    FactoredVars best_factoring = get_factoring();
    
    cout << "factoring time " << t << endl;
    
    if (best_factoring.empty()){
        exit_error("No factoring found!", EXIT_ABSTAINED);
    }
    
    apply_factoring(best_factoring);    
    
    print_factoring();
    if (no_search) {
        cout << "Factoring complete, do not perform search" << endl;
        exit_with(EXIT_NO_SEARCH);
    }
}

vector<set<int> > XShapeFactoring::get_factoring() {    ///////////////////////////  X-SHAPE
    
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
            inv_fork = get_fork_maximize_mobility(true, variables);
        } else {
            inv_fork = get_fork_maximize_leaves(true, variables);
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
#ifdef DEBUG_FACTORING
                        cout << "ifork leaf " << i << " is connected to fork leaf " << j << ". need to discard it" << endl;
#endif
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
            ++added;
        }
    }
    
    cout << "added " << added << " roots" << endl;
    
    if (best_factoring.size() < min_number_leaves){
        
        if (maximize_mobility){
            best_factoring = get_fork_maximize_mobility(true);
        } else {
            best_factoring = get_fork_maximize_leaves(true);
        }
        
        cout << "found inv-fork factoring with " << best_factoring.size() << " leaves" << endl;
        
        if (best_factoring.size() < min_number_leaves){
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
        factoring = get_fork_maximize_mobility(profile == I_FORK);
    } else {
        factoring = get_fork_maximize_leaves(profile == I_FORK);
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
            cout << "   var: " << *it << " - " << g_fact_names[*it][0] << endl;
        }        
    }
    cout << endl;
#endif
    
    return best_factoring;
}

Factoring::FactoredVars ForkFactoring::get_fork_maximize_leaves(bool inverted, vector<int> used_vars) const {             ///////////////////////// HELPER FUNCTIONS
    // TODO probably refactor and merge with get_fork_maximize_mobility method
    
    FactoredVars sccs = get_sccs(used_vars);
    
    FactoredVars best_factoring;

    if (sccs.size() == 1){
#ifdef DEBUG_FACTORING
        cout << "CG is a single SCC" << endl;
#endif
        return best_factoring;
    }
    
    typedef multimap<uint64_t, pair<vector<int>, set<int> > > CandSCCs;
    // contains all leaf SCCs sorted by size
    CandSCCs canditate_sccs;
    
    bool all_leaves_fit_bound = true;
    
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
        cout << "=========      " << (is_leaf ? "leaf " : "") << "SCC with " << sccs[i].size() << " variables" << endl;
        for (set<int>::iterator it = sccs[i].begin(); it != sccs[i].end(); ++it){
            cout << "   " << g_fact_names[*it][0] << endl;
        }
        cout << "=========      end SCC; has size " << current_size << endl;
#endif
        if (max_leaf_size == 0 || current_size <= max_leaf_size){
            if (is_leaf){
                best_factoring.push_back(sccs[i]);
            } else if (extend_leaves){
                canditate_sccs.insert(make_pair(current_size, make_pair(vector<int>(sccs[i].begin(), sccs[i].end()), set<int>())));
            }
        } else {
            all_leaves_fit_bound = false;
        }
    }
    bool change = true;
    while (extend_leaves && change && (!all_leaves_fit_bound || canditate_sccs.size() > 1)){
        change = false;
        
                // update interaction graph of remaining SCCs
        for (CandSCCs::iterator it = canditate_sccs.begin(); it != canditate_sccs.end(); ++it){
            set<int> &succ_pred_sccs = it->second.second;
            succ_pred_sccs.clear();
            
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
                        for (size_t j = 0; j < best_factoring.size(); ++j){
                            if (best_factoring[j].find(succ_pred) != best_factoring[j].end()){
                                succ_pred_sccs.insert(j);
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
                    succ_pred_sccs.insert(-1);
                    break;
                }
            }
        }
        
        // try to augment an existing leaf with another SCC
        for (CandSCCs::iterator it = canditate_sccs.begin(); it != canditate_sccs.end(); ++it){
#ifdef DEBUG_FACTORING
            cout << "trying to augment leaf by candidate SCC:" << endl;
            for (size_t i = 0; i < it->second.first.size(); ++i){
                cout << "       " <<  g_fact_names[it->second.first[i]][0] << endl;
            }
            if (it->second.second.size() != 1){
                cout << "has more than 1 successor" << endl;
            }
            if (it->second.second.find(-1) != it->second.second.end()){
                cout << "has non leaf successors" << endl;
            }
#endif
            if (it->second.second.size() == 1 &&
                it->second.second.find(-1) == it->second.second.end()){
                // candidate only has a single CG successor/predecessor SCC that is also a leaf
                
                int successor = *it->second.second.begin();
                
                bool can_add = true;
                uint64_t new_size = it->first;
                for (set<int>::const_iterator scc_it = best_factoring[successor].begin(); scc_it != best_factoring[successor].end(); ++scc_it){
                    new_size *= g_variable_domain[*scc_it];
                    if (new_size > max_leaf_size){
                        can_add = false;
                        break;
                    }
                }
                
                if (can_add){
                    vector<int> &cand = it->second.first;
                    for (size_t i = 0; i < cand.size(); ++i){
                        best_factoring[successor].insert(cand[i]);
                    }
                    canditate_sccs.erase(it);
                    change = true;
#ifdef DEBUG_FACTORING
                    cout << "merged" << endl;
#endif
                    break;
                }
#ifdef DEBUG_FACTORING
                else {
                    cout << "resulting leaf would exceed size limit" << endl;
                }
#endif
            }
        }
        
        
    }
    return best_factoring;
}

Factoring::FactoredVars ForkFactoring::get_fork_maximize_mobility(bool inverted, vector<int> used_vars) const {
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
                if (average_mobility){
                    best_mobility += mobility[i];
                } else {
                    best_mobility *= mobility[i];
                }
            } else {
                canditate_sccs.insert(make_pair(current_size, make_pair(vector<int>(sccs[i].begin(), sccs[i].end()), set<int>())));
            }
        } else {
            all_sscs_fit_bound = false;
#ifdef DEBUG_FACTORING
            cout << " => too large" << endl;
#endif
        }
    }
    
#ifdef DEBUG_FACTORING
    cout << "number leaf factors with leaf-SCCs only: " << best_factoring.size() << " mobility = " << (average_mobility ? best_mobility / best_factoring.size() : best_mobility) << endl;
    cout << "number canditate sccs: " << canditate_sccs.size() << endl;
#endif
    
    FactoredVars current_factoring = best_factoring;
    
    bool change = !all_sscs_fit_bound || canditate_sccs.size() > 1;
    while (change){

        change = false;
        
        // update interaction graph of remaining SCCs
        for (CandSCCs::iterator it = canditate_sccs.begin(); it != canditate_sccs.end(); ++it){
            set<int> &succ_pred_sccs = it->second.second;
            succ_pred_sccs.clear();
            
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
                                succ_pred_sccs.insert(j);
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
                    succ_pred_sccs.insert(-1);
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
                    if (average_mobility) {
                        new_mobility += mob;
                    } else {
                        new_mobility *= mob;
                    }
                }
                if (average_mobility) {
                    new_mobility /= sccs.size();
                }
#ifdef DEBUG_FACTORING
                cout << "new factoring would have mobility " << new_mobility << " and " << sccs.size() << " leaf factors" << endl;
#endif
                
                if (new_mobility > successor_mobility && sccs.size() >= min_number_leaves){
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
        set<int> pre_factors;
        set<int> eff_factors;

        for (size_t prev = 0; prev < op->get_preconditions().size(); ++prev) {
            const Condition &prevail = op->get_preconditions()[prev];
            pre_factors.insert(belongs_to_factor[prevail.var]);
        }
        pre_factors.erase(-1);
        bool center = false;
        for (size_t post = 0; post < op->get_effects().size(); post++){
            const Effect &prepost = op->get_effects()[post];
            eff_factors.insert(belongs_to_factor[prepost.var]);
            affects_factor = belongs_to_factor[prepost.var];
            if (affects_factor == -1){
                center = true;
            }
        }
        if (!center && !(eff_factors.size() > 1) && !(pre_factors.size() > 1) &&
                (pre_factors.empty() || (*pre_factors.begin() == *eff_factors.begin()))) {
            is_frozen_factor[affects_factor] = false;
        }
    }

    return is_frozen_factor;
}

Factoring::FactoredVars MaxIndependentSetFactoring::get_factoring() {
    const NGraph cg_g = generate_ngraph();

    MIS mis(cg_g);
    set<int> found_mis = mis.get_mis(arg_mis_algo, arg_xth_factoring, arg_find_all);
#ifdef DEBUG_FACTORING
    cout << "MIS: " << endl;
    for (set<int>::iterator it = found_mis.begin(); it != found_mis.end(); ++it) {
        cout << "(" << *it << " )" << endl;
    }
#endif

    // generate factoring
    FactoredVars max_ind_sets;
    int l = 0;
    for(set<int>::iterator it = found_mis.begin(); it != found_mis.end(); ++it){
        set<int> s;
        s.insert(*it);
        max_ind_sets.push_back(s);
        l++;
    }

    //max_ind_sets = minimize_center(max_ind_sets, cg_g);
    optimize_feature(max_ind_sets, cg_g);

#ifdef DEBUG_FACTORING
    cout << "minimized MIS: " << endl;
    for(size_t i = 0; i < max_ind_sets.size(); i++){
        cout << "(";
        for (set<int>::iterator it = max_ind_sets[i].begin(); it != max_ind_sets[i].end(); ++it) {
            cout << *it << " ";
        }
        cout << ")" << endl;
    }
#endif
    // push frozen leaves back in center
    FactoredVars temp;
    int f_rm = 0;

    vector<bool> frozen_leaves = get_frozen_leaves(max_ind_sets);
    for (size_t i = 0; i < frozen_leaves.size(); i++) {
        if (!frozen_leaves[i]) {
            temp.push_back(max_ind_sets[i]);
        } else {
            f_rm++;
        }
    }
    max_ind_sets = temp;
    cout << "Removed frozen leaves: " << f_rm << endl;

    cout << "Number of factors: " << max_ind_sets.size() << endl;

    bool valid = check_factoring(max_ind_sets);
    if (!valid) {
        exit_with(EXIT_ABSTAINED);
    }

    return max_ind_sets;
}

NGraph MaxIndependentSetFactoring::generate_ngraph() {
    vector<int> vars;
    for (size_t var = 0; var < g_variable_domain.size(); var++) {
        vars.push_back(var);
    }

    // build neighbour representation of causal graph (easier to modify)
    // graph of vars (var -> neighbours)
    NGraph cg_g;
    for (size_t i = 0; i < vars.size(); i++) {
        set<int> neighbours;
        for (size_t pre = 0; pre < g_causal_graph->get_predecessors(vars[i]).size(); pre++) {
            int var = g_causal_graph->get_predecessors(vars[i])[pre];
            neighbours.insert(var);
        }
        for (size_t succ = 0; succ < g_causal_graph->get_successors(vars[i]).size(); succ++) {
            int var = g_causal_graph->get_successors(vars[i])[succ];
            neighbours.insert(var);
        }
        cg_g.insert(NVertex(i, neighbours));
    }
    return cg_g;
}

bool MaxIndependentSetFactoring::check_factoring(FactoredVars f) {
    vector<int> vars;
    bool valid = true;
    for (size_t var = 0; var < g_variable_domain.size(); var++) {
        vars.push_back(var);
    }
    vector<int> var_to_factor(vars.size(), -1);
    for (size_t i = 0; i < f.size(); i++) {
        for (set<int>::iterator it = f[i].begin(); it != f[i].end(); ++it) {
            var_to_factor[*it] = i;
        }
    }
    map<int, pair<set<int>,set<int>>> ig;
    for (size_t i = 0; i < f.size(); i++) {
        set<int> parents;
        set<int> children;
        for (set<int>::iterator it = f[i].begin(); it != f[i].end(); ++it) {
            for (size_t pre = 0; pre < g_causal_graph->get_predecessors(vars[*it]).size(); pre++) {
                int var = g_causal_graph->get_predecessors(vars[*it])[pre];
                parents.insert(var_to_factor[var]);
            }
            for (size_t succ = 0; succ < g_causal_graph->get_successors(vars[*it]).size(); succ++) {
                int var = g_causal_graph->get_successors(vars[*it])[succ];
                children.insert(var_to_factor[var]);
            }
        }
        ig.insert(make_pair(i, make_pair(parents, children)));
    }
#ifdef DEBUG_FACTORING
    cout << "Factoring: " << endl;
    for (map<int, pair<set<int>,set<int>>>::iterator it = ig.begin(); it != ig.end(); ++it) {
        cout << "   Factor: " << it->first << endl;
        cout << "       Parents: ";
        for (set<int>::iterator p = it->second.first.begin(); p != it->second.first.end(); ++p) {
            if (it->first != -1 && (*p != it->first && *p != -1)) {
                valid = false;
                cout << "invalid parent: " << *p;
            } else {
                cout << *p << " ";
            }
        }
        cout << endl;
        cout << "       Children: ";
        for (set<int>::iterator c = it->second.second.begin(); c != it->second.second.end(); ++c) {
            if (it->first != -1 && (*c != it->first && *c != -1)) {
                valid = false;
                cout << "invalid child: " << *c;
            } else {
                cout << *c << " ";
            }
        }
        cout << endl;
    }
#endif
    return valid;
}
uint64_t MaxIndependentSetFactoring::rank_factoring(const FactoredVars & factoring, const FEATURE feature, bool all_data){
    int ret = 0;
    switch (feature) {
        default:
        case MOBILITY:
            ret = rank_mobility(factoring, all_data);
            break;
        case LEAVES:
            ret = rank_leaves(factoring, all_data);
            break;
        case CENTER_AFF_A:
            ret = rank_aff_a(factoring, all_data, false, false);
            break;
        case LEAF_AFF_CENTER_A_AVG:
            ret = rank_aff_a(factoring, all_data, true, false);
            break;
        case LEAF_AFF_LEAF_A_AVG:
            ret = rank_aff_a(factoring, all_data, true, true);
            break;
        case LEAF_SIZE_AVG:
            ret = rank_leaf_size_avg(factoring, all_data);
            break;
        case CENTER_SIZE:
            ret = rank_center_size(factoring, all_data);
            break;
        case NONE:
            break;
    }
    return ret;
}

uint64_t MaxIndependentSetFactoring::rank_mobility(const FactoredVars &factoring, bool) {
    vector<int> factor_actions(factoring.size(), 0);
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);

    for (size_t i = 0; i < factoring.size(); i++){
        for (set<int>::iterator it = factoring[i].begin(); it != factoring[i].end(); ++it){
            belongs_to_factor[*it] = i;
        }
    }

    for (size_t i = 0; i < g_operators.size(); i++){
        const Operator *op = &g_operators[i];
        int affects_factor = -1;
        set<int> pre_factors;
        set<int> eff_factors;

        for (size_t prev = 0; prev < op->get_preconditions().size(); ++prev) {
            const Condition &prevail = op->get_preconditions()[prev];
            pre_factors.insert(belongs_to_factor[prevail.var]);
        }
        pre_factors.erase(-1);
        bool center = false;
        for (size_t post = 0; post < op->get_effects().size(); post++){
            const Effect &prepost = op->get_effects()[post];
            eff_factors.insert(belongs_to_factor[prepost.var]);
            affects_factor = belongs_to_factor[prepost.var];
            if (affects_factor == -1){
                center = true;
            }
        }
        if (!center && !(eff_factors.size() > 1) && !(pre_factors.size() > 1) &&
            (pre_factors.empty() || (*pre_factors.begin() == *eff_factors.begin()))) {
            ++factor_actions[affects_factor];
        }
    }
    uint64_t mobility = 1;
    bool any_actions = false;
    for (int i:factor_actions) {
        if (i > 0) {
            any_actions = true;
            mobility *= i;
        }
    }
    if (!any_actions) {
        mobility = 0;
    }
    return mobility;
}

uint64_t MaxIndependentSetFactoring::rank_leaves(const FactoredVars &factoring, bool) {
    vector<bool> frozen = get_frozen_leaves(factoring);
    int f = 0;
    for (bool b:frozen) {
        if (!b) {
            ++f;
        }
    }
    return (uint64_t)f;
}

uint64_t MaxIndependentSetFactoring::rank_aff_a(const FactoredVars &factoring, bool all_data, bool aff_leaf, bool leaf_a) {
    vector<int> factor_affecting_actions(factoring.size(), 0);
    int affecting_center = 0;
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);

    for (size_t i = 0; i < factoring.size(); i++){
        for (set<int>::iterator it = factoring[i].begin(); it != factoring[i].end(); ++it){
            belongs_to_factor[*it] = i;
        }
    }

    for (size_t i = 0; i < g_operators.size(); i++){
        const Operator *op = &g_operators[i];
        int affects_factor = -1;
        set<int> pre_factors;
        set<int> eff_factors;

        for (size_t prev = 0; prev < op->get_preconditions().size(); ++prev) {
            const Condition &prevail = op->get_preconditions()[prev];
            pre_factors.insert(belongs_to_factor[prevail.var]);
        }
        pre_factors.erase(-1);
        bool center = false;
        for (size_t post = 0; post < op->get_effects().size(); post++){
            const Effect &prepost = op->get_effects()[post];
            eff_factors.insert(belongs_to_factor[prepost.var]);
            affects_factor = belongs_to_factor[prepost.var];
            if (affects_factor == -1){
                center = true;
            }
        }
        if (aff_leaf && leaf_a && !center && !(eff_factors.size() > 1) && !(pre_factors.size() > 1) &&
            (pre_factors.empty() || (*pre_factors.begin() == *eff_factors.begin()))) {
            ++factor_affecting_actions[affects_factor];
        }
        else if (aff_leaf && !leaf_a && (center || (eff_factors.size() > 1) || (pre_factors.size() > 1) ||
            !(pre_factors.empty() || (*pre_factors.begin() == *eff_factors.begin())))) {
            for (int eff:eff_factors) {
                if (eff != -1) {
                    ++factor_affecting_actions[eff];
                }
            }
        }
        else if (!aff_leaf && center) {
            ++affecting_center;
        }

    }
    if (aff_leaf) {
        int avg = 0;
        int max = 0;
        int min = 0;
        if (!factor_affecting_actions.empty()) {
            int min = factor_affecting_actions[0];
            for (int i:factor_affecting_actions) {
                avg += i;
                if (i > max) {
                    max = i;
                }
                if (i < min) {
                    min = i;
                }
            }
            avg = avg / factor_affecting_actions.size();
        }
        if (all_data) {
            if (!leaf_a) {
                cout << "Ranking " << "leaf_affecting_center_actions_min" << ": " << min << endl;
                cout << "Ranking " << "leaf_affecting_center_actions_max" << ": " << max << endl;
            } else {
                cout << "Ranking " << "leaf_affecting_leaf_actions_min" << ": " << min << endl;
                cout << "Ranking " << "leaf_affecting_leaf_actions_max" << ": " << max << endl;
            }
        }
        return (uint64_t)avg;
    } else {
        return (uint64_t)affecting_center;
    }

}

uint64_t MaxIndependentSetFactoring::rank_leaf_size_avg(const FactoredVars &factoring, bool all_data) {
    int avg = 0;
    int max = 0;
    int min = 0;
    if (!factoring.empty()) {
        int min = factoring[0].size();
        for (set<int> factor:factoring) {
            int s = (int) factor.size();
            avg += s;
            if (s > max) {
                max = s;
            }
            if (s < min) {
                min = s;
            }
        }
        avg = avg / factoring.size();
    }
    if (all_data) {
        cout << "Ranking " << "leaf_size_min" << ": " << min << endl;
        cout << "Ranking " << "leaf_size_max" << ": " << max << endl;
    }
    return (uint64_t)avg;
}

uint64_t MaxIndependentSetFactoring::rank_center_size(const FactoredVars &factoring, bool) {
    size_t size = g_variable_domain.size();
    for (set<int> f:factoring){
        size -= f.size();
    }
    return size;
}

void MaxIndependentSetFactoring::optimize_feature(FactoredVars & mis, const NGraph & g) {
    if (arg_minimize_center == 0) {
        return;
    }

    // TODO - upgrade parser to parse a list of features
    vector<enum FEATURE> features (NONE,NONE);
    if (arg_feature_1 != NONE) {
        features[0] = arg_feature_1;
    } else {
        return;
    }
    if (arg_feature_2 != NONE) {
        features[2] = arg_feature_2;
    }

    // retrieve center vars
    set<int> center;
    vector<bool> in_center(g.size(), true);
    map<int, int> var_to_factor;
    for (size_t i = 0; i < mis.size(); i++) {
        for (set<int>::iterator it = mis[i].begin(); it != mis[i].end(); ++it) {
            in_center[*it] = false;
            var_to_factor.insert(pair<int, int>(*it, i));
        }
    }
    for (size_t i = 0; i < in_center.size(); i++) {
        if (in_center[i]) {
            center.insert(i);
            var_to_factor.insert(make_pair(i, -1));
        }
    }

    cout << "center size: " << center.size() << endl;

    vector<int> candidates; // pos -> var
    vector<int> candidates_to_leaf; // pos -> leaf

    // collect candidates (center vars with one leaf neighbour)
    for (set<int>::iterator it = center.begin(); it != center.end(); ++it) {
        set<int> leaf_neighbours;
        for (set<int>::iterator n = g.at(*it).begin(); n != g.at(*it).end(); ++n) {
            if (!in_center[*n]) {
                leaf_neighbours.insert(var_to_factor.at(*n));
            }
        }
        if (leaf_neighbours.size() == 1) {
            candidates.push_back(*it);
            candidates_to_leaf.push_back(*leaf_neighbours.begin());

        }
    }

    bool minimize = true;
    int c_rm = 0;
    int f_rm = 0;

    bool removed_frozen_leaves = false;

    while (minimize) {
        if (!candidates.empty()) {
            size_t max = 0;
            vector<uint64_t> max_rank (NONE,0);
            for (size_t i = 0; i < candidates.size(); ++i) {
                int candidate = candidates[i];
                int leaf = candidates_to_leaf[i];

                mis[leaf].insert(candidate);

                // call ranking (multiple rankings?)
                bool update = false;
                vector<uint64_t> ranking;
                for (int i = 0; i < NONE && !update; ++i) {
                    if (features[i] != NONE) {
                        ranking.push_back(rank_factoring(mis, features[i]));
                        if (ranking[i] > max_rank[i]) {
                            update = true;
                        }
                    }else {
                        break;
                    }
                }
                if (update) {
                    max = i;
                    if (ranking.size() < features.size()) {
                        for (size_t i = ranking.size(); i < features.size(); ++i) {
                            ranking.push_back(rank_factoring(mis, features[i]));
                        }
                    }
                    max_rank = ranking;
                }
                mis[leaf].erase(mis[leaf].find(candidate));

            }
            int candidate = candidates[max];
            int leaf = candidates_to_leaf[max];
            // and push it into the respective leaf
            mis[leaf].insert(candidate);
            in_center[candidate] = false;
            center.erase(candidate);
            var_to_factor[candidate] = leaf;
            candidates.erase(candidates.begin()+max);
            candidates_to_leaf.erase(candidates_to_leaf.begin()+max);
            c_rm++;
        } else {
            if (!removed_frozen_leaves) {
                // push frozen leaves back in center
                removed_frozen_leaves = true;
                FactoredVars temp;
                f_rm = 0;

                vector<bool> frozen_leaves = get_frozen_leaves(mis);
                for (size_t i = 0; i < frozen_leaves.size(); i++) {
                    if (!frozen_leaves[i]) {
                        temp.push_back(mis[i]);
                    } else {
                        f_rm++;
                    }
                }
                mis = temp;
                cout << "Removed frozen leaves: " << f_rm << endl;
#ifdef DEBUG_FACTORING
                cout << "Factoring" << endl;
                for (size_t i = 0; i < mis.size(); i++) {
                    cout << "   (";
                    set<int> factor = mis[i];
                    for (const int &it: factor) {
                        cout << it << " ";
                    }
                    cout << ")" << endl;
                }
                cout << endl;
#endif
                cout << "Ranking of Factoring: " << rank_factoring(mis,arg_feature_1) << endl;
                if (arg_all_rankings) {
                    cout << "All Rankings: " << endl;
                    for (int i = 0; i < NONE; ++i) {
                        string feature = "";
                        switch (i) {
                            case 0:
                                feature = "mobility";
                                break;
                            case 1:
                                feature = "leaves";
                                break;
                            case 2:
                                feature = "center_affecting_actions";
                                break;
                            case 3:
                                feature = "leaf_affecting_center_actions_avg";
                                break;
                            case 4:
                                feature = "leaf_affecting_leaf_actions_avg";
                                break;
                            case 5:
                                feature = "leaf_size_avg";
                                break;
                            case 6:
                                feature = "center_size";
                                break;
                            default:
                                feature = "none";
                        }
                        cout << "Ranking " << feature << ": " << rank_factoring(mis, FEATURE(i), true) << endl;
                    }
                    cout << endl;
                }
                if (f_rm == 0 || mis.empty()) {
                    cout << "removed no frozen leaves or no leaves left" << endl;
                    break;
                } else if (!arg_minimize_frozen) {
                    break;
                } else {
                    int f_c = 0;
                    // only continue to minimize center, including frozen leaves, if minimize_frozen = true
                    for (set<int>::iterator it = center.begin(); it != center.end(); ++it) {
                        set<int> leaf_neighbours;
                        for (set<int>::iterator n = g.at(*it).begin(); n != g.at(*it).end(); ++n) {
                            if (!in_center[*n]) {
                                leaf_neighbours.insert(var_to_factor.at(*n));
                            }
                        }
                        if (leaf_neighbours.size() == 1) {
                            candidates.push_back(*it);
                            candidates_to_leaf.push_back(*leaf_neighbours.begin());
                            ++f_c;
                        }
                    }
                    cout << "Frozen candidates: " << f_c;
                    if (f_c == 0) {
                        break;
                    } else {
                        continue;
                    }
                }
            } else {
                break;
            }
        }
    }
    cout << "Minimized center, variables removed: " << c_rm << endl;

}


Factoring::FactoredVars MaxIndependentSetFactoring::minimize_center(FactoredVars max_ind_sets, const NGraph & cg_g) {
    FactoredVars adapted_factoring;
    if (arg_minimize_center == 0) {
        cout << "do not minimize center" << endl;
        adapted_factoring =  max_ind_sets;
    }
    else if (arg_minimize_center == 1 || arg_minimize_center == 2) {
        cout << "minimize center - baseline - greedily increase mobility" << endl;
        // baseline - greedily maximize mobility

        // retrieve center vars
        set<int> center;
        vector<bool> in_center(cg_g.size(), true);
        map<int, int> var_to_factor;
        for (size_t i = 0; i < max_ind_sets.size(); i++) {
            for (set<int>::iterator it = max_ind_sets[i].begin(); it != max_ind_sets[i].end(); ++it) {
                in_center[*it] = false;
                var_to_factor.insert(pair<int, int>(*it, i));
            }
        }
        for (size_t i = 0; i < in_center.size(); i++) {
            if (in_center[i]) {
                center.insert(i);
                var_to_factor.insert(make_pair(i, -1));
            }
        }

        bool minimize = true;
        int c_rm = 0;

        vector<int> candidates; // pos -> var
        map<int,int> var_to_candidate; // var -> pos in candidates
        map<int, int> candidates_to_leaf; // var -> leaf

        bool removed_frozen_leaves = false;

        while (minimize) {
            candidates.clear();
            var_to_candidate.clear();
            candidates_to_leaf.clear();
            // collect candidates (center vars with one leaf neighbour)
            for (set<int>::iterator it = center.begin(); it != center.end(); ++it) {
                set<int> leaf_neighbours;
                for (set<int>::iterator n = cg_g.at(*it).begin(); n != cg_g.at(*it).end(); ++n) {
                    if (!in_center[*n]) {
                        leaf_neighbours.insert(var_to_factor[*n]);
                    }
                }
                if (leaf_neighbours.size() == 1) {
                    candidates.push_back(*it);
                    var_to_candidate.insert(make_pair(*it,var_to_candidate.size()));
                    candidates_to_leaf.insert(make_pair(*it, *leaf_neighbours.begin()));

                }
            }

            if (candidates_to_leaf.empty()){
                if (!removed_frozen_leaves){
                    // push frozen leaves back in center
                    removed_frozen_leaves = true;
                    FactoredVars temp;
                    int f_rm = 0;

                    vector<bool> frozen_leaves = get_frozen_leaves(max_ind_sets);
                    for (size_t i = 0; i < frozen_leaves.size(); i++) {
                        if (!frozen_leaves[i]) {
                            temp.push_back(max_ind_sets[i]);
                        } else {
                            f_rm++;
                        }
                    }
                    max_ind_sets = temp;
                    cout << "Removed frozen leaves: " << f_rm << endl;
                    if (f_rm == 0 || max_ind_sets.empty()) {
                        cout << "removed no frozen leaves or no leaves left" << endl;
                        break;
                    } else if(arg_minimize_center == 1) {
                        break;
                    } else {
                        // only continue to minimize center, including frozen leaves, if arg_min_center == 2
                        continue;
                    }
                } else {
                    break;
                }
            }

            // count how many actions affect each center var exclusively (no shared actions)
            vector<int> affecting_actions(var_to_candidate.size(), 0);
            for (size_t i = 0; i < g_operators.size(); i++) {
                const Operator *op = &g_operators[i];
                set<int> eff_factors;
                set<int> eff_center_vars;

                // test first whether this action affects one of the candidates, otherwise skip it
                for (size_t post = 0; post < op->get_effects().size(); post++) {
                    const int prepost = (op->get_effects()[post]).var;
                    eff_factors.insert(var_to_factor[prepost]);
                    if (var_to_factor[prepost] == -1) {
                        eff_center_vars.insert(prepost);
                    }
                }
                // has to affect center
                if (eff_center_vars.empty()) {
                    continue;
                }

                // has to affect a candidate
                bool affects_candidate = false;
                int candidate = -1; // var, not pos in candidates
                for (set<int>::iterator cv = eff_center_vars.begin(); cv != eff_center_vars.end(); ++cv) {
                    if (candidates_to_leaf.find(*cv) != candidates_to_leaf.end()) {
                        affects_candidate = true;
                        candidate = *cv;
                        break;
                    }
                }
                if (!affects_candidate) {
                    continue;
                }
                assert(candidate != -1);

                // may only affect candidate and/or respective candidate leaf
                if (eff_factors.size() >= 2) {
                    if (eff_factors.size() != 2 ||
                        (eff_factors.find(candidates_to_leaf[candidate]) == eff_factors.end())) {
                        continue;
                    }
                }

                // preconditions only on center or candidate leaf
                bool invalid = false;
                for (size_t prev = 0; prev < op->get_preconditions().size(); ++prev) {
                    const Condition &prevail = op->get_preconditions()[prev];
                    if ((var_to_factor[prevail.var] != -1) &&
                        (var_to_factor[prevail.var] != candidates_to_leaf[candidate])) {
                        invalid = true;
                        break;
                    }
                }
                if (invalid) {
                    continue;
                }

                // action either affects a center var exclusively,
                // or is shared between a center var and its only leaf neighbour
                affecting_actions[var_to_candidate[candidate]]++;
            }

            // find candidate that increases mobility the most
            int max = 0;
            for (size_t i = 1; i < affecting_actions.size(); i++) {
                if (affecting_actions[i] > affecting_actions[max]) {
                    max = i;
                }
            }

            // and push it into the respective leaf
            int candidate = candidates[max];
            int leaf = candidates_to_leaf[candidates[max]];
            max_ind_sets[leaf].insert(candidate);
            in_center[candidate] = false;
            center.erase(candidate);
            var_to_factor[candidate] = leaf;
            c_rm++;
        }
        cout << "Minimized center, variables removed: " << c_rm << endl;
        adapted_factoring = max_ind_sets;
    }
    else if (arg_minimize_center == 3 || arg_minimize_center == 4) {
        cout << "minimize center - experimental + merge leaves" << endl;
        // experimental + merge leaves
        // minimize center, by pushing center variables into leaves
        int c_rm = 0;
        int f_rm = 0;
        int c_rm_o = 0;
        int f_rm_o = 0;
        bool changed = true;
        bool removed_frozen_leaves = false;
        int times_minimized = 0;
        while (changed) {
            changed = false;
            times_minimized++;
            map<int, int> var_to_leaf;
            // extract center variables
            list<int> center;
            vector<bool> in_center(cg_g.size(), true);
            for (size_t i = 0; i < max_ind_sets.size(); i++) {
                for (set<int>::iterator it = max_ind_sets[i].begin(); it != max_ind_sets[i].end(); ++it) {
                    in_center[*it] = false;
                    var_to_leaf.insert(pair<int, int>(*it, i));
                }
            }
            // sort center variables increasingly by number of neighbours
            for (size_t i = 0; i < cg_g.size(); i++) {
                if (in_center[i]) {
                    if (center.empty() || cg_g.at(i).size() <= cg_g.at(center.front()).size()) {
                        center.push_front(i);
                    } else if (cg_g.at(i).size() >= cg_g.at(center.back()).size()) {
                        center.push_back(i);
                    } else {
                        for (list<int>::iterator s_it = center.begin(); s_it != center.end(); ++s_it) {
                            if (cg_g.at(i).size() <= cg_g.at(*s_it).size()) {
                                center.insert(s_it, i);
                                break;
                            }
                        }
                    }
                }
            }
            vector<bool> merged_leaf(max_ind_sets.size(), false);
            for (list<int>::iterator c = center.begin(); c != center.end(); ++c) {
                if (cg_g.at(*c).size() == 0) {
                    continue;
                } else if (cg_g.at(*c).size() == 1) {

                    set<int>::iterator n = cg_g.at(*c).begin();
                    if (!in_center[*n]) {
                        // add c to respective leaf
                        max_ind_sets[var_to_leaf[*n]].insert(*c);
                        in_center[*c] = false;
                        var_to_leaf.insert(pair<int, int>(*c, var_to_leaf[*n]));
                        c_rm++;
                    }
                } else {
                    // if all neighbours lie in the center we can skip this var
                    set<int> leaf_neighbours;
                    for (set<int>::iterator n = cg_g.at(*c).begin(); n != cg_g.at(*c).end(); ++n) {
                        if (!in_center[*n]) {
                            leaf_neighbours.insert(var_to_leaf[*n]);
                        }
                    }
                    if (leaf_neighbours.size() == 0) {
                        continue;
                    }
                    if (leaf_neighbours.size() == 1) {
                        int l = *leaf_neighbours.begin();
                        max_ind_sets[l].insert(*c);
                        in_center[*c] = false;
                        var_to_leaf.insert(pair<int, int>(*c, l));
                        c_rm++;
                    } else {
                        if (arg_merge_leaves == 2 || arg_merge_leaves == 3) {
                            set<int> temp;
                            int pos = max_ind_sets.size();
                            bool invalid_merge = false;
                            map<int, int> temp_map(var_to_leaf);
                            for (set<int>::iterator l = leaf_neighbours.begin(); l != leaf_neighbours.end(); ++l) {
                                if (arg_merge_leaves == 3 || (arg_merge_leaves == 2 && !merged_leaf[*l])) {
                                    temp.insert(max_ind_sets[*l].begin(), max_ind_sets[*l].end());
                                    set<int> empty;
                                    max_ind_sets[*l] = empty;
                                    for (set<int>::iterator it = temp.begin(); it != temp.end(); ++it){
                                        temp_map[*it] = pos;
                                    }
                                }
                                else if (arg_merge_leaves == 2 && merged_leaf[*l]) {
                                    invalid_merge = true;
                                    break;
                                }
                            }
                            if (!invalid_merge) {
                                var_to_leaf = temp_map;
                                temp.insert(*c);
                                in_center[*c] = false;
                                var_to_leaf.insert(pair<int, int>(*c, pos));
                                c_rm++;
                                merged_leaf.push_back(true);
                                max_ind_sets.push_back(temp);
                            }
                        }
                    }
                }
            }
            for (vector<set<int>>::iterator it = max_ind_sets.begin(); it != max_ind_sets.end();) {
                if (it->empty()) {
                    it = max_ind_sets.erase(it);
                } else {
                    it++;
                }
            }
            cout << "Minimized center, variables removed: " << c_rm << endl;
#ifdef DEBUG_FACTORING
            for (size_t i = 0; i < max_ind_sets.size(); i++) {
                cout << "   (";
                set<int> factor = max_ind_sets[i];
                for (set<int>::iterator it = factor.begin(); it != factor.end(); it++) {
                    cout << *it << " ";
                }
                cout << ")" << endl;
            }
            cout << endl;
#endif
            if (!removed_frozen_leaves) {
                if (arg_merge_leaves == 1){
                    f_rm = 0;
                    for (const int &c: center) {
                        if (in_center[c]) {
                            // if all neighbours lie in the center we can skip this var
                            set<int> leaf_neighbours;
                            for (set<int>::iterator n = cg_g.at(c).begin(); n != cg_g.at(c).end(); ++n) {
                                if (!in_center[*n]) {
                                    leaf_neighbours.insert(var_to_leaf[*n]);
                                }
                            }
                            if (leaf_neighbours.empty()) {
                                continue;
                            }
                            set<int> temp;
                            int pos = max_ind_sets.size();
                            map<int, int> temp_map(var_to_leaf);
                            vector<bool> frozen = get_frozen_leaves(max_ind_sets);
                            int not_frozen = 0;
                            for (const int &l: leaf_neighbours) {
                                temp.insert(max_ind_sets[l].begin(), max_ind_sets[l].end());
                                set<int> empty;
                                max_ind_sets[l] = empty;
                                for (const int &it: temp) {
                                    temp_map[it] = pos;
                                }
                                if (!frozen[l]) {
                                    not_frozen++;
                                } else {
                                    f_rm++;
                                }
                            }
                            if (not_frozen <= 1) {
                                var_to_leaf = temp_map;
                                temp.insert(c);
                                in_center[c] = false;
                                var_to_leaf.insert(pair<int, int>(c, pos));
                                c_rm++;
                                merged_leaf.push_back(true);
                                max_ind_sets.push_back(temp);
                            }
                        }
                    }
                    cout << "merged " << f_rm << " frozen leaves" << endl;
#ifdef DEBUG_FACTORING
                    cout << "Factoring" << endl;
                    for (size_t i = 0; i < max_ind_sets.size(); i++) {
                        cout << "   (";
                        set<int> factor = max_ind_sets[i];
                        for (const int &it: factor) {
                            cout << it << " ";
                        }
                        cout << ")" << endl;
                    }
                    cout << endl;
#endif
                }
                f_rm = 0;
                // put frozen leaves back in center
                FactoredVars temp;
                vector<bool> frozen_leaves = get_frozen_leaves(max_ind_sets);
                for (size_t i = 0; i < frozen_leaves.size(); i++) {
                    if (!frozen_leaves[i]) {
                        temp.push_back(max_ind_sets[i]);
                    } else {
                        f_rm++;
                    }
                }
                max_ind_sets.clear();
                max_ind_sets.insert(max_ind_sets.begin(), temp.begin(), temp.end());
                if (f_rm != f_rm_o) {
                    changed = true;
                    f_rm_o = f_rm;
                }
                cout << "Removed frozen leaves: " << f_rm << endl;
#ifdef DEBUG_FACTORING
                cout << "Factoring" << endl;
                for (size_t i = 0; i < max_ind_sets.size(); i++) {
                    cout << "   (";
                    set<int> factor = max_ind_sets[i];
                    for (const int &it: factor) {
                        cout << it << " ";
                    }
                    cout << ")" << endl;
                }
                cout << endl;
#endif
            }

            if ((c_rm_o != c_rm) && arg_minimize_center == 4) {
#ifdef DEBUG_FACTORING
                cout << "removed center" << c_rm << "vars in round: " << times_minimized << endl;
#endif
                changed = true;
                c_rm_o = c_rm;
                c_rm = 0;
            } else if (changed && !(arg_minimize_center == 4)) {
                // only count removing frozen leaves as a change, if iterativ minimize_center is active
                changed = false;
            }
        }
#ifdef DEBUG_FACTORING
        cout << "do not repeat arg_minimize_center, round: " << times_minimized << endl;
#endif
        adapted_factoring = max_ind_sets;
    }

    // if center is empty, use biggest leaf as center
    size_t leaf_vars = 0;
    for (FactoredVars::iterator factor = adapted_factoring.begin(); factor != adapted_factoring.end(); ++factor){
        for (set<int>::iterator var = factor->begin(); var != factor->end(); ++var) {
            leaf_vars++;
        }
    }
    if (leaf_vars == g_variable_domain.size()) {
        FactoredVars::iterator max = adapted_factoring.begin();
        if (adapted_factoring.size() > 1) {
            for (FactoredVars::iterator i = adapted_factoring.begin(); i != adapted_factoring.end(); i++) {
                if (i->size() > max->size()) {
                    max = i;
                }
            }
        }
        adapted_factoring.erase(max);
        cout << "Repopulated center: 1" << endl;
    } else {
        cout << "Repopulated center: 0" << endl;
    }

    return adapted_factoring;
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

Factoring::FactoredVars DocTransferFactoring::get_factoring() {
    FactoredVars factoring;
    for (size_t var = 0; var < g_variable_domain.size(); ++var){
        if (strncmp(g_fact_names[var][0].c_str(), "Atom at(document", 16) == 0 || strncmp(g_fact_names[var][0].c_str(), "Atom at(fuel", 12) == 0){
            set<int> tmp;
            tmp.insert(var);
            factoring.push_back(tmp);
        }
    }
    return factoring;
}

Factoring::FactoredVars PegsolFactoring::get_factoring() {
    FactoredVars factoring(4);
    vector<vector<string> > factors(4, vector<string>(7));
    factors[0][0] = "Atom free(pos-0-2)";factors[0][1] = "Atom free(pos-0-3)";factors[0][2] = "Atom free(pos-0-4)";factors[0][3] = "Atom free(pos-1-2)";factors[0][4] = "Atom free(pos-1-3)";factors[0][5] = "Atom free(pos-1-4)";//factors[0][6] = "Atom free(pos-2-3)";
    factors[1][0] = "Atom free(pos-2-0)";factors[1][1] = "Atom free(pos-3-0)";factors[1][2] = "Atom free(pos-4-0)";factors[1][3] = "Atom free(pos-2-1)";factors[1][4] = "Atom free(pos-3-1)";factors[1][5] = "Atom free(pos-4-1)";//factors[1][6] = "Atom free(pos-3-2)";
    factors[2][0] = "Atom free(pos-5-2)";factors[2][1] = "Atom free(pos-5-3)";factors[2][2] = "Atom free(pos-5-4)";factors[2][3] = "Atom free(pos-6-2)";factors[2][4] = "Atom free(pos-6-3)";factors[2][5] = "Atom free(pos-6-4)";//factors[2][6] = "Atom free(pos-4-3)";
    factors[3][0] = "Atom free(pos-2-5)";factors[3][1] = "Atom free(pos-3-5)";factors[3][2] = "Atom free(pos-4-5)";factors[3][3] = "Atom free(pos-2-6)";factors[3][4] = "Atom free(pos-3-6)";factors[3][5] = "Atom free(pos-4-6)";//factors[3][6] = "Atom free(pos-3-4)";
    
    for (size_t var = 0; var < g_variable_domain.size(); ++var){
        for (size_t factor = 0; factor < factors.size(); ++factor){
            vector<string>::const_iterator it = find(factors[factor].begin(), factors[factor].end(), g_fact_names[var][0].c_str());
            if (it != factors[factor].end()){
                factoring[factor].insert(var);
                break;
            }
        }
    }
    return factoring;
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
    search_types.push_back("unsat");
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
    parser.add_option<int>(
        "build_state_space_size",
        "TODO",
        "10000");
    parser.add_option<bool>(
        "no_search",
        "TODO",
        "false");
}

void ForkFactoring::add_options_to_parser(OptionParser& parser) {
    // TODO add docu
    parser.add_option<bool>(
        "maximize_mobility",
        "TODO",
        "false");
    parser.add_option<bool>(
        "average_mobility",
        "TODO",
        "false");
    parser.add_option<bool>(
        "extend_leaves",
        "TODO",
        "false"
    );
}

void MaxIndependentSetFactoring::add_options_to_parser(OptionParser& parser) {
    // TODO add docu
    parser.add_option<int>(
            "minimize_center",
            "TODO",
            "0"
    );
    parser.add_option<bool>(
            "minimize_frozen",
            "TODO",
            "false"
    );
    parser.add_option<int>(
            "xth_fact",
            "TODO",
            "0"
    );
    parser.add_option<int>(
            "merge_leaves",
            "TODO",
            "0"
    );
    parser.add_option<int>(
            "find_all",
            "TODO",
            "0"
    );
    vector<string> mis_algos;
    mis_algos.push_back("a1");
    mis_algos.push_back("a2");
    mis_algos.push_back("dfs");

    parser.add_enum_option(
            "mis_algo",
            mis_algos,
            "TODO",
            "a2",
            vector<string>(),
            OptionFlags(false));

    vector<string> feature;
    feature.push_back("mobility");
    feature.push_back("leaves");
    feature.push_back("none");

    parser.add_enum_option(
            "feature_1",
            feature,
            "TODO",
            "mobility",
            vector<string>(),
            OptionFlags(false));
    parser.add_enum_option(
            "feature_2",
            feature,
            "TODO",
            "none",
            vector<string>(),
            OptionFlags(false));
    parser.add_option<bool>(
            "all_rankings",
            "TODO",
            "false"
    );
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
        factoring = new ForkFactoring(opts, I_FORK);
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

    IncidentArcsFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new IncidentArcsFactoring(opts);
    }

    return factoring;
}

static Factoring *_parse_maxindsets(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    MaxIndependentSetFactoring::add_options_to_parser(parser);

    Options opts = parser.parse();

    MaxIndependentSetFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new MaxIndependentSetFactoring(opts);
    }

    return factoring;
}

static Factoring *_parse_doctransfer(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    
    Options opts = parser.parse();

    DocTransferFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new DocTransferFactoring(opts);
    }

    return factoring;
}

static Factoring *_parse_pegsol(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    
    Options opts = parser.parse();

    PegsolFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new PegsolFactoring(opts);
    }

    return factoring;
}

static Plugin<Factoring> _plugin_fork("fork", _parse_fork);
static Plugin<Factoring> _plugin_ifork("ifork", _parse_ifork);
static Plugin<Factoring> _plugin_xshape("xshape", _parse_xshape);
static Plugin<Factoring> _plugin_incarcs("incarcs", _parse_incarcs);
static Plugin<Factoring> _plugin_maxindsets("maxindsets", _parse_maxindsets);
// static Plugin<Factoring> _plugin_rand("rand", _parse_rand);
// static Plugin<Factoring> _plugin_sokoban("sokoban", _parse_sokoban);
// static Plugin<Factoring> _plugin_tpp_const("tpp_const", _parse_tpp_const);
// static Plugin<Factoring> _plugin_itpp_const("itpp_const", _parse_itpp_const);

static Plugin<Factoring> _plugin_doctransfer("doctransfer", _parse_doctransfer);
static Plugin<Factoring> _plugin_pegsol("pegsol", _parse_pegsol);

