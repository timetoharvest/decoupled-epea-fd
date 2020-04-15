#include "compliant_path_graph.h"

#include "../algorithms/sccs.h"
#include "cpg_storage.h"
#include "../factoring.h"
#include "../leaf_state_id.h"
#include "../operator.h"
#include "pricing_function.h"
#include "pruning_options.h"
#include "reachability_function.h"
#include "../search_space.h"
#include "../state.h"
#include "../state_id.h"
#include "../state_registry.h"
#include "../task_utils/successor_generator.h"
#include "../utils/timer.h"


using namespace std;

unique_ptr<PruningOptions> CompliantPathGraph::pruning;

const int CompliantPathGraph::INF = -1;
    
const size_t CompliantPathGraph::MAX_LEAF_SPACE_STORE = 100000;

bool CompliantPathGraph::compute_leaf_backwards_graph = false;

SearchSpace* CompliantPathGraph::search_space;

OperatorCost CompliantPathGraph::cost_type;

vector<bool> CompliantPathGraph::precompute_leaf_state_spaces;

vector<vector<StateID> > CompliantPathGraph::cpg_cache;

vector<LeafStateID> CompliantPathGraph::leaf_states_to_cache_for_current_cpg;

vector<boost::dynamic_bitset<> > CompliantPathGraph::is_a_leaf_goal_state;

vector<vector<LeafStateHash> > CompliantPathGraph::leaf_goal_states;
    
vector<vector<vector<OperatorID> > > CompliantPathGraph::center_successors;

unique_ptr<successor_generator::SuccessorGenerator> CompliantPathGraph::center_action_successor_generator;

vector<unique_ptr<successor_generator::SuccessorGenerator> > CompliantPathGraph::leaf_successor_generators;
    
vector<vector<vector<pair<OperatorID, LeafStateHash> > > > CompliantPathGraph::leaf_state_successors;

vector<vector<vector<pair<OperatorID, LeafStateHash> > > > CompliantPathGraph::leaf_state_predecessors;

vector<bool> CompliantPathGraph::is_leaf_state_space_scc;
    
vector<vector<int> > CompliantPathGraph::leaf_state_id_map;
    
vector<size_t> CompliantPathGraph::curr_leaf_state_max_id;



bool CompliantPathGraph::is_leaf_goal_state(LeafStateHash id, LeafFactorID factor) {
    assert(is_a_leaf_goal_state.size() > factor && is_a_leaf_goal_state[factor].size() > id);
    return is_a_leaf_goal_state[factor][id];
}

void CompliantPathGraph::cache_cpg(const State &state) {
    if (g_factoring->get_profile() != FORK && !leaf_states_to_cache_for_current_cpg.empty()){
        for (size_t i = 0; i < leaf_states_to_cache_for_current_cpg.size(); i++){
            LeafStateHash leaf_id = leaf_states_to_cache_for_current_cpg[i].hash();
            LeafFactorID factor = leaf_states_to_cache_for_current_cpg[i].get_factor();
            if (cpg_cache[factor].size() <= leaf_id){
                cpg_cache[factor].resize(leaf_id + 1, StateID::no_state);
            }
            cpg_cache[factor][leaf_id] = state.get_id();
        }
        vector<LeafStateID>().swap(leaf_states_to_cache_for_current_cpg);
    }
}

void CompliantPathGraph::clear_cpg_cache(const State &state) {
	if (g_factoring->get_profile() != FORK){
		for (LeafFactorID factor(0); factor < cpg_cache.size(); ++factor){
			if (g_factoring->is_ifork_leaf(factor)){
				for (LeafStateHash id(0); id < cpg_cache[factor].size(); ++id){
					if (cpg_cache[factor][id] == state.get_id()){
						cpg_cache[factor][id] = StateID::no_state;
					}
				}
			}
		}
	}
}

void CompliantPathGraph::discard_cpg_cache() {
    if (!leaf_states_to_cache_for_current_cpg.empty()){
        vector<LeafStateID>().swap(leaf_states_to_cache_for_current_cpg);
    }
}

