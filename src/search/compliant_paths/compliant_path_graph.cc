#include "compliant_path_graph.h"

#include "cpg_storage.h"
#include "../factoring.h"
#include "../leaf_state_id.h"
#include "../operator.h"
#include "pricing_function.h"
#include "../pruning/pruning.h"
#include "reachability_function.h"
#include "../state.h"
#include "../state_id.h"
#include "../state_registry.h"
#include "../successor_generator.h"
#include "../timer.h"


using namespace std;


const int CompliantPathGraph::INF = -1;
    
const size_t CompliantPathGraph::MAX_LEAF_SPACE_STORE = 100000;

bool CompliantPathGraph::compute_leaf_backwards_graph = false;

vector<bool> CompliantPathGraph::precompute_leaf_state_spaces;

vector<vector<StateID> > CompliantPathGraph::leaf_price_cache;

vector<pair<int, int> > CompliantPathGraph::leaf_tags_to_cache;

vector<boost::dynamic_bitset<> > CompliantPathGraph::is_a_leaf_goal_state;

vector<vector<size_t> > CompliantPathGraph::leaf_goal_states;
    
vector<vector<vector<size_t> > > CompliantPathGraph::center_successors;
    
vector<vector<vector<pair<const Operator*, size_t> > > > CompliantPathGraph::leaf_state_successors;

vector<vector<vector<pair<const Operator*, size_t> > > > CompliantPathGraph::leaf_state_predecessors;
    
vector<vector<int> > CompliantPathGraph::leaf_state_id_map;
    
vector<size_t> CompliantPathGraph::curr_leaf_state_max_id;



bool CompliantPathGraph::is_leaf_goal_state(const LeafStateID &fact) {
    return is_a_leaf_goal_state[fact.get_factor()][fact.hash()];
}

void CompliantPathGraph::cache_leaf_price_tags(const State &state) {
    if (g_factoring->get_profile() != FORK && !leaf_tags_to_cache.empty()){
        for (size_t i = 0; i < leaf_tags_to_cache.size(); i++){
            size_t leaf_id = leaf_tags_to_cache[i].first;
            int factor = leaf_tags_to_cache[i].second;
            if (leaf_price_cache[factor].size() <= leaf_id){
                leaf_price_cache[factor].resize(leaf_id + 1, StateID::no_state);
            }
            leaf_price_cache[factor][leaf_id] = state.get_id();
        }
        vector<pair<int, int> >().swap(leaf_tags_to_cache);
    }
}

void CompliantPathGraph::discard_leaf_price_cache() {
    if (g_factoring->get_profile() != FORK && !leaf_tags_to_cache.empty()){
        vector<pair<int, int> >().swap(leaf_tags_to_cache);
    }
}

void CompliantPathGraph::initialize() {
    leaf_state_successors.resize(g_factors.size());    
    center_successors.resize(g_factors.size());    
    is_a_leaf_goal_state.resize(g_factors.size());
    leaf_goal_states.resize(g_factors.size());
    
    // initialize cache if at least one inv-fork leaf present
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (!g_factoring->has_predecessors(factor)){
            leaf_price_cache = vector<vector<StateID> >(g_factors.size());
            break;
        }
    }
}

void CompliantPathGraph::build_leaf_state_spaces() {

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "+++++++++++++++ STARTING RPG for calculation of minimal goal cost" << endl;
#endif
      
    leaf_state_id_map.resize(g_factors.size(), vector<int>(1, -1));
    curr_leaf_state_max_id.resize(g_factors.size(), 0);
    
    if (compute_leaf_backwards_graph){
        leaf_state_predecessors.resize(g_factors.size());
        precompute_leaf_state_spaces = vector<bool>(g_factors.size(), true);
    }
    
    Prices goal_price_tag(0);
    
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        goal_price_tag.add_state(LeafStateID(0, factor), 0);
    }
    
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        
        if (!precompute_leaf_state_spaces[factor]){
            continue;
        }
        
        if (!g_factoring->has_successors(factor) && g_goals_per_factor[factor].empty()){
            // skip fork-leaf factor without a goal
            continue;
        }

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
        cout << "       starting RPG for leaf factor " << factor << endl;
        int number_states = goal_price_tag.get_number_states(factor);
        size_t id = 0;
        while (number_states > 0){
            if (goal_price_tag.has_leaf_state(id, factor)){
                --number_states;
                g_state_registry->lookup_leaf_state(LeafStateID(id, factor)).dump_pddl();
            }
            ++id;
        }
