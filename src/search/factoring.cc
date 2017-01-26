#include "factoring.h" 

#include "causal_graph.h"
#include "compliant_paths/compliant_path_graph.h"
#include "compliant_paths/cpg_storage.h"
#include "globals.h"
#include "mis.h"
#include "operator.h"
#include "option_parser.h"
#include "plugin.h"
#include "../preprocess/scc.h"
#include "pruning/pruning_options.h"
#include "ranking.h"
#include "search_engine.h"
#include "state_registry.h"
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

// TODO refine command line option for abstaining


void InteractionGraph::add_dependency(LeafFactorID from_, LeafFactorID to_) {
    LeafFactorID from(from_ + 1);
    if (from_ == LeafFactorID::CENTER){
        from = LeafFactorID(0);
    }
    LeafFactorID to(to_ + 1);
    if (to_ == LeafFactorID::CENTER){
        to = LeafFactorID(0);
    }
    if (find(successors[from].begin(), successors[from].end(), to) == successors[from].end()){
        successors[from].push_back(to);
    }
    if (find(predecessors[to].begin(), predecessors[to].end(), from) == predecessors[to].end()){
        predecessors[to].push_back(from);
    }
}


Factoring::Factoring() {
    max_leaf_size = numeric_limits<size_t>::max();
    min_number_leaves = 2;
    max_precompute_state_space_size = 10000;
}

Factoring::Factoring(const Options &opts) :
            search_type(SEARCH_TYPE(opts.get_enum("search_type"))) {
    switch(search_type){
        case FRA : cout << "performing Fork-Root A* search" << endl; break;
        case ASDA : cout << "performing Anytime Star Decoupled A* search" << endl; break;
        case SDA : cout << "performing Star Decoupled A* search" << endl; break;
        case SAT : cout << "performing decoupled satisficing search" << endl; break;
        case UNSAT : cout << "trying to prove task unsolvable" << endl; break;
        case NO_SEARCH : cout << "not running actual search" << endl; break;
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
        CompliantPathGraph::set_pruning_options(opts.get<PruningOptions*>("pruning"));
    } else {
        CompliantPathGraph::set_pruning_options(new PruningOptions());
        cout << "using default pruning method: BasicPruning" << endl;
    }
    min_number_leaves = 2;
    max_precompute_state_space_size = opts.get<int>("build_state_space_size");
}

class AutoFactoring : public Factoring {
    
    int argc;
    
    const char** argv;
    
    vector<Factoring*> factoring_methods;
    
    int max_test_search_time;
    
    mutable vector<pair<int, int> > safed_goal;
    
    mutable SuccessorGenerator *safed_successor_generator;
    
    
    void reset_globals() const;
    
    vector<size_t> run_and_rank_search() const;
    
protected:
    
    FactoredVars get_factoring() override;
    
public:
    
    AutoFactoring(const Options &opts);
    
    virtual void set_main_input(int argc, const char** argv) {
        this->argc = argc;
        this->argv = argv;
    }
    
};

class CenterFactoring : public Factoring {
    
protected:
    
    FactoredVars get_factoring() override;
    
public:
    
    CenterFactoring(const Options &opts)
            : Factoring(opts) {
        profile = STAR;
        cout << "using random 2-factor factoring profile" << endl;
    }
    
};

class ForkFactoring : public Factoring {
    friend class AutoFactoring;
    
protected:
    
    ForkFactoring(PROFILE profile = FORK) {
        maximize_mobility = false;
        extend_leaves = false;
        average_mobility = false;
        if (profile == I_FORK){
            Factoring::profile = I_FORK;
            cout << "using inverted-fork factoring profile" << endl;
        } else if (profile == FORK){
            Factoring::profile = FORK;
            cout << "using fork factoring profile" << endl;
        }
    }
    
    bool maximize_mobility;
    
    bool extend_leaves;
    
    bool average_mobility;
    
    FactoredVars get_fork_maximize_leaves(bool inverted = false, std::vector<int> variables = std::vector<int>()) const;
    
    FactoredVars get_fork_maximize_mobility(bool inverted = false, std::vector<int> variables = std::vector<int>()) const;
    
    FactoredVars get_factoring() override;
    
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
    }
    
    static void add_options_to_parser(OptionParser &parser);
    
};

class XShapeFactoring : public ForkFactoring {
    friend class AutoFactoring;
    
    XShapeFactoring()
            : ForkFactoring(X_SHAPE) {
        profile = X_SHAPE;
        cout << "using X-shape factoring profile" << endl;
    }
        
protected:
    
    FactoredVars get_factoring() override;
    
public:
    
    XShapeFactoring(const Options &opts)
            : ForkFactoring(opts, X_SHAPE) {
        profile = X_SHAPE;
        cout << "using X-shape factoring profile" << endl;
    }
    
};

class IncidentArcsFactoring : public Factoring {
    friend class AutoFactoring;
    
    IncidentArcsFactoring()
            : Factoring() {
        profile = STAR;
        cout << "using incident-arcs factoring profile" << endl;
    }
    
protected:
    
    FactoredVars get_factoring() override;
  
public:
    
    IncidentArcsFactoring(const Options &opts)
            : Factoring(opts) {
        profile = STAR;
        cout << "using incident-arcs factoring profile" << endl;
    }
  
};

class MaxIndependentSetFactoring : public Factoring {
    friend class AutoFactoring;
    
    vector<FactoredVars> get_factorings(const vector<vector<Ranking*> > &rankings);
    
    MaxIndependentSetFactoring()
            : Factoring(),
              arg_minimize_center(1),
              arg_minimize_frozen(false),
              arg_xth_factoring(0),
              arg_merge_leaves(0),
              arg_find_all(0),
              arg_all_rankings(true),
              arg_timeout(30),
              arg_multiple_mis(1){
        profile = STAR;
        cout << "using maximum independet set factoring profile - find all MIS: "
             << arg_find_all << " minimize_center " << arg_minimize_center << " all rankings: " << arg_all_rankings
             << " minimize frozen " << arg_minimize_frozen << " select xth factoring: " << arg_xth_factoring
             << " merge leaves level: " << arg_merge_leaves;
    }

protected:
    int arg_minimize_center;
    bool arg_minimize_frozen;
    vector<Ranking* > arg_rankings;
    int arg_xth_factoring = 0;
    int arg_merge_leaves = 0;
    int arg_find_all;
    bool arg_all_rankings;
    int arg_timeout;
    int arg_multiple_mis;
    NGraph cg_g; // cache used for AutoFactoring
    vector<FactoredVars> max_ind_sets; // cache used for AutoFactoring

    
    void optimize_feature(FactoredVars &, const NGraph &,int &, int &, int &) const;
    
    //int optimize_experimental(const NGraph &, const set<int> &, const vector<bool> &, const map<int,int> &);
    
    NGraph generate_ngraph() const;
    
    bool check_factoring(const FactoredVars &) const;
    
    FactoredVars get_factoring() override;

public:

    MaxIndependentSetFactoring(const Options &opts)
            : Factoring(opts),
                arg_minimize_center(opts.get<int>("minimize_center")),
                arg_minimize_frozen(opts.get<bool>("minimize_frozen")),
                arg_xth_factoring(opts.get<int>("xth_fact")),
                arg_merge_leaves(opts.get<int>("merge_leaves")),
                arg_find_all(opts.get<int>("find_all")),
                arg_all_rankings(opts.get<bool>("all_rankings")),
                arg_timeout(opts.get<int>("timeout")),
                arg_multiple_mis(opts.get<int>("multiple_mis")){
        profile = STAR;
        cout << "using maximum independet set factoring profile - find all MIS: "
             << arg_find_all << " minimize_center " << arg_minimize_center << " all rankings: " << arg_all_rankings
             << " minimize frozen " << arg_minimize_frozen << " select xth factoring: " << arg_xth_factoring
             << " merge leaves level: " << arg_merge_leaves;
        cout << " ranking_features: ";
        if (!opts.contains("rankings")) {
            cout << "None" << endl;
        } else {
            arg_rankings = opts.get_list<Ranking *>("rankings");
            for (Ranking *ranking : arg_rankings) {
                cout << ranking->get_feature() << " ";
            }
            cout << endl;
        }
    }
    