void CompliantPathGraph::initialize() {
    leaf_state_successors.resize(g_leaves.size());    
    center_successors.resize(g_leaves.size());    
    is_a_leaf_goal_state.resize(g_leaves.size());
    leaf_goal_states.resize(g_leaves.size());
    
    // initialize cache if at least one inv-fork leaf present
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        if (g_factoring->is_ifork_leaf(factor)){
            cpg_cache = vector<vector<StateID> >(g_leaves.size());
            break;
        }
    }
}

void CompliantPathGraph::build_leaf_successor_generators() {
    if (g_factoring->get_profile() != FACTORING_PROFILE::FORK){
        center_action_successor_generator = unique_ptr<successor_generator::SuccessorGenerator>(new successor_generator::SuccessorGenerator(LeafFactorID::CENTER, true));
    }
    leaf_successor_generators.reserve(g_leaves.size());
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        leaf_successor_generators.push_back(unique_ptr<successor_generator::SuccessorGenerator>(new successor_generator::SuccessorGenerator(factor)));
    }
}

void CompliantPathGraph::set_cost_type(OperatorCost ct) {
    cost_type = ct;
    g_min_action_cost_per_factor.resize(g_leaves.size(), numeric_limits<int>::max());
    for (const auto &op : g_operators){
        LeafFactorID factor = op.get_affected_factor();
        if (factor != LeafFactorID::CENTER){
            g_min_action_cost_per_factor[factor] = min(g_min_action_cost_per_factor[factor], get_adjusted_action_cost(op, cost_type));
        }
    }
}