#endif
        
        vector<int> best_prices;
        
        bool change = true;
        while (change) {

            change = false;

            for (size_t id = 0; id < g_state_registry->size(factor); ++id){
                if (!goal_price_tag.has_leaf_state(id, factor)){
                    continue;
                }
                int cost = goal_price_tag.get_cost_of_state(id, factor);
                if (id < best_prices.size()){
                    if (cost > best_prices[id]
                        || ((g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT)
                            && best_prices[id] != numeric_limits<int>::max())){
                        continue;
                    }
                }
                
                const LeafState predecessor = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
                
                if (id >= best_prices.size()){
                    best_prices.resize(id + 1, numeric_limits<int>::max());
                }
                bool first_time_seen = best_prices[id] == numeric_limits<int>::max();
                best_prices[id] = cost;
                
                
                vector<const Operator*> applicable_ops;
                
                if (g_factoring->has_successors(factor)){       // is non-fork leaf
                    if (first_time_seen){
                        vector<const Operator*> center_ops;
                        g_successor_generator->generate_applicable_ops_ignore_others(predecessor, center_ops, true);
                        
                        // store center ops whose preconditions are satisfied by predecessor
                        if (id >= center_successors[factor].size()){
                            center_successors[factor].resize(id+1);
                        }
                        center_successors[factor][id].reserve(center_ops.size());
                        for (size_t op = 0; op < center_ops.size(); ++op){
                            if (center_ops[op]->has_precondition_on(factor)){
                                center_successors[factor][id].push_back(center_ops[op]->get_index());
                            }
                        }
                        center_successors[factor][id].swap(center_successors[factor][id]);
                        if (g_factoring->has_predecessors(factor)){
                            // no need to apply center ops if none of them changes this leaf factor
                            applicable_ops.swap(center_ops);
                        }
                    } else if (g_factoring->has_predecessors(factor)){
                        // no need to apply center ops if none of them changes this leaf factor
                        applicable_ops.resize(center_successors[factor][id].size());
                        for (size_t o = 0; o < center_successors[factor][id].size(); ++o){
                            applicable_ops[o] = &g_operators[center_successors[factor][id][o]];
                        }
                    }
                }

                if (first_time_seen){
                    g_successor_generator->generate_applicable_ops_ignore_others(predecessor, applicable_ops);
                } else {
                    size_t old_size = applicable_ops.size();
                    applicable_ops.resize(applicable_ops.size() + leaf_state_successors[factor][id].size());
                    for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
                        applicable_ops[old_size + o] = leaf_state_successors[factor][id][o].first;
                    }
                }
                

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                cout << "       generated " << applicable_ops.size() << " applicable operators" << endl;
#endif
                
                if (id >= leaf_state_successors[factor].size()) {
                    leaf_state_successors[factor].resize(id + 1);
                }

                // apply all applicable_ops to predecessor and store the outcome          
                for (size_t j = 0; j < applicable_ops.size(); ++j){
                    const Operator *op = applicable_ops[j];

                    if (!op->get_effects(factor).empty()){                        
                        bool is_center_op = op->get_affected_factor() == -1;
                        
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                        cout << "################ trying to apply " << op->get_name() << " in state " << predecessor.get_id() << endl;
                        predecessor.dump_pddl();
#endif
                        LeafStateID lid = g_state_registry->get_successor_leaf_state(predecessor, *op).get_id();                        

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                        cout << "created leaf state " << lid << " of cost " << (cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type())) << endl;
                        g_state_registry->lookup_leaf_state(lid).dump_pddl();
#endif
                        int op_cost = 0;
                        if (!is_center_op){
                            op_cost = get_adjusted_action_cost(*op, g_factoring->get_cost_type());
                        }
                        
                        if (lid.hash() >= leaf_state_id_map[factor].size()){
                            leaf_state_id_map[factor].resize(lid.hash() + 1, -1);
                        }
                        
                        change |= goal_price_tag.add_state(lid, cost + op_cost);
                        
                        // TODO once the irrelevance_pruning methods are added, it might make sense to do something about
                        // that here, i.e. e.g. the goal states of fork leaves dont need successors
                        // is that actually true? what about finding a globally cheaper goal state behind a goal?
                        // the last comment doesn't make too much sense..
                        
                        if (!is_center_op && first_time_seen){
                            leaf_state_successors[factor][id].push_back(make_pair(op, lid.hash()));
                            if (compute_leaf_backwards_graph) { 
                                if (lid.hash() >= leaf_state_predecessors[factor].size()) {
                                    leaf_state_predecessors[factor].resize(lid.hash() + 1);
                                }
                                leaf_state_predecessors[factor][lid.hash()].push_back(make_pair(op, id));
                            }
                        }
                    }
                }
            }
        }
        
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "   RPG for leaf factor " << factor << " finished" << endl;
#endif
    
    }
    
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "+++++++++++++++ RPG  for calculation of minimal goal cost FINISHED" << endl;
#endif
    
    g_factoring->get_pruning()->init();
   
    size_t min_leaf_factor_size = numeric_limits<size_t>::max();
    double avg_leaf_factor_size = 0;
    size_t max_leaf_factor_size = 0;
    size_t tmp;
    bool built_all_state_spaces = true;
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (precompute_leaf_state_spaces[factor]){
            tmp = g_state_registry->size(factor);
            min_leaf_factor_size = min(min_leaf_factor_size, tmp);
            avg_leaf_factor_size += tmp;
            max_leaf_factor_size = max(max_leaf_factor_size, tmp);
        } else {
            built_all_state_spaces = false;
        }
    }
    if (min_leaf_factor_size != numeric_limits<size_t>::max()){
        cout << "min reachable leaf factor size "  << min_leaf_factor_size << endl;
    }
    if (built_all_state_spaces){
        cout << "avg reachable leaf factor size "  << (int) (avg_leaf_factor_size/g_factors.size()) << endl;
        cout << "max reachable leaf factor size "  << max_leaf_factor_size << endl;
    }
    
    
    g_min_goal_cost = vector<int>(g_factors.size(), numeric_limits<int>::max());
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (!precompute_leaf_state_spaces[factor]){
            cout << "min goal cost of factor " << factor << " not computed" << endl;
            g_min_goal_cost[factor] = 0;
            continue;
        }
        g_min_goal_cost[factor] = goal_price_tag.get_goal_cost(factor);
        
        if (g_min_goal_cost[factor] == INF){
            if (!g_goals_per_factor[factor].empty()){
                cout << "There is a goal that cannot be achieved in factor " << factor << endl;
                for (size_t i = 0; i < g_goals_per_factor[factor].size(); i++){
                    cout << g_fact_names[g_goals_per_factor[factor][i].first][g_goals_per_factor[factor][i].second] << endl;
                }
                cout << "Total time: " << g_timer << endl;
                exit_with(EXIT_UNSOLVABLE);
            }
            g_min_goal_cost[factor] = 0;
        } else {
            g_sum_min_goal_cost += g_min_goal_cost[factor];
        }
        if (g_goals_per_factor[factor].empty()){
            cout << "factor " << factor << " does not have a goal" << endl;
        } else {
            cout << "min goal cost of factor " << factor << " : " << g_min_goal_cost[factor] << endl;
        }
    }
    
    g_leaf_successor_generators.resize(g_factors.size());
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        if (!precompute_leaf_state_spaces[factor]){
            g_leaf_successor_generators[factor] = g_successor_generator->copy();
            g_leaf_successor_generators[factor]->split_ops_center_leaf(factor);
        }
    }
    g_successor_generator->split_ops_center_leaf(-1);
    
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        leaf_state_id_map[factor] = vector<int>(g_state_registry->size(factor), -1);
        curr_leaf_state_max_id[factor] = 0;
    }
    
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "leaving calculate_min_goal_cost function" << endl;
#endif
}