    static void add_options_to_parser(OptionParser &parser);

};

class SokobanFactoring : public Factoring {
            
protected:
    
    FactoredVars get_factoring() override;
  
public:
    
    SokobanFactoring(const Options &opts)
            : Factoring(opts) {
        profile = SOKOBAN;
        cout << "using sokoban factoring profile" << endl;
    }
  
};

class OpenstacksFactoring : public Factoring {
            
protected:
    
    FactoredVars get_factoring() override;
  
public:
    
    OpenstacksFactoring(const Options &opts)
            : Factoring(opts) {
        profile = OPENSTACKS;
        cout << "using openstacks factoring profile" << endl;
    }
    
};

class TPPConstrainedFactoring : public Factoring {
    
    bool inverted;
            
protected:
    
    FactoredVars get_factoring() override;
  
public:
    
    TPPConstrainedFactoring(const Options &opts)
            : Factoring(opts), inverted(opts.get<bool>("inverted")) {
        profile = OPENSTACKS;
        cout << "using " <<  (inverted ? "inverted-" : "") << "tpp factoring profile" << endl;
    }
  
};

class DocTransferFactoring : public Factoring {
            
protected:
    
    FactoredVars get_factoring() override;
  
public:
    
    DocTransferFactoring(const Options &opts)
            : Factoring(opts) {
        profile = DOC_TRANSFER;
        cout << "using doc transfer factoring profile" << endl;
    }

};

class PegsolFactoring : public Factoring {
            
protected:
    
    FactoredVars get_factoring() override;
  
public:
    
    PegsolFactoring(const Options &opts)
            : Factoring(opts) {
        profile = PEGSOL;
        cout << "using pegsol factoring profile" << endl;
    }
    
};



void Factoring::apply_factoring(const FactoredVars &factoring) {
    g_belongs_to_factor = vector<LeafFactorID>(g_variable_domain.size(), LeafFactorID::CENTER);
    g_new_index = vector<size_t>(g_variable_domain.size());
    g_leaves.resize(factoring.size());
    fork_leaf.resize(factoring.size(), false);
    ifork_leaf.resize(factoring.size(), false);
    
    InteractionGraph interaction_graph(factoring.size());
    
    for (LeafFactorID factor(0); factor < factoring.size(); ++factor){
        vector<int> variables(factoring[factor].begin(), factoring[factor].end());              
        for (size_t j = 0; j < variables.size(); ++j){
            g_belongs_to_factor[variables[j]] = factor;
            g_new_index[variables[j]] = j;
#ifdef DEBUG_FACTORING
            cout << "   var: " << variables[j] << " - " << g_fact_names[variables[j]][0] << endl;
#endif
        }
        g_leaves[factor] = variables;
    }
    
#ifdef DEBUG_FACTORING
    cout << "center factor:" << endl;
#endif
    
    for (size_t i = 0; i < g_variable_domain.size(); ++i){
        LeafFactorID affected_factor = g_belongs_to_factor[i];
        if (affected_factor == LeafFactorID::CENTER){
            g_center.push_back(i);
            g_new_index[i] = g_center.size() - 1;
#ifdef DEBUG_FACTORING
            cout << "   variable " << i << " :" << g_fact_names[i][0] << endl;
#endif
        }
    }
   
    // assign operators to their factors
#ifdef DEBUG_FACTORING
    vector<int> num_ops_per_factor(g_leaves.size());
#endif
    bool is_fork_factoring = true;
    bool is_ifork_factoring = true;
    for (Operator &op : g_operators){
        
        set<LeafFactorID> pre_factors;
        set<LeafFactorID> eff_factors;

        for (const Condition &pre : op.get_preconditions()){            
            pre_factors.insert(g_belongs_to_factor[pre.var]);
        }
        
        for (const Effect &eff : op.get_effects()){
            eff_factors.insert(g_belongs_to_factor[eff.var]);
        }
        assert(!eff_factors.empty());
        
        for (LeafFactorID pre_factor : pre_factors){
            for (LeafFactorID eff_factor : eff_factors){
                if (pre_factor != eff_factor){
                    interaction_graph.add_dependency(pre_factor, eff_factor);
                }
            }
        }
        for (LeafFactorID eff1_factor : eff_factors){
            for (LeafFactorID eff2_factor : eff_factors){
                if (eff1_factor != eff2_factor){
                    interaction_graph.add_dependency(eff1_factor, eff2_factor);
                }
            }
        }

        bool is_center_op = false;
        if (eff_factors.find(LeafFactorID::CENTER) != eff_factors.end()){
            // effect on center variable
            is_center_op = true;
        } else if (eff_factors.size() > 1){
            // effect on more than one factor
            is_center_op = true;
        } else {
            bool removed = pre_factors.erase(LeafFactorID::CENTER) > 0;
            if (pre_factors.size() > 1){
                // precondition on more than one leaf
                is_center_op = true;
            } else if (!pre_factors.empty() && *pre_factors.begin() != *eff_factors.begin()){
                // precondition on leaf A, but effect on leaf B
                is_center_op = true;
            }
            if (removed){
                // needed below
                pre_factors.insert(LeafFactorID::CENTER);
            }
        }
        if (!is_center_op){
            LeafFactorID factor = *eff_factors.begin();
#ifdef DEBUG_FACTORING
            num_ops_per_factor[factor]++;
#endif
            if (pre_factors.find(LeafFactorID::CENTER) != pre_factors.end()){
                // leaf action has center precondition
                is_ifork_factoring = false;
            }
            op.set_affected_factor(factor);
        } else {
            pre_factors.erase(LeafFactorID::CENTER);
            if (eff_factors.size() > 1){
                // center action has leaf effect
                is_fork_factoring = false;
                is_ifork_factoring = false;
            }
            if (!pre_factors.empty()){
                // center action has precondition on leaf
                is_fork_factoring = false;
            }
            op.set_affected_factor(LeafFactorID::CENTER);
        }
    }

    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        if (interaction_graph.get_successors(factor).empty()){
            fork_leaf[factor] = true;
        } else {
            fork_leaf[factor] = false;
            assert(!is_fork_factoring);
        }
        if (interaction_graph.get_predecessors(factor).empty()){
            ifork_leaf[factor] = true;
        } else {
            ifork_leaf[factor] = false;
            assert(!is_ifork_factoring);
        }
    }
    if (is_fork_factoring){
        profile = FORK;
        cout << "factoring type set to fork, no non-fork leaf factors" << endl;
    } else if (is_ifork_factoring){
        profile = I_FORK;
        cout << "factoring type set to inverted-fork, no non-inverted-fork leaf factors" << endl;
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
    g_goals_per_factor = vector<vector<pair<int, int> > >(g_leaves.size());
    vector<int> indeces_to_remove;
    for (size_t g = 0; g < g_goal.size(); ++g){
        LeafFactorID affected_factor = g_belongs_to_factor[g_goal[g].first];
        if (affected_factor != LeafFactorID::CENTER){
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

    vector<bool> precompute_leaf_state_space(g_leaves.size(), true);
    
    for (size_t i = 0; i < g_leaves.size(); ++i){
        uint64_t f_size = 1;
        for (size_t j = 0; j < g_leaves[i].size(); ++j){
            f_size *= g_variable_domain[g_leaves[i][j]];
            if ((search_type == SAT || search_type == UNSAT) && f_size > max_precompute_state_space_size){
                precompute_leaf_state_space[i] = false;
            }
        }
    }
    CompliantPathGraph::set_precompute_leaf_state_spaces(precompute_leaf_state_space);
}

void Factoring::print_factoring() const {
#ifdef DEBUG_FACTORING
    cout << "final factoring:" << endl;
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        cout << "leaf factor " << factor << endl;
        for (size_t var_i = 0; var_i < g_leaves[factor].size(); ++var_i){
            cout << "   " << g_fact_names[g_leaves[factor][var_i]][0] << endl;
        }
    }
#endif
    
    cout << "number leaf factors: " << g_leaves.size() << endl;
    
    cout << g_center.size() << " variables in center factor" << endl;
    
    for (size_t i = 0; i < g_leaves.size(); ++i) {
        cout << g_leaves[i].size() << " variables in leaf factor " << i << endl;
    }
    
    uint64_t min_size = numeric_limits<uint64_t>::max();
    double avg_size = 0;
    uint64_t max_size = 0;
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        uint64_t f_size = 1;
        for (size_t j = 0; j < g_leaves[factor].size(); ++j){
            f_size *= g_variable_domain[g_leaves[factor][j]];
        }
        avg_size += f_size;
        min_size = min(min_size, f_size);
        max_size = max(max_size, f_size);
    }
    
    cout << "min leaf factor size: " << min_size << endl;
    cout << "average leaf factor size: " << (avg_size / g_leaves.size()) << endl;
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
    assert(best_factoring.size() < (size_t) LeafFactorID::CENTER - 1);
    
    cout << "factoring time " << t << endl;
    
    if (best_factoring.size() < min_number_leaves){
        exit_error("No factoring found!", EXIT_ABSTAINED);
    }
    
    apply_factoring(best_factoring);    
    
    print_factoring();
    
    if (search_type == NO_SEARCH) {
        cout << "Factoring complete, do not perform search" << endl;
        exit_with(EXIT_NO_SEARCH);
    }
}