pair<int, bool> CompliantPathGraph::check_dominance(const State &base_state,
                                                    int curr_number_duplicates,
                                                    const State &predecessor,
                                                    const Operator &op) {
    
    int new_g = search_space->get_node(predecessor).get_g() + get_adjusted_action_cost(op, cost_type) + g_inc_g_by;
    
#ifdef DEBUG_PRUNING
    cout << "start checking duplicates for state " << base_state.get_id() << ", g = " << new_g << endl;
    dump();
#endif
    
    vector<size_t> predecessors;
    
    if (PruningOptions::are_closed_nodes_reopened() &&
            !(g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT)){
        StateID curr_id = predecessor.get_id();
        while (curr_id.hash() != 0){
	    assert(find(predecessors.begin(), predecessors.end(), curr_id.hash()) == predecessors.end());
            predecessors.push_back(curr_id.hash());
            curr_id = search_space->get_node(g_state_registry->lookup_state(curr_id)).get_parent_state_id();
        }
        predecessors.push_back(0); // initial state always has StateID 0
    }
    
    int replaced_state_dup = -1;

    uint64_t hash = 0;
    if (pruning->use_exact_duplicate_checking()){
        hash = get_hash();
    }

    for (int dup = 0; dup <= curr_number_duplicates; ++dup){
        // check the price tags of all equal center states
        
#ifdef DEBUG_PRUNING
        cout << "current dup counter " << dup << endl;
#endif

        // get other state
        const State s = g_state_registry->get_center_state(base_state, dup);
        
#ifdef DEBUG_PRUNING
        cout << "compare to state " << s.get_id() << ", g = " << search_space->get_node(s).get_g() << endl;
        CPGStorage::storage->get_mutable_cpg(s)->dump();
#endif

        if (!PruningOptions::are_closed_nodes_reopened()){
            // in greedy search, close nodes that are dominated by this one as cannot
            // reach anything new from those that cannot be reached from this node
            // if dominated => do not consider this state
            // in this case, all dominated nodes are also dominated by the state
            // that dominates this one

            DOMINANCE res = check_dominance(s, hash);
            
            if (res == DOMINANCE::DOMINATES) {
                // set dominated nodes to closed
                SearchNode node = search_space->get_node(s);
                if (node.is_open()){
                    node.close();
                }
            } else if (res == DOMINANCE::DOMINATED || res == DOMINANCE::EQUAL) {
                // TODO could as well return duplicate right away; try this out
                // TODO if it turns out not to make a difference, merge this with
                // the SAT/UNSAT case below
                replaced_state_dup = dup;

                // otherwise, the path to an existing state dominating this one
                // might be updated (if reached with lower g), so that paths to leaf states that are not
                // reachable in this state cannot be constructed
                // this probably only happens in eager search, which updates the parent if
                // new g is lower even if closed nodes are not reopened
                PruningOptions::set_ignore_current_state();
            }
            
        } else {
            
            if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
                
                DOMINANCE res = check_dominance(s, hash);
            
                if (res == DOMINANCE::EQUAL || res == DOMINANCE::DOMINATED){
                    int old_g = search_space->get_node(s).get_g();
                    if (new_g < old_g){
                        // otherwise, that existing state would be reopened
                        PruningOptions::set_ignore_current_state();
                    }
                    return {dup, false};
                } else if (res == DOMINANCE::DOMINATES){
                    // set dominated nodes to closed
                    SearchNode node = search_space->get_node(s);
                    if (node.is_open()){
                        node.close();
                    }
                }
                
            } else {
            
                // pruning for optimal search
                
                int old_g = search_space->get_node(s).get_g();

                if (find(predecessors.begin(), predecessors.end(), s.get_id().hash()) == predecessors.end()){
                    // we need the distinction between g-values here, to mimic the behaviour of A*
                    // TODO we could improve pruning power by e.g. allowing states with higher g-value
                    // to dominate existing states, but this requires an explicit way to tell the search
                    // engine what to do with the current state, independent of its g value.
                    if (new_g > old_g){

                        DOMINANCE res = check_dominance(s, hash, old_g - new_g);

                        if (res == DOMINANCE::DOMINATED || res == DOMINANCE::EQUAL){
                            // current state is dominated by or equal to a previously generated state
                            return {dup, false};
                        } else if (pruning->include_g_in_dominance() && res == DOMINANCE::DOMINATES){
                            // close dominated states if they are not parent of current state
                            // cannot do anything else; search does not reopen nodes with higher g
                            SearchNode node = search_space->get_node(s);
                            if (node.is_open()){
                                node.close();
                            }
                        }

                    } else if (new_g < old_g) {

                        DOMINANCE res = check_dominance(s, hash, old_g - new_g);

                        if (res == DOMINANCE::EQUAL || res == DOMINANCE::DOMINATES){
                            // state will replace old state in search
                        	// need to set h dirty so the search evaluates the heuristic on the
                        	// new state, which can result in lower h value
                        	search_space->get_node(s).set_h_dirty();
                        	return {dup, true};
                        } else if (pruning->include_g_in_dominance() && res == DOMINANCE::DOMINATED){
                            // treat as duplicate, need to tell search to not reopen/update parent
                            PruningOptions::set_ignore_current_state();
                            return {dup, false};
                        }

                    } else {

                        DOMINANCE res = check_dominance(s, hash);

                        if (res == DOMINANCE::DOMINATED || res == DOMINANCE::EQUAL){
                            // current state is dominated by or equal to a previously generated state
                            return {dup, false};
                        } else if (res == DOMINANCE::DOMINATES){
                            // close dominated states if they are not parent of current state
                            // cannot do anything else; search does not reopen nodes with equal g
                            SearchNode node = search_space->get_node(s);
                            if (node.is_open()){
                                node.close();
                            }
                        }
                    }

                } else {
                    // current state is successor of the state compared to, so can only be
                    // pruned if it is dominated by/equal to its predecessor
                    assert(new_g >= old_g);

                    DOMINANCE res = check_dominance(s, hash, old_g - new_g, DOMINANCE::DOMINATED);

                    if (res == DOMINANCE::DOMINATED || res == DOMINANCE::EQUAL){
                        return {dup, false};
                    }
                }
            }
        }
    }
    if (replaced_state_dup != -1){
        return {replaced_state_dup, false};
    } else {
        return {curr_number_duplicates + 1, false};
    }
}