CompliantPathGraph& CompliantPathGraph::get_init_state_cpg() {
    if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
        return Reachable::get_init_state_reachable();
    } else {
        return Prices::get_init_state_prices();
    }
}

void CompliantPathGraph::store_is_leaf_goal_state(const LeafState &state) {
    int factor = state.get_id().get_factor();
    if (!g_goals_per_factor[factor].empty()){
        bool is_goal = true;
        for (size_t g = 0; g < g_goals_per_factor[factor].size(); ++g){
            int var = g_goals_per_factor[factor][g].first;
            int val = g_goals_per_factor[factor][g].second;
            if (state[var] != val){
                is_goal = false;
                break;
            }
        }
        assert(is_a_leaf_goal_state[factor].size() == state.get_id().hash());
        is_a_leaf_goal_state[factor].push_back(is_goal);
        if (is_goal && compute_leaf_backwards_graph){
            leaf_goal_states[factor].push_back(state.get_id().hash());
        }
    }
}

int CompliantPathGraph::get_leaf_state_id(const vector<int> &new_leaf_state, int factor) {
    assert(new_leaf_state.size() == g_factors[factor].size());

    size_t num_leaf_states_before = g_state_registry->size(factor);
    
    const LeafState &state = g_state_registry->get_leaf_state(new_leaf_state, factor);
    
    if ((leaf_state_successors[factor].size() <= state.get_id().hash() ||
        leaf_state_successors[factor][state.get_id().hash()].empty()) && // TODO that's a bit of a hack.. could also be a dead-end leaf state!
        precompute_leaf_state_spaces[factor]){
        
        Prices goal_price_tag(0);
    
        if (leaf_state_id_map[factor].size() <= state.get_id().hash()){
            leaf_state_id_map[factor].resize(state.get_id().hash() + 1, -1);
        }
        
        goal_price_tag.add_state(state.get_id(), 0);
        
        vector<int> best_prices;
        
        bool change = true;
        while (change) {

            change = false;

            for (size_t id = num_leaf_states_before; id < g_state_registry->size(factor); ++id){
                if (!goal_price_tag.has_leaf_state(id, factor)){
                    continue;
                }
                int cost = goal_price_tag.get_cost_of_state(id, factor);
                if (id < best_prices.size()){
                    if (cost > best_prices[id]
                        || ((g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT)
                            && best_prices[id] != numeric_limits<int>::max())){
                        continue;
                    }
                }
                
                const LeafState predecessor = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
                
                if (id >= best_prices.size()){
                    best_prices.resize(id + 1, numeric_limits<int>::max());
                }
                bool first_time_seen = best_prices[id] == numeric_limits<int>::max();
                best_prices[id] = cost;
                
                
                vector<const Operator*> applicable_ops;
                
                if (g_factoring->has_successors(factor)){       // is non-fork leaf
                    if (first_time_seen){
                        vector<const Operator*> center_ops;
                        g_successor_generator->generate_applicable_ops_ignore_others(predecessor, center_ops, true);
                        
                        // store center ops whose preconditions are satisfied by predecessor
                        if (id >= center_successors[factor].size()){
                            center_successors[factor].resize(id+1);
                        }
                        center_successors[factor][id].reserve(center_ops.size());
                        for (size_t op = 0; op < center_ops.size(); ++op){
                            if (center_ops[op]->has_precondition_on(factor)){
                                center_successors[factor][id].push_back(center_ops[op]->get_index());
                            }
                        }
                        center_successors[factor][id].swap(center_successors[factor][id]);
                        if (g_factoring->has_predecessors(factor)){
                            // no need to apply center ops if none of them changes this leaf factor
                            applicable_ops.swap(center_ops);
                        }
                    } else if (g_factoring->has_predecessors(factor)){
                        // no need to apply center ops if none of them changes this leaf factor
                        applicable_ops.resize(center_successors[factor][id].size());
                        for (size_t o = 0; o < center_successors[factor][id].size(); ++o){
                            applicable_ops[o] = &g_operators[center_successors[factor][id][o]];
                        }
                    }
                }

                if (first_time_seen){
                    g_successor_generator->generate_applicable_ops_ignore_others(predecessor, applicable_ops);
                } else {
                    size_t old_size = applicable_ops.size();
                    applicable_ops.resize(applicable_ops.size() + leaf_state_successors[factor][id].size());
                    for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
                        applicable_ops[old_size + o] = leaf_state_successors[factor][id][o].first;
                    }
                }
                

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                cout << "       generated " << applicable_ops.size() << " applicable operators" << endl;
#endif
                
                if (id >= leaf_state_successors[factor].size()) {
                    leaf_state_successors[factor].resize(id + 1);
                }

                // apply all applicable_ops to predecessor and store the outcome          
                for (size_t j = 0; j < applicable_ops.size(); ++j){
                    const Operator *op = applicable_ops[j];

                    if (!op->get_effects(factor).empty()){                        
                        bool is_center_op = op->get_affected_factor() == -1;
                        
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                        cout << "################ trying to apply " << op->get_name() << " in state " << predecessor.get_id() << endl;
                        predecessor.dump_pddl();
#endif
                        LeafStateID lid = g_state_registry->get_successor_leaf_state(predecessor, *op).get_id();                        

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                        cout << "created leaf state " << lid << " of cost " << (cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type())) << endl;
                        g_state_registry->lookup_leaf_state(lid).dump_pddl();
#endif
                        int op_cost = 0;
                        if (!is_center_op){
                            op_cost = get_adjusted_action_cost(*op, g_factoring->get_cost_type());
                        }
                        
                        if (lid.hash() >= leaf_state_id_map[factor].size()){
                            leaf_state_id_map[factor].resize(lid.hash() + 1, -1);
                        }
                        
                        change |= goal_price_tag.add_state(lid, cost + op_cost);
                        
                        // TODO once the irrelevance_pruning methods are added, it might make sense to do something about
                        // that here, i.e. e.g. the goal states of fork leaves dont need successors
                        // is that actually true? what about finding a globally cheaper goal state behind a goal?
                        // the last comment doesn't make too much sense..
                        
                        if (!is_center_op && first_time_seen){
                            leaf_state_successors[factor][id].push_back(make_pair(op, lid.hash()));
                            if (compute_leaf_backwards_graph) {
                                 // TODO check this
                                if (lid.hash() >= leaf_state_predecessors[factor].size()) {
                                    leaf_state_predecessors[factor].resize(lid.hash() + 1);
                                }
                                leaf_state_predecessors[factor][lid.hash()].push_back(make_pair(op, id));
                            }
                        }
                    }
                }
            }
        }
    }
    
    return state.get_id().hash();
}

void CompliantPathGraph::discard_new_graph() const {
    discard_leaf_price_cache();
}