AutoFactoring::AutoFactoring(const Options &opts) : Factoring(opts), 
                                                    max_test_search_time(opts.get<int>("max_search_time")) {
    profile = STAR;
    cout << "using self-configuring factoring" << endl;
    if (opts.contains("factorings")){
        factoring_methods = opts.get_list<Factoring*>("factorings"); 
    } else {
        cout << "no factorings provided, using default settings" << endl;
        factoring_methods.push_back(new ForkFactoring());
        factoring_methods.push_back(new ForkFactoring(I_FORK));
        factoring_methods.push_back(new IncidentArcsFactoring());
        factoring_methods.push_back(new MaxIndependentSetFactoring());
    }
}

Factoring::FactoredVars AutoFactoring::get_factoring() {
    safed_goal = g_goal;
    safed_successor_generator = g_successor_generator->copy();
    
    vector<FactoredVars> factorings;
    
    for (const auto factoring_method : factoring_methods){
        
        MaxIndependentSetFactoring *mis_factoring_method = dynamic_cast<MaxIndependentSetFactoring*>(factoring_method);
        if (mis_factoring_method != nullptr){
            // is MIS factoring
            vector<vector<Ranking*> > rankings;
            vector<Ranking*> tmp;
            tmp.push_back(new LeavesRanking());
            tmp.push_back(new MobilityRanking());
            rankings.push_back(tmp);
            
            vector<FactoredVars> mis_factorings =  mis_factoring_method->get_factorings(rankings);
            for (const auto &factoring : mis_factorings){
                if (factoring.size() >= min_number_leaves){
                    factorings.push_back(factoring);
                }
            }
        } else {
            FactoredVars factoring = factoring_method->get_factoring();
            if (factoring.size() >= min_number_leaves){
                factorings.push_back(factoring);
            }
        }
    }
    
    cout << endl << "Number factoring candidates: " << factorings.size() << endl << endl;
    
    FactoredVars best_factoring;
    vector<vector<size_t>> all_rankings;
    for (const auto &factoring : factorings){
        cout << "Running short search for " << max_test_search_time << "s to test factoring(" << factoring.size() << " leaves)." << endl;
    
        g_factoring->apply_factoring(factoring);
        
        vector<size_t> ranking = run_and_rank_search();
        all_rankings.push_back(ranking);

        reset_globals();

        cout << endl << endl;
    }

    if (!factorings.empty()) {
        int best_ranking = 0;
        int best_count = 0;
        for (size_t r1 = 0; r1 < all_rankings.size(); ++r1) {
            int better = 0;
            for (size_t r2 = 0; r2 < all_rankings.size(); ++r2) {
                if (r1 != r2) {
                    for (size_t i = 0; i < all_rankings[r1].size(); ++i) {
                        if (all_rankings[r1][i] >= all_rankings[r2][i]) {
                            better++;
                        }
                    }
                }
            }
            if (better > best_count) {
                best_ranking = r1;
                best_count = better;
            }
        }
        best_factoring = factorings[best_ranking];
    }
    
    return best_factoring;
}

vector<size_t> AutoFactoring::run_and_rank_search() const {
    
    cout << flush;
    streambuf *real_out = cout.rdbuf();
    ostringstream tmp_out;
    cout.rdbuf(tmp_out.rdbuf());
    
    print_factoring();
    
    if (g_factoring){        
        vector<int> center_domains;
        vector<vector<int> > leaf_domains(g_leaves.size());
        for (size_t var = 0; var < g_belongs_to_factor.size(); var++){
            LeafFactorID factor = g_belongs_to_factor[var];
            if (factor == LeafFactorID::CENTER){
                center_domains.push_back(g_variable_domain[var]);
            } else {
                if (leaf_domains[factor].empty()){
                    leaf_domains[factor].resize(g_leaves[factor].size(), -1);
                }
                leaf_domains[factor][g_new_index[var]] = g_variable_domain[var];
            }
        }
        
        center_domains.push_back(MAX_DUPLICATE_COUNTER);
        
        cout << "packing state variables..." << flush;
        g_state_packer = new IntPacker(center_domains);
        cout << "Variables: " << g_center.size() << endl;
        g_leaf_factor_packers = vector<IntPacker*>(g_leaves.size(), 0);
        for (size_t i = 0; i < g_leaf_factor_packers.size(); i++){
            g_leaf_factor_packers[i] = new IntPacker(leaf_domains[i]);
        }
    } else {
	g_state_packer = new IntPacker(g_variable_domain);
	g_leaf_factor_packers.push_back(new IntPacker(vector<int>(1, 2)));
	cout << "Variables: " << g_variable_domain.size() << endl;
    }
    
    cout << "Bytes per state: "
         << g_state_packer->get_num_bins() *
            g_state_packer->get_bin_size_in_bytes() << endl;
    
    if (g_factoring){
	for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
	    cout << "Bytes per state of leaf factor " << factor << ": "
		<< g_leaf_factor_packers[factor]->get_num_bins() *
		    g_leaf_factor_packers[factor]->get_bin_size_in_bytes() << endl;
	}
    }
    
    cout << "done! [t=" << g_timer << "]" << endl;

    // NOTE: state registry stores the sizes of the state, so must be
    // built after the problem has been read in.
    g_state_registry = new StateRegistry();

    cout << "done initalizing global data [t=" << g_timer << "]" << endl;
    
    SearchEngine *engine = 0;

    // The command line is parsed twice: once in dry-run mode, to
    // check for simple input errors, and then in normal mode.
    bool unit_cost = is_unit_cost();
    try {
        OptionParser::parse_cmd_line(argc, argv, true, unit_cost);
        engine = OptionParser::parse_cmd_line(argc, argv, false, unit_cost);
    } catch (ArgError &error) {
        cerr << error << endl;
        OptionParser::usage(argv[0]);
        exit_with(EXIT_INPUT_ERROR);
    } catch (ParseError &error) {
        cerr << error << endl;
        exit_with(EXIT_INPUT_ERROR);
    }

    engine->set_max_time(max_test_search_time);
    
    Timer search_timer;
    engine->search();
    search_timer.stop();
    
    cout.rdbuf(real_out);

    vector<size_t> ranking;
    size_t dec_state_space_size = g_state_registry->get_decoupled_state_space_size();
    size_t heuristic_value_improv = engine->search_progress.best_heuristic_values[0] / engine->search_progress.initial_h_values[0];
    size_t exp_states = engine->search_progress.expanded_states;
    size_t eval_states = engine->search_progress.evaluated_states;
    ranking.push_back(dec_state_space_size);
    ranking.push_back(heuristic_value_improv);
    ranking.push_back(exp_states);
    ranking.push_back(eval_states);

    if (engine->found_solution()){
        g_timer.stop();
        
        cout << tmp_out.str();

        engine->save_plan_if_necessary();
        engine->statistics();
        engine->heuristic_statistics();
        cout << "Search time: " << search_timer << endl;
        cout << "Total time: " << g_timer << endl;

        exit_with(EXIT_PLAN_FOUND);
    } else {
        // TODO generate some reasonable output
        cout << "did not find goal in test search" << endl;
        cout << "decoupled state space size: " << dec_state_space_size << endl;
        cout << "improved heuristic value by: " << heuristic_value_improv << endl;
        cout << "expanded states: " << exp_states << endl;
        cout << "evaluated states: " << eval_states << endl;
    }
    cout << "max number duplicates: " << g_state_registry->get_max_number_duplicates() << endl;

    delete engine;
    
    return ranking; // TODO add more features
}