void CompliantPathGraph::build_leaf_state_spaces() {

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "building leaf state spaces" << endl;
#endif
      
    leaf_state_id_map.resize(g_leaves.size(), vector<int>(1, -1));
    curr_leaf_state_max_id.resize(g_leaves.size(), 0);
    
    is_leaf_state_space_scc.resize(g_leaves.size(), false);

    if (compute_leaf_backwards_graph){
        leaf_state_predecessors.resize(g_leaves.size());
        precompute_leaf_state_spaces = vector<bool>(g_leaves.size(), true);
    }
    
    Prices goal_price_tag;
    goal_price_tag.resize();
    
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        goal_price_tag.add_state(LeafStateHash(0), factor, 0);
    }
    
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){

        if (!precompute_leaf_state_spaces[factor]){
            continue;
        }
        
        if (g_factoring->is_fork_leaf(factor) && g_goals_per_factor[factor].empty()){
            // TODO remove these leaves from g_leaves; then this can go away
            // skip fork-leaf factor without a goal
            continue;
        }

        vector<vector<int> > leaf_only_state_space_graph;

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

            for (LeafStateHash id(0); id < g_state_registry->size(factor); ++id){
                int cost = goal_price_tag.get_cost_of_state(id, factor);
                if (id < best_prices.size()){
                    if (cost >= best_prices[id]
                        || ((g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT)
                            && best_prices[id] != numeric_limits<int>::max())){
                        continue;
                    }
                } else {
                    best_prices.resize(id + 1, numeric_limits<int>::max());
                    leaf_only_state_space_graph.resize(id + 1);
                }
                
                const LeafState curr_leaf_state = g_state_registry->lookup_leaf_state(id, factor);

                bool first_time_seen = best_prices[id] == numeric_limits<int>::max();
                best_prices[id] = cost;
                
                if (id >= leaf_state_successors[factor].size()) {
                    leaf_state_successors[factor].resize(id + 1);
                }

                if (g_factoring->is_fork_leaf(factor) && is_a_leaf_goal_state[factor][id]){
                    // the goal states of fork leaves don't need successors
                    if (compute_leaf_backwards_graph && id >= leaf_state_predecessors[factor].size()) {
                        leaf_state_predecessors[factor].resize(id + 1);
                    }
                    continue;
                }

                
                vector<OperatorID> applicable_ops;
                
                if (!g_factoring->is_fork_leaf(factor) &&
                        (first_time_seen || !g_factoring->is_ifork_leaf(factor))){

                    vector<OperatorID> center_ops;
                    center_action_successor_generator->generate_applicable_ops_ignore_outside_pre(curr_leaf_state, center_ops);

                    if (id >= center_successors[factor].size()){
                        center_successors[factor].resize(id+1);
                    }
                    for (OperatorID op_id : center_ops){
                        if (first_time_seen && g_operators[op_id].has_precondition_on(factor)){
                            // store center ops whose preconditions are satisfied by curr_leaf_state
                            center_successors[factor][id].push_back(op_id);
                        }
                        if (!g_factoring->is_ifork_leaf(factor) && g_operators[op_id].has_effect_on(factor)){
                            // no need to apply center ops if it does not change this leaf factor
                            applicable_ops.push_back(op_id);
                        }
                    }
                    if (first_time_seen){
                        center_successors[factor][id].shrink_to_fit();
                    }
                }

                if (first_time_seen){
                    leaf_successor_generators[factor]->generate_applicable_ops(curr_leaf_state, applicable_ops);
                } else {
                    size_t old_size = applicable_ops.size();
                    applicable_ops.resize(applicable_ops.size() + leaf_state_successors[factor][id].size(), OperatorID::no_operator);
                    for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
                        applicable_ops[old_size + o] = leaf_state_successors[factor][id][o].first;
                    }
                }
                

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                cout << "       generated " << applicable_ops.size() << " applicable operators" << endl;
#endif

                // apply all applicable_ops to predecessor and store the outcome
                for (OperatorID op_id : applicable_ops){
                    const Operator &op = g_operators[op_id];
                    if (op.has_effect_on(factor)){
                        bool is_center_op = op.get_affected_factor() == LeafFactorID::CENTER;
                        
#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                        cout << "################ trying to apply " << op->get_name() << " in state " << curr_leaf_state.get_id() << endl;
                        curr_leaf_state.dump_pddl();
#endif
                        LeafStateHash succ_id = g_state_registry->get_successor_leaf_state_hash(curr_leaf_state, op);

                        if (id == succ_id){
                            // this would induce a self-loop in the leaf state space
                            continue;
                        }

                        if (!op.has_precondition_on(LeafFactorID::CENTER) && op.get_affected_factor() != LeafFactorID::CENTER){
                            // is leaf action without center precondition
                            leaf_only_state_space_graph[id].push_back(succ_id);
                        }

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
                        cout << "created leaf state " << succ_id << " of cost " << (cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type())) << endl;
                        g_state_registry->lookup_leaf_state(succ_id).dump_pddl();
#endif
                        int op_cost = 0;
                        if (!is_center_op){
                            op_cost = get_adjusted_action_cost(op, cost_type);
                        }
                        
                        if (succ_id >= leaf_state_id_map[factor].size()){
                            leaf_state_id_map[factor].resize(succ_id + 1, -1);
                        }
                        
                        change |= goal_price_tag.add_state(succ_id, factor, cost + op_cost);
                        
                        if (compute_leaf_backwards_graph && first_time_seen){
                            // this ensures that there is an entry leaf_state_predecessors[factor][lid]
                            // even in case lid has been generated by a center action
                            if (succ_id >= leaf_state_predecessors[factor].size()) {
                                leaf_state_predecessors[factor].resize(succ_id + 1);
                            }
                        }
                        
                        if (!is_center_op && first_time_seen){
                            leaf_state_successors[factor][id].push_back(make_pair(op_id, succ_id));
                            if (compute_leaf_backwards_graph){
                                leaf_state_predecessors[factor][succ_id].push_back(make_pair(op_id, id));
                            }
                        }
                    }
                }
            }
        }
        
        size_t num_sccs = sccs::compute_maximal_sccs(leaf_only_state_space_graph).size();
        if (num_sccs == 1){
            size_t prod_size = 1;
            for (int var : g_leaves[factor]){
                prod_size *= g_variable_domain[var];
            }
            if (prod_size != g_state_registry->size(factor)){
                cout << "WARNING: not all leaf states for leaf " << factor
                        << " are reachable, removing non-applicable center actions from successor generator" << endl;
                // TODO could do this for all leaves where leaf state space is constructed
                // TODO be careful with decoupled orbit search, there can be symmetric leaf
                // states not reachable from the initial state
                g_successor_generator->remove_never_applicable_center_ops(factor);
            }
            is_leaf_state_space_scc[factor] = true;
            cout << "state space of leaf " << factor << " is strongly connected via leaf-only actions" << endl;
        } else if (num_sccs == g_state_registry->size(factor)){
            cout << "no non-trivial leaf-only SCC in state space of leaf " << factor << endl;
        }

#ifdef DEBUG_PRECOMPUTE_GOAL_COST
    cout << "   RPG for leaf factor " << factor << " finished" << endl;
#endif
    
    }
    
    pruning->initialize();
   
    size_t min_leaf_factor_size = numeric_limits<size_t>::max();
    double avg_leaf_factor_size = 0;
    size_t max_leaf_factor_size = 0;
    size_t tmp;
    bool built_all_state_spaces = true;
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
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
        cout << "avg reachable leaf factor size "  << (int) (avg_leaf_factor_size/g_leaves.size()) << endl;
        cout << "max reachable leaf factor size "  << max_leaf_factor_size << endl;
    }
    
    
    g_min_goal_cost = vector<int>(g_leaves.size(), numeric_limits<int>::max());
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
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
                cout << "Total time: " << utils::g_timer << endl;
                exit_with(utils::ExitCode::SEARCH_UNSOLVABLE);
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
    
    bool all_built = true;
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        if (precompute_leaf_state_spaces[factor]){
            leaf_successor_generators[factor].reset();
        } else {
            all_built = false;
        }
    }
    if (center_action_successor_generator && all_built){
        center_action_successor_generator.reset();
    }
    
    for (LeafFactorID factor(0); factor < g_leaves.size(); ++factor){
        leaf_state_id_map[factor] = vector<int>(g_state_registry->size(factor), -1);
        curr_leaf_state_max_id[factor] = 0;
    }
    
    cout << "done building leaf state spaces [t=" << utils::g_timer() << "s]" << endl;
}

unique_ptr<CompliantPathGraph> CompliantPathGraph::get_init_state_cpg() {
    if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
        return Reachable::get_init_state_cpg();
    } else {
        return Prices::get_init_state_cpg();
    }
}

void CompliantPathGraph::store_is_leaf_goal_state(const LeafState &state) {
    LeafFactorID factor = state.get_id().get_factor();
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

void CompliantPathGraph::discard_new_cpg() const {
    discard_cpg_cache();
}