void AutoFactoring::reset_globals() const {
    delete g_successor_generator;    
    g_successor_generator = safed_successor_generator->copy();
    
    for (auto l_succ_gen : g_leaf_successor_generators){
        delete l_succ_gen;
    }
    g_leaf_successor_generators.clear();
    
    delete g_state_packer;
    for (auto l_packers : g_leaf_factor_packers){
        delete l_packers;
    }
    g_leaf_factor_packers.clear();
    
    delete g_state_registry;
    
    delete CPGStorage::storage;
    CompliantPathGraph::clear_static_data();
    
    g_goal = safed_goal;
    
    g_belongs_to_factor.clear();
    g_new_index.clear();
    g_center.clear();
    g_leaves.clear();
    
    g_min_action_cost_per_factor.clear();
    g_goals_per_factor.clear();
    g_min_goal_cost.clear();
    g_sum_min_goal_cost = 0;
}

Factoring::FactoredVars XShapeFactoring::get_factoring() {    ///////////////////////////  X-SHAPE
    
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

Factoring::FactoredVars CenterFactoring::get_factoring() { //////////////////////////////  CENTER
    // TODO implement factoring
    FactoredVars best_factoring(1);

    srand(time(NULL));
    
    uint64_t current_size = 1;
    
    for (size_t var = 0; var < g_variable_domain.size(); var++){
        if (rand() % 2 == 0){
            best_factoring[0].insert(var);
	    current_size *= g_variable_domain[var];
        }
        if (max_leaf_size != 0 && current_size > max_leaf_size){
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

Factoring::FactoredVars ForkFactoring::get_factoring() { ////////////////////////////////  (INVERTED) FORK
    
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

Factoring::FactoredVars IncidentArcsFactoring::get_factoring() {  ///////////////////// INCIDENT ARCS
    
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
    size_t best_number_leaves = 0;
    
    vector<bool> discarded_leaves;
    
    set<int> leaf_vars;
    for (size_t var = 0; var < g_variable_domain.size(); ++var){
        leaf_vars.insert(var);
    }
    
    for (int arcs = variables.size() -1 ; arcs >= 0; --arcs){
        for (size_t cand = 0; cand < variables[arcs].size(); ++cand){
            //center.push_back(variables[arcs][cand]);
            
            leaf_vars.erase(variables[arcs][cand]);            
            
            FactoredVars factoring = get_connected_components(vector<int>(leaf_vars.begin(), leaf_vars.end()));
            
            vector<bool> discard_leaf = get_frozen_leaves(factoring);
            
            size_t number_leaves = factoring.size();
            for (size_t i = 0; i < factoring.size(); ++i){
                if (discard_leaf[i]){
                    --number_leaves;
                    continue;
                }
                uint64_t size = 1;
                for (int var : factoring[i]){
                    size *= g_variable_domain[var];
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
            if (max_leaf_size == 0 || current_size <= max_leaf_size){
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
                    if (max_leaf_size != 0 && new_size > max_leaf_size){
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
            if (max_leaf_size == 0 || current_size <= max_leaf_size){
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

Factoring::FactoredVars Factoring::get_connected_components(const vector<int> &variables) const {
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

vector<bool> Factoring::get_frozen_leaves(const FactoredVars &components) const {
    vector<bool> is_frozen_factor(components.size(), true);
    vector<int> belongs_to_factor(g_variable_domain.size(), -1);
    size_t num_frozen = components.size();
    
    for (size_t i = 0; i < components.size(); i++){
        for (int var : components[i]){
            belongs_to_factor[var] = i;
        }
    }
    
    for (const Operator &op : g_operators){
        bool center = false;
        set<int> eff_factors;
        int affects_factor = -1;
        
        for (const Effect &eff : op.get_effects()){
            eff_factors.insert(belongs_to_factor[eff.var]);
            affects_factor = belongs_to_factor[eff.var];
            if (affects_factor == -1 || eff_factors.size() > 1){
                center = true;
                break;
            }
        }
        
        if (center){
            // op affects center variable or has effect on more than 1 factor
            continue;
        }
        if (!is_frozen_factor[affects_factor]){
            // factor is mobile, no need to continue
            continue;
        }
        
        set<int> pre_factors;
        for (const Condition &pre : op.get_preconditions()) {
            pre_factors.insert(belongs_to_factor[pre.var]);
        }
        pre_factors.erase(-1);
        
        if (pre_factors.size() <= 1  && (pre_factors.empty() || *pre_factors.begin() == *eff_factors.begin())){
            // op has only preconditions and effects on (at most) a single leaf factor
            if (is_frozen_factor[affects_factor]){
                --num_frozen;
                is_frozen_factor[affects_factor] = false;
                if (num_frozen == 0){
                    return is_frozen_factor;
                }
            }
        }
    }

    return is_frozen_factor;
}

Factoring::FactoredVars MaxIndependentSetFactoring::get_factoring() {
    if (cg_g.empty()) {
        cg_g = generate_ngraph();

        MIS mis = MIS();
        vector<set<int> > found_mis = mis.get_mis(cg_g, arg_xth_factoring, arg_find_all, arg_timeout, arg_multiple_mis);

        // generate factoring
        for (const set<int> &m : found_mis) {
            FactoredVars mis;
            for (set<int>::iterator it = m.begin(); it != m.end(); ++it) {
                set<int> s;
                s.insert(*it);
                mis.push_back(s);
            }
            max_ind_sets.push_back(mis);
        }
    }

    //max_ind_sets = minimize_center(max_ind_sets, cg_g);
    vector<int> c_rm(max_ind_sets.size(),0);
    vector<int> f_rm(max_ind_sets.size(),0);
    vector<int> f_c(max_ind_sets.size(),0);
    for (size_t i = 0; i < max_ind_sets.size(); ++i) {
        optimize_feature(max_ind_sets[i], cg_g, f_rm[i], c_rm[i], f_c[i]);
    }
    size_t max = 0;
    vector<VarInt> max_rank(arg_rankings.size(), VarInt(0));
    for (size_t i = 0; i < max_ind_sets.size(); ++i) {
        const FactoredVars &m = max_ind_sets[i];
        bool update = false;
        vector<VarInt> rankings;
        uint64_t n = 0;
        for (Ranking *ranking : arg_rankings) {
            VarInt vi(ranking->get_rank(m));
            rankings.push_back(vi);
            if (ranking->maximizes()) {
                if (rankings[n] > max_rank[n]) {
                    update = true;
                }
            } else {
                if (rankings[n] < max_rank[n]) {
                    update = true;
                }
            }
            ++n;
        }
        if (update) {
            assert(rankings.size() == arg_rankings.size());
            max = i;
            max_rank = rankings;
        }
    }
    FactoredVars final_factoring = max_ind_sets[max];

#ifdef DEBUG_FACTORING
    cout << "optimized MIS: " << endl;
    for(size_t i = 0; i < final_factoring.size(); i++){
        cout << "(";
        for (set<int>::iterator it = final_factoring[i].begin(); it != final_factoring[i].end(); ++it) {
            cout << *it << " ";
        }
        cout << ")" << endl;
    }
#endif
    cout << "Minimized center, variables removed: " << c_rm[max] << endl;
    cout << "Frozen candidates: " << f_c[max] << endl;

    if (!arg_rankings.empty()) {
        cout << "Ranking of Factoring: ";
        for (size_t i = 0; i < arg_rankings.size(); ++i){
            cout << "   ";
            arg_rankings[i]->get_rank(final_factoring).dump_number();
        }
        cout << endl;
    }
    if (arg_all_rankings) {
        cout << "All Rankings: " << endl;

        print_all_rankings(final_factoring);
        cout << endl;
    }

    // push frozen leaves back in center
    int f_rm_max = f_rm[max];

    vector<bool> frozen_leaves = get_frozen_leaves(final_factoring);
    for (int i = frozen_leaves.size() - 1; i >= 0; --i) {
        if (frozen_leaves[i]) {
            final_factoring.erase(final_factoring.begin() + i);
            f_rm_max++;
        }
    }

    cout << "Removed frozen leaves: " << f_rm_max << endl;

    cout << "Number of factors: " << final_factoring.size() << endl;

    bool valid = check_factoring(final_factoring);
    if (!valid) {
        exit_with(EXIT_ABSTAINED);
    }

    return final_factoring;
}

vector<Factoring::FactoredVars> MaxIndependentSetFactoring::get_factorings(const vector<vector<Ranking*>> &rankings) {
    vector<FactoredVars> factorings;
    for (const vector<Ranking*> &ranking: rankings) {
        arg_rankings = ranking;
        factorings.push_back(this->get_factoring());
    }
    return factorings;
}

NGraph MaxIndependentSetFactoring::generate_ngraph() const {
    // build neighbour representation of causal graph (easier to modify)
    // graph of vars (var -> neighbours)
    NGraph cg_g(g_variable_domain.size(), set<int>());
    for (size_t var = 0; var < g_variable_domain.size(); ++var) {
        cg_g[var].insert(var);
        for (auto pre : g_causal_graph->get_predecessors(var)) {
            cg_g[var].insert(pre);
        }
        for (auto succ : g_causal_graph->get_successors(var)) {
            cg_g[var].insert(succ);
        }
    }
    return cg_g;
}

bool MaxIndependentSetFactoring::check_factoring(const FactoredVars &f) const {
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

void MaxIndependentSetFactoring::optimize_feature(FactoredVars &mis, const NGraph &g, int &f_rm, int &c_rm, int &f_c) const {
    if (arg_minimize_center == 0) {
        return;
    }

    set<int> center;
    vector<bool> in_center(g_variable_domain.size(), true);
    vector<int> var_to_factor(g_variable_domain.size(), -1);
    for (size_t i = 0; i < mis.size(); ++i) {
        for (int var : mis[i]){
            in_center[var] = false;
            var_to_factor[var] = i;
        }
    }
    for (size_t var = 0; var < in_center.size(); ++var) {
        if (in_center[var]) {
            assert(var_to_factor[var] == -1);
            center.insert(var);
        }
    }

    bool minimize = true;
    bool removed_frozen_leaves = false;

    while (minimize) {
        vector<int> candidates; // pos -> var
        vector<int> candidates_to_leaf; // pos -> leaf

        // collect candidates (center vars with one leaf neighbour)
        for (int c : center) {
            set<int> neighbour_leaves;
            for (int n : g[c]) {
                if (!in_center[n]) {
                    neighbour_leaves.insert(var_to_factor[n]);
                }
            }
            if (neighbour_leaves.size() == 1) {
                // check leaf factor size
                uint64_t f_size = g_variable_domain[c];
                for (int v : mis[*neighbour_leaves.begin()]) {
                    f_size *= g_variable_domain[v];
                }
                if (max_leaf_size == 0 || f_size <= max_leaf_size) {
                    candidates.push_back(c);
                    candidates_to_leaf.push_back(*neighbour_leaves.begin());
                }
            }
        }
        if (!candidates.empty()) {
            // TODO in general, if none of the current candidates does improve the ranking
            // over "mis", we should not force candidate[0] to be put into a leaf!
            // it should be enough to initialize max_rank to the ranking of mis and have some
            // way to prevent infinite looping over candidate sets that never change
            size_t max = 0;
            vector<VarInt> max_rank (arg_rankings.size(), VarInt(0));
            for (size_t i = 0; i < candidates.size(); ++i) {
                int candidate = candidates[i];
                int leaf = candidates_to_leaf[i];

                mis[leaf].insert(candidate);

                // call ranking (multiple rankings?)
                bool update = false;
                vector<VarInt> rankings;
                for (size_t n = 0; n < arg_rankings.size(); ++n) {
                    rankings.push_back(VarInt(arg_rankings[n]->get_rank(mis)));
                    if (arg_rankings[n]->maximizes()) {
                        if (rankings[n] > max_rank[n]) {
                            update = true;
                        }
                    } else {
                        if (rankings[n] < max_rank[n]) { // TODO max_rank is initialized to all 0!
                            update = true;
                        }
                    }
                }
                if (update) {
                    assert(rankings.size() == arg_rankings.size());
                    max = i;
                    max_rank = rankings;
                }
                mis[leaf].erase(candidate);
            }
            
            int candidate = candidates[max];
            int leaf = candidates_to_leaf[max];
            // and push it into the respective leaf
            mis[leaf].insert(candidate);
            var_to_factor[candidate] = leaf;
            in_center[candidate] = false;
            center.erase(candidate);
            c_rm++;
        } else {
            if (arg_merge_leaves > 0 && !removed_frozen_leaves) {
                
                bool merge = true;
                vector<int> merged_leaves_counter(mis.size(), 0);

                while (merge) {
                    vector<int> merge_candidates;
                    vector<set<int>> merge_candidates_to_leaves;

                    // try to merge frozen leaves
                    // collect candidates (connected to frozen vars + max 1 unfrozen var)
                    for (int var : center) {
                        set<int> neighbour_leaves;
                        int max_merge_count = 0;
                        for (int n : g[var]) {
                            if (!in_center[n]) {
                                neighbour_leaves.insert(var_to_factor[n]);
                                if (merged_leaves_counter[var_to_factor[n]] > max_merge_count){
                                    max_merge_count = merged_leaves_counter[var_to_factor[n]];
                                }
                            }
                        }
                        if (neighbour_leaves.size() > 1 && max_merge_count < arg_merge_leaves) {
                            // check leaf factor size
                            uint64_t f_size = g_variable_domain[var];
                            for (int f : neighbour_leaves) {
                                for (int v : mis[f]) {
                                    f_size *= g_variable_domain[v];
                                }
                            }
                            if ((max_leaf_size == 0 || f_size <= max_leaf_size) && 
                                    min_number_leaves <= (mis.size() - neighbour_leaves.size() + 1)) {
                                merge_candidates.push_back(var);
                                merge_candidates_to_leaves.push_back(neighbour_leaves);
                            }
                        }
                    }

                    // select best candidate
                    if (!merge_candidates.empty()) {
                        // TODO in general, if none of the current merge_candidates does improve the ranking
                        // over "mis", we should not force candidate[0] to be put into a leaf!
                        // it should be enough to initialize max_rank to the ranking of mis, have a flag that indicates
                        // whether if max_elem==0, it actually improves the ranking, and have some
                        // way to prevent infinite looping over candidate sets that never change
                        size_t max_elem = 0;
                        vector<VarInt> max_rank(arg_rankings.size(), VarInt(0));
                        for (size_t i = 0; i < merge_candidates.size(); ++i) {
                            FactoredVars new_mis(mis);
                            set<int> new_leaf;
                            new_leaf.insert(merge_candidates[i]);
                            for (int l : merge_candidates_to_leaves[i]) {
                                for (int v : mis[l]) {
                                    new_leaf.insert(v);
                                }
                                new_mis[l].clear();
                            }
                            new_mis.push_back(new_leaf);

                            FactoredVars tmp;
                            for (int m = new_mis.size() - 1; m >= 0; --m) {
                                if (new_mis[m].empty()){
                                    new_mis.erase(new_mis.begin() + m);
                                }
                            }

                            bool update = false;
                            vector<VarInt> rankings;
                            for (size_t n = 0; n < arg_rankings.size(); ++n) {
                                rankings.push_back(VarInt(arg_rankings[n]->get_rank(new_mis)));
                                if (arg_rankings[n]->maximizes()) {
                                    if (rankings[n] > max_rank[n]) {
                                        update = true;
                                    }
                                } else {
                                    if (rankings[n] < max_rank[n]) { // TODO max_rank is initialized to all 0!
                                        update = true;
                                    }
                                }
                            }
                            if (update) {
                                assert(rankings.size() == arg_rankings.size());
                                max_elem = i;
                                max_rank = rankings;
                            }
                        }

                        set<int> new_leaf;
                        int c = merge_candidates[max_elem];
                        new_leaf.insert(c);
                        in_center[c] = false;
                        center.erase(c);
                        c_rm++;
                        int max_merge_counter = 0;
                        for (int l : merge_candidates_to_leaves[max_elem]){
                            for (int v : mis[l]) {
                                new_leaf.insert(v);
                            }
                            mis[l].clear();
                            if (merged_leaves_counter[l] > max_merge_counter) {
                                max_merge_counter = merged_leaves_counter[l];
                            }
                        }
                        mis.push_back(new_leaf);
                        merged_leaves_counter.push_back(max_merge_counter + 1);

                        for (int i = mis.size() - 1; i >= 0; --i) {
                            if (mis[i].empty()) {
                                mis.erase(mis.begin() + i);
                                merged_leaves_counter.erase(merged_leaves_counter.begin() + i);
                            }
                        }
                        assert(mis.size() == merged_leaves_counter.size());
                        
                        for (size_t f = 0; f < mis.size(); ++f){
                            for (int var : mis[f]){
                                var_to_factor[var] = f;
                            }
                        }

                    } else {
                        break;
                    }
                }
            }
            if (!removed_frozen_leaves) {
                // push frozen leaves back in center
                removed_frozen_leaves = true;

                f_rm = 0;
                vector<bool> frozen_leaves = get_frozen_leaves(mis);
                for (int i = frozen_leaves.size() - 1; i >= 0; --i) {
                    if (frozen_leaves[i]) {
                        for (int v : mis[i]) {
                            in_center[v] = true;
                            center.insert(v);
                            var_to_factor[v] = -1;
                        }
                        mis.erase(mis.begin() + i);
                        f_rm++;
                    }
                }

                if (f_rm == 0 || mis.empty()) {
                    // removed no frozen leaves or no leaves left
                    break;
                } else if (!arg_minimize_frozen) {
                    break;
                } else {
                    f_c = 0;
                    // only continue to minimize center, including frozen leaves, if minimize_frozen = true
                    for (int var : center) {
                        set<int> neighbour_leaves;
                        for (int n : g[var]) {
                            if (!in_center[n]) {
                                neighbour_leaves.insert(var_to_factor[n]);
                            }
                        }
                        if (neighbour_leaves.size() == 1) {
                            ++f_c;
                        }
                    }
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
}


// Factoring::FactoredVars MaxIndependentSetFactoring::minimize_center(FactoredVars max_ind_sets, const NGraph &cg_g) {
//     FactoredVars adapted_factoring;
//     if (arg_minimize_center == 0) {
//         cout << "do not minimize center" << endl;
//         adapted_factoring =  max_ind_sets;
//     }
//     else if (arg_minimize_center == 1 || arg_minimize_center == 2) {
//         cout << "minimize center - baseline - greedily increase mobility" << endl;
//         // baseline - greedily maximize mobility
// 
//         // retrieve center vars
//         set<int> center;
//         vector<bool> in_center(cg_g.size(), true);
//         map<int, int> var_to_factor;
//         for (size_t i = 0; i < max_ind_sets.size(); i++) {
//             for (set<int>::iterator it = max_ind_sets[i].begin(); it != max_ind_sets[i].end(); ++it) {
//                 in_center[*it] = false;
//                 var_to_factor.insert(pair<int, int>(*it, i));
//             }
//         }
//         for (size_t i = 0; i < in_center.size(); i++) {
//             if (in_center[i]) {
//                 center.insert(i);
//                 var_to_factor.insert(make_pair(i, -1));
//             }
//         }
// 
//         bool minimize = true;
//         int c_rm = 0;
// 
//         vector<int> candidates; // pos -> var
//         map<int,int> var_to_candidate; // var -> pos in candidates
//         map<int, int> candidates_to_leaf; // var -> leaf
// 
//         bool removed_frozen_leaves = false;
// 
//         while (minimize) {
//             candidates.clear();
//             var_to_candidate.clear();
//             candidates_to_leaf.clear();
//             // collect candidates (center vars with one leaf neighbour)
//             for (set<int>::iterator it = center.begin(); it != center.end(); ++it) {
//                 set<int> leaf_neighbours;
//                 for (set<int>::iterator n = cg_g.at(*it).begin(); n != cg_g.at(*it).end(); ++n) {
//                     if (!in_center[*n]) {
//                         leaf_neighbours.insert(var_to_factor[*n]);
//                     }
//                 }
//                 if (leaf_neighbours.size() == 1) {
//                     candidates.push_back(*it);
//                     var_to_candidate.insert(make_pair(*it,var_to_candidate.size()));
//                     candidates_to_leaf.insert(make_pair(*it, *leaf_neighbours.begin()));
// 
//                 }
//             }
// 
//             if (candidates_to_leaf.empty()){
//                 if (!removed_frozen_leaves){
//                     // push frozen leaves back in center
//                     removed_frozen_leaves = true;
//                     FactoredVars temp;
//                     int f_rm = 0;
// 
//                     vector<bool> frozen_leaves = get_frozen_leaves(max_ind_sets);
//                     for (size_t i = 0; i < frozen_leaves.size(); i++) {
//                         if (!frozen_leaves[i]) {
//                             temp.push_back(max_ind_sets[i]);
//                         } else {
//                             f_rm++;
//                         }
//                     }
//                     max_ind_sets = temp;
//                     cout << "Removed frozen leaves: " << f_rm << endl;
//                     if (f_rm == 0 || max_ind_sets.empty()) {
//                         cout << "removed no frozen leaves or no leaves left" << endl;
//                         break;
//                     } else if(arg_minimize_center == 1) {
//                         break;
//                     } else {
//                         // only continue to minimize center, including frozen leaves, if arg_min_center == 2
//                         continue;
//                     }
//                 } else {
//                     break;
//                 }
//             }
// 
//             // count how many actions affect each center var exclusively (no shared actions)
//             vector<int> affecting_actions(var_to_candidate.size(), 0);
//             for (size_t i = 0; i < g_operators.size(); i++) {
//                 const Operator *op = &g_operators[i];
//                 set<int> eff_factors;
//                 set<int> eff_center_vars;
// 
//                 // test first whether this action affects one of the candidates, otherwise skip it
//                 for (size_t post = 0; post < op->get_effects().size(); post++) {
//                     const int prepost = (op->get_effects()[post]).var;
//                     eff_factors.insert(var_to_factor[prepost]);
//                     if (var_to_factor[prepost] == -1) {
//                         eff_center_vars.insert(prepost);
//                     }
//                 }
//                 // has to affect center
//                 if (eff_center_vars.empty()) {
//                     continue;
//                 }
// 
//                 // has to affect a candidate
//                 bool affects_candidate = false;
//                 int candidate = -1; // var, not pos in candidates
//                 for (set<int>::iterator cv = eff_center_vars.begin(); cv != eff_center_vars.end(); ++cv) {
//                     if (candidates_to_leaf.find(*cv) != candidates_to_leaf.end()) {
//                         affects_candidate = true;
//                         candidate = *cv;
//                         break;
//                     }
//                 }
//                 if (!affects_candidate) {
//                     continue;
//                 }
//                 assert(candidate != -1);
// 
//                 // may only affect candidate and/or respective candidate leaf
//                 if (eff_factors.size() >= 2) {
//                     if (eff_factors.size() != 2 ||
//                         (eff_factors.find(candidates_to_leaf[candidate]) == eff_factors.end())) {
//                         continue;
//                     }
//                 }
// 
//                 // preconditions only on center or candidate leaf
//                 bool invalid = false;
//                 for (size_t prev = 0; prev < op->get_preconditions().size(); ++prev) {
//                     const Condition &prevail = op->get_preconditions()[prev];
//                     if ((var_to_factor[prevail.var] != -1) &&
//                         (var_to_factor[prevail.var] != candidates_to_leaf[candidate])) {
//                         invalid = true;
//                         break;
//                     }
//                 }
//                 if (invalid) {
//                     continue;
//                 }
// 
//                 // action either affects a center var exclusively,
//                 // or is shared between a center var and its only leaf neighbour
//                 affecting_actions[var_to_candidate[candidate]]++;
//             }
// 
//             // find candidate that increases mobility the most
//             int max = 0;
//             for (size_t i = 1; i < affecting_actions.size(); i++) {
//                 if (affecting_actions[i] > affecting_actions[max]) {
//                     max = i;
//                 }
//             }
// 
//             // and push it into the respective leaf
//             int candidate = candidates[max];
//             int leaf = candidates_to_leaf[candidates[max]];
//             max_ind_sets[leaf].insert(candidate);
//             in_center[candidate] = false;
//             center.erase(candidate);
//             var_to_factor[candidate] = leaf;
//             c_rm++;
//         }
//         cout << "Minimized center, variables removed: " << c_rm << endl;
//         adapted_factoring = max_ind_sets;
//     }
//     else if (arg_minimize_center == 3 || arg_minimize_center == 4) {
//         cout << "minimize center - experimental + merge leaves" << endl;
//         // experimental + merge leaves
//         // minimize center, by pushing center variables into leaves
//         int c_rm = 0;
//         int f_rm = 0;
//         int c_rm_o = 0;
//         int f_rm_o = 0;
//         bool changed = true;
//         bool removed_frozen_leaves = false;
//         int times_minimized = 0;
//         while (changed) {
//             changed = false;
//             times_minimized++;
//             map<int, int> var_to_leaf;
//             // extract center variables
//             list<int> center;
//             vector<bool> in_center(cg_g.size(), true);
//             for (size_t i = 0; i < max_ind_sets.size(); i++) {
//                 for (set<int>::iterator it = max_ind_sets[i].begin(); it != max_ind_sets[i].end(); ++it) {
//                     in_center[*it] = false;
//                     var_to_leaf.insert(pair<int, int>(*it, i));
//                 }
//             }
//             // sort center variables increasingly by number of neighbours
//             for (size_t i = 0; i < cg_g.size(); i++) {
//                 if (in_center[i]) {
//                     if (center.empty() || cg_g.at(i).size() <= cg_g.at(center.front()).size()) {
//                         center.push_front(i);
//                     } else if (cg_g.at(i).size() >= cg_g.at(center.back()).size()) {
//                         center.push_back(i);
//                     } else {
//                         for (list<int>::iterator s_it = center.begin(); s_it != center.end(); ++s_it) {
//                             if (cg_g.at(i).size() <= cg_g.at(*s_it).size()) {
//                                 center.insert(s_it, i);
//                                 break;
//                             }
//                         }
//                     }
//                 }
//             }
//             vector<bool> merged_leaf(max_ind_sets.size(), false);
//             for (list<int>::iterator c = center.begin(); c != center.end(); ++c) {
//                 if (cg_g.at(*c).size() == 0) {
//                     continue;
//                 } else if (cg_g.at(*c).size() == 1) {
// 
//                     set<int>::iterator n = cg_g.at(*c).begin();
//                     if (!in_center[*n]) {
//                         // add c to respective leaf
//                         max_ind_sets[var_to_leaf[*n]].insert(*c);
//                         in_center[*c] = false;
//                         var_to_leaf.insert(pair<int, int>(*c, var_to_leaf[*n]));
//                         c_rm++;
//                     }
//                 } else {
//                     // if all neighbours lie in the center we can skip this var
//                     set<int> leaf_neighbours;
//                     for (set<int>::iterator n = cg_g.at(*c).begin(); n != cg_g.at(*c).end(); ++n) {
//                         if (!in_center[*n]) {
//                             leaf_neighbours.insert(var_to_leaf[*n]);
//                         }
//                     }
//                     if (leaf_neighbours.size() == 0) {
//                         continue;
//                     }
//                     if (leaf_neighbours.size() == 1) {
//                         int l = *leaf_neighbours.begin();
//                         max_ind_sets[l].insert(*c);
//                         in_center[*c] = false;
//                         var_to_leaf.insert(pair<int, int>(*c, l));
//                         c_rm++;
//                     } else {
//                         if (arg_merge_leaves == 2 || arg_merge_leaves == 3) {
//                             set<int> temp;
//                             int pos = max_ind_sets.size();
//                             bool invalid_merge = false;
//                             map<int, int> temp_map(var_to_leaf);
//                             for (set<int>::iterator l = leaf_neighbours.begin(); l != leaf_neighbours.end(); ++l) {
//                                 if (arg_merge_leaves == 3 || (arg_merge_leaves == 2 && !merged_leaf[*l])) {
//                                     temp.insert(max_ind_sets[*l].begin(), max_ind_sets[*l].end());
//                                     set<int> empty;
//                                     max_ind_sets[*l] = empty;
//                                     for (set<int>::iterator it = temp.begin(); it != temp.end(); ++it){
//                                         temp_map[*it] = pos;
//                                     }
//                                 }
//                                 else if (arg_merge_leaves == 2 && merged_leaf[*l]) {
//                                     invalid_merge = true;
//                                     break;
//                                 }
//                             }
//                             if (!invalid_merge) {
//                                 var_to_leaf = temp_map;
//                                 temp.insert(*c);
//                                 in_center[*c] = false;
//                                 var_to_leaf.insert(pair<int, int>(*c, pos));
//                                 c_rm++;
//                                 merged_leaf.push_back(true);
//                                 max_ind_sets.push_back(temp);
//                             }
//                         }
//                     }
//                 }
//             }
//             for (vector<set<int>>::iterator it = max_ind_sets.begin(); it != max_ind_sets.end();) {
//                 if (it->empty()) {
//                     it = max_ind_sets.erase(it);
//                 } else {
//                     it++;
//                 }
//             }
//             cout << "Minimized center, variables removed: " << c_rm << endl;
// #ifdef DEBUG_FACTORING
//             for (size_t i = 0; i < max_ind_sets.size(); i++) {
//                 cout << "   (";
//                 set<int> factor = max_ind_sets[i];
//                 for (set<int>::iterator it = factor.begin(); it != factor.end(); it++) {
//                     cout << *it << " ";
//                 }
//                 cout << ")" << endl;
//             }
//             cout << endl;
// #endif
//             if (!removed_frozen_leaves) {
//                 if (arg_merge_leaves == 1){
//                     f_rm = 0;
//                     for (const int &c: center) {
//                         if (in_center[c]) {
//                             // if all neighbours lie in the center we can skip this var
//                             set<int> leaf_neighbours;
//                             for (set<int>::iterator n = cg_g.at(c).begin(); n != cg_g.at(c).end(); ++n) {
//                                 if (!in_center[*n]) {
//                                     leaf_neighbours.insert(var_to_leaf[*n]);
//                                 }
//                             }
//                             if (leaf_neighbours.empty()) {
//                                 continue;
//                             }
//                             set<int> temp;
//                             int pos = max_ind_sets.size();
//                             map<int, int> temp_map(var_to_leaf);
//                             vector<bool> frozen = get_frozen_leaves(max_ind_sets);
//                             int not_frozen = 0;
//                             for (const int &l: leaf_neighbours) {
//                                 temp.insert(max_ind_sets[l].begin(), max_ind_sets[l].end());
//                                 set<int> empty;
//                                 max_ind_sets[l] = empty;
//                                 for (const int &it: temp) {
//                                     temp_map[it] = pos;
//                                 }
//                                 if (!frozen[l]) {
//                                     not_frozen++;
//                                 } else {
//                                     f_rm++;
//                                 }
//                             }
//                             if (not_frozen <= 1) {
//                                 var_to_leaf = temp_map;
//                                 temp.insert(c);
//                                 in_center[c] = false;
//                                 var_to_leaf.insert(pair<int, int>(c, pos));
//                                 c_rm++;
//                                 merged_leaf.push_back(true);
//                                 max_ind_sets.push_back(temp);
//                             }
//                         }
//                     }
//                     cout << "merged " << f_rm << " frozen leaves" << endl;
// #ifdef DEBUG_FACTORING
//                     cout << "Factoring" << endl;
//                     for (size_t i = 0; i < max_ind_sets.size(); i++) {
//                         cout << "   (";
//                         set<int> factor = max_ind_sets[i];
//                         for (const int &it: factor) {
//                             cout << it << " ";
//                         }
//                         cout << ")" << endl;
//                     }
//                     cout << endl;
// #endif
//                 }
//                 f_rm = 0;
//                 // put frozen leaves back in center
//                 FactoredVars temp;
//                 vector<bool> frozen_leaves = get_frozen_leaves(max_ind_sets);
//                 for (size_t i = 0; i < frozen_leaves.size(); i++) {
//                     if (!frozen_leaves[i]) {
//                         temp.push_back(max_ind_sets[i]);
//                     } else {
//                         f_rm++;
//                     }
//                 }
//                 max_ind_sets.clear();
//                 max_ind_sets.insert(max_ind_sets.begin(), temp.begin(), temp.end());
//                 if (f_rm != f_rm_o) {
//                     changed = true;
//                     f_rm_o = f_rm;
//                 }
//                 cout << "Removed frozen leaves: " << f_rm << endl;
// #ifdef DEBUG_FACTORING
//                 cout << "Factoring" << endl;
//                 for (size_t i = 0; i < max_ind_sets.size(); i++) {
//                     cout << "   (";
//                     set<int> factor = max_ind_sets[i];
//                     for (const int &it: factor) {
//                         cout << it << " ";
//                     }
//                     cout << ")" << endl;
//                 }
//                 cout << endl;
// #endif
//             }
// 
//             if ((c_rm_o != c_rm) && arg_minimize_center == 4) {
// #ifdef DEBUG_FACTORING
//                 cout << "removed center" << c_rm << "vars in round: " << times_minimized << endl;
// #endif
//                 changed = true;
//                 c_rm_o = c_rm;
//                 c_rm = 0;
//             } else if (changed && !(arg_minimize_center == 4)) {
//                 // only count removing frozen leaves as a change, if iterativ minimize_center is active
//                 changed = false;
//             }
//         }
// #ifdef DEBUG_FACTORING
//         cout << "do not repeat arg_minimize_center, round: " << times_minimized << endl;
// #endif
//         adapted_factoring = max_ind_sets;
//     }
// 
//     // if center is empty, use biggest leaf as center
//     size_t leaf_vars = 0;
//     for (FactoredVars::iterator factor = adapted_factoring.begin(); factor != adapted_factoring.end(); ++factor){
//         for (set<int>::iterator var = factor->begin(); var != factor->end(); ++var) {
//             leaf_vars++;
//         }
//     }
//     if (leaf_vars == g_variable_domain.size()) {
//         FactoredVars::iterator max = adapted_factoring.begin();
//         if (adapted_factoring.size() > 1) {
//             for (FactoredVars::iterator i = adapted_factoring.begin(); i != adapted_factoring.end(); i++) {
//                 if (i->size() > max->size()) {
//                     max = i;
//                 }
//             }
//         }
//         adapted_factoring.erase(max);
//         cout << "Repopulated center: 1" << endl;
//     } else {
//         cout << "Repopulated center: 0" << endl;
//     }
// 
//     return adapted_factoring;
// }


/////////////////////////////////////////////////////////////////////////////////////////////// domain specific factorings


Factoring::FactoredVars SokobanFactoring::get_factoring() {
    
    FactoredVars best_factoring(1);
    
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
	    if (max_leaf_size != 0 && current_size > max_leaf_size){
		break;
	    }
	}
	if (max_leaf_size == 0 || current_size <= max_leaf_size){
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
            if (max_leaf_size != 0 && current_size > max_leaf_size){
                break;
            }
        }
        if (max_leaf_size == 0 || current_size <= max_leaf_size){
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
    parser.add_option<PruningOptions*>(
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
    search_types.push_back("no_search");
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
}

void ForkFactoring::add_options_to_parser(OptionParser &parser) {
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

void MaxIndependentSetFactoring::add_options_to_parser(OptionParser &parser) {
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

    parser.add_list_option<Ranking *>("rankings",
                                      "TODO",
                                      "",
                                      OptionFlags(false));
    parser.add_option<bool>(
            "all_rankings",
            "TODO",
            "false"
    );
    parser.add_option<int>(
            "timeout",
            "TODO",
            "0"
    );
    parser.add_option<int>(
            "multiple_mis",
            "TODO",
            "1"
    );
}

static Factoring *_parse_auto(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    
    parser.add_list_option<Factoring*>("factorings",
                                       "TODO",
                                       "",
                                       OptionFlags(false));
    
    parser.add_option<int>("max_search_time",
                           "maximum time for test searches in seconds",
                           "10");

    Options opts = parser.parse();

    AutoFactoring *factoring = 0;
    if (!parser.dry_run()) {        
        factoring = new AutoFactoring(opts);
    }

    return factoring;
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

static Plugin<Factoring> _plugin_auto("auto", _parse_auto);
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

