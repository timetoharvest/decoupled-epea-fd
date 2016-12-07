#include "pricing_function.h"

#include "cpg_storage.h"
#include "../factoring.h"
#include "../globals.h"
#include "../operator.h"
#include "../priority_queue.h"
#include "../pruning/pruning.h"
#include "../search_space.h"
#include "../state_registry.h"
#include "../successor_generator.h"
#include "../utilities.h"
#include "../symmetries/decoupled_permutation.h"

#include <limits>
#include <numeric>


using namespace std;


Prices Prices::tmp_price;

PriceStorage* Prices::cpg_storage;

void Prices::register_cpg_storage(PriceStorage *storage) {
    cpg_storage = storage;
}


Prices::Prices(const Prices *tag) {
    if (tag == 0){
        number_states = vector<size_t>(g_factors.size(), 0);
        prices = vector<vector<int> >(g_factors.size());
        goal_cost = vector<int>(g_factors.size(), INF);
    } else {
        number_states = tag->number_states;
        prices = tag->prices;
        goal_cost = tag->goal_cost;
    }
}

void Prices::reduce_cost_by(int factor, int cost) {
    assert(g_factoring->get_profile() != FORK);
    
    if (goal_cost[factor] != INF){
        // if there is a goal state, its cost must be >= cost!
        goal_cost[factor] -= cost;
    }
    for (size_t id = 0; id < prices[factor].size(); ++id){
        if (prices[factor][id] != INF){
            prices[factor][id] -= cost;
        }
    }
}

bool Prices::add_state(const LeafStateID &state, int cost) {
    size_t factor = state.get_factor();
    if (leaf_state_id_map[factor][state.hash()] == -1){ // get new index to prevent facts vector from being sparse
        leaf_state_id_map[factor][state.hash()] = curr_leaf_state_max_id[factor];
        ++curr_leaf_state_max_id[factor];
    }
    size_t leaf_id = leaf_state_id_map[factor][state.hash()];
    if (leaf_id >= prices[factor].size()){
        prices[factor].resize(leaf_id + 1, INF);
    }
    if (prices[factor][leaf_id] == INF){
        prices[factor][leaf_id] = cost;
        ++number_states[factor];
        if (!g_goals_per_factor[factor].empty() && is_leaf_goal_state(state)){
            if (goal_cost[factor] == INF || goal_cost[factor] > cost){
                goal_cost[factor] = cost;
            }
        }
        return true;
    } else if (prices[factor][leaf_id] > cost){
        prices[factor][leaf_id] = cost;
        if (!g_goals_per_factor[factor].empty() && is_leaf_goal_state(state)){
            if (goal_cost[factor] > cost){
                goal_cost[factor] = cost;
            }
        }
        return true;
    }
    return false;
}

CompliantPathGraph& Prices::update_price_tags(const State &base_state,
                                    const Operator *op,
                                    const State &) const {
    
    if (g_factoring->get_profile() != FORK && op != 0){
        tmp_price = Prices(0);                  // when updating initial state => op == 0
        tmp_price.apply_center_op_to_leaves(*this, *op);
    } else {                
        tmp_price = Prices(this);
    }
    // the rest of this function works on tmp_price!
    
    
#ifdef DEBUG_SEARCH
    cout << "+++++++++++++++ STARTING RPG" << endl;
#endif
    
    assert(leaf_tags_to_cache.empty());

    for (size_t factor = 0; factor < g_factors.size(); factor++){
        
        // skip fork leaves that don't have a goal!
        if (!g_factoring->has_successors(factor) && g_goals_per_factor[factor].empty()){
            continue;
        }
        
        size_t number_leaf_states = g_state_registry->size(factor);

#ifdef DEBUG_SEARCH
        cout << "       starting RPG for leaf factor " << factor << endl;
        const vector<PricesInfo*> &achieved_facts = new_tag.get_facts(factor);
        for (int i = 0; i < achieved_facts.size(); i++){
            if (achieved_facts[i] == 0){
                continue;
            }
            lookup_leaf_state(LeafStateID(i, factor)).dump_pddl();
        }
        cout << "       " << new_tag.get_number_states(factor) << " fact(s) initially true in leaf factor " << factor << endl;
#endif 
        
        bool is_ifork_leaf = !g_factoring->has_predecessors(factor);
        
        if (g_factoring->get_profile() != FORK && is_ifork_leaf){     // is inv-fork leaf
            // the two above conditions together prevent errors in case the CG is not connected.
            // i.e. leaf_price_cache is empty for fork topologies
            
            // no need to rebuild the reachable leaf state space every time
            // instead: do it once, cache which State has the same start leaf state
            // and copy everything from that state
            if (tmp_price.number_states[factor] == 1){
                size_t leaf_id = 0;
                for (;;){
                    // TODO there should be a better way to find the state
                    if (tmp_price.has_leaf_state(leaf_id, factor)){
                        break;
                    }
                    ++leaf_id;
                }
                if (leaf_price_cache[factor].size() > leaf_id && leaf_price_cache[factor][leaf_id] != StateID::no_state){                    
                    const Prices &tag_to_copy = cpg_storage->prices[g_state_registry->lookup_state(leaf_price_cache[factor][leaf_id])];
                    
                    int cost_delta = tmp_price.get_cost_of_state(leaf_id, factor);
                    cost_delta -= tag_to_copy.get_cost_of_state(leaf_id, factor);
                    
                    size_t num_states = tag_to_copy.get_number_states(factor) - 1;
                    for (size_t fact = 0 ; fact < number_leaf_states; fact++){
                        if (fact != leaf_id && tag_to_copy.has_leaf_state(fact, factor)){
                            int cost = tag_to_copy.get_cost_of_state(fact, factor);
                            tmp_price.add_state(LeafStateID(fact, factor), cost + cost_delta);
                            if (--num_states == 0){
                                break;
                            }
                        }       
                    }
                    continue;
                } else {
                    leaf_tags_to_cache.push_back(make_pair(leaf_id, factor));
                }
            }
        }
        
        vector<int> best_prices(number_leaf_states, numeric_limits<int>::max());
        
        bool change = true;

        while (change) {
           
            change = false;
            
            for (size_t id = 0; id < g_state_registry->size(factor); ++id){
                
                if (tmp_price.has_leaf_state(id, factor)){
                
                    int cost = tmp_price.get_cost_of_state(id, factor);
                    if (best_prices[id] <= cost){
                        continue;
                    }
                    
                    best_prices[id] = cost;
                    
                    assert(id < leaf_state_successors[factor].size());

                    for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
                        const Operator *op = leaf_state_successors[factor][id][o].first;
                        if (is_ifork_leaf || op->is_center_applicable(base_state)){
                            size_t successor = leaf_state_successors[factor][id][o].second;
                            change |= tmp_price.add_state(LeafStateID(successor, factor), 
                                                   cost + get_adjusted_action_cost(*op, g_factoring->get_cost_type()));
                        }
                    }
                    
                    // TODO probably reintroduce this for optimal search again
                    // keep in mind to change the pruning methods accordingly! + change the corresponding expression in PathPrices
                    // when introducing this, also have a shortcut at the beginning of the outmost for loop
//                 if (g_factoring->get_search_type() == SAT && !g_factoring->has_successors(factor) && get_goal_cost(factor) >= 0){
//                     // in satisficing search => stop once a goal is reachable in fork-leaves
//                     change = false;
//                     break;
//                 }
                }
            }
        }
        
#ifdef DEBUG_SEARCH
    cout << "   RPG for leaf factor " << factor << " finished" << endl;
#endif

    }
        
#ifdef DEBUG_SEARCH
    cout << "+++++++++++++++ RPG FINISHED" << endl;
#endif

    return tmp_price;
}

void Prices::apply_center_op_to_leaves(const Prices &old_tag, const Operator &op) {
    // TODO store some tree that captures which leaf states satisfy a certain leaf condition
    // then run only over the states that satisfy the leaf precondition of op.
    // this should be a lot faster than running over all leaf states and check if the condition
    // is achieved!
     
#ifdef DEBUG_SEARCH
    cout << "start to apply center op " << op.get_name() << " to leaves" << endl;
#endif

    for (size_t factor = 0; factor < g_factors.size(); ++factor){
          
        bool has_pre_on_factor = op.has_precondition_on(factor);
        const vector<const Effect*> &eff_on_factor = op.get_effects(factor);
        
        if (!has_pre_on_factor && eff_on_factor.empty()){
            // op does not affect leaf at all => copy everything
            
            number_states[factor] = old_tag.number_states[factor];
            prices[factor] = old_tag.prices[factor];
            goal_cost[factor] = old_tag.goal_cost[factor];
            continue;
        }
        
        int min_price = has_pre_on_factor ? numeric_limits<int>::max() : 0;
        
        if (eff_on_factor.size() == g_factors[factor].size()){
            // center op completely overwrites this leaf factor
                    
            const LeafState pred = g_state_registry->lookup_leaf_state(LeafStateID(0, factor));
            
            add_state(g_state_registry->get_successor_leaf_state(pred, op).get_id(), 0);
            
            if (has_pre_on_factor){
                size_t number_states = old_tag.get_number_states(factor);
                size_t id = 0;
                while (number_states > 0 && min_price > 0) {
                    if (old_tag.has_leaf_state(id, factor)){
                        --number_states;
                        min_price = min(min_price, old_tag.get_cost_of_state(id, factor));
                    }
                    ++id;
                }
            }
            
        } else {            
            // check which leaf states satisfy center precondition and apply effects
            
            int number_states = old_tag.get_number_states(factor);
            size_t id = 0;
            while (number_states > 0) {
                if (old_tag.has_leaf_state(id, factor)){
                    --number_states;
                    
                    LeafStateID lid(id, factor);
                    
                    if (!has_pre_on_factor || op.is_applicable(lid)){ // TODO there should be a more efficient way to check this! yep, cf above TODO 
                        
                        int cost = old_tag.get_cost_of_state(id, factor);
                        
                        min_price = min(min_price, cost);
                                
                        if (eff_on_factor.empty()){
                            add_state(lid, cost);
                        } else {
                            const LeafState state = g_state_registry->lookup_leaf_state(lid);
                
                            add_state(g_state_registry->get_successor_leaf_state(state, op).get_id(), cost);
                        }
                    }
                }
                ++id;
            }
        }

        if (min_price > 0){
            g_inc_g_by += min_price;
            reduce_cost_by(factor, min_price);
        }
        
        assert(min_price < numeric_limits<int>::max() && min_price >= 0);
#ifdef DEBUG_SEARCH
        cout << "min_price of factor " << factor << " = " << min_price << endl;
#endif
    }
}

bool Prices::has_leaf_state(int id, int factor) const {
    if (leaf_state_id_map[factor][id] == -1){
        return false;
    }
    size_t leaf_id = leaf_state_id_map[factor][id];
    return leaf_id < prices[factor].size() && prices[factor][leaf_id] != INF;
}

bool Prices::has_effective_leaf_state(int id, int factor) const {
    return has_leaf_state(id, factor);
}

int Prices::get_cost_of_state(int id, int factor) const {
    assert((size_t) leaf_state_id_map[factor][id] < prices[factor].size());
    return prices[factor][leaf_state_id_map[factor][id]];
}

int Prices::get_effective_cost_of_state(int id, int factor) const {
    return get_cost_of_state(id, factor);
}

size_t Prices::get_number_states(int factor) const {
    return number_states[factor];
}

size_t Prices::get_number_effective_states(int factor) const {
    return number_states[factor];
}

int Prices::get_goal_cost(int factor) const {
    return goal_cost[factor];
}

bool Prices::goal_reachable(int factor) const {
    return goal_cost[factor] != INF;
}

const vector<boost::dynamic_bitset<> > & Prices::get_frontier() {
    cout << "this code must not be reached (pricing_function.cc)" << endl;
    exit_with(EXIT_CRITICAL_ERROR);
}

void Prices::store_new_graph(const State &state) const {
    discard_new_graph();
    cpg_storage->store_cpg(state, *this);
}

CompliantPathGraph& Prices::get_init_state_prices() {
    if (do_pruning()){
        return PruningPrices::get_init_state_prices();
    } else {
        Prices prices(0);
            
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            prices.add_state(LeafStateID(0, factor), 0);
        }
            
        prices.update_price_tags(g_state_registry->get_initial_state(), 0, g_state_registry->get_initial_state());
        
        return tmp_price;
    }
}

void Prices::dump() const {
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        cout << "factor " << factor << " goal_cost = " << goal_cost[factor] << endl;
        for (size_t id = 0; id < g_state_registry->size(factor); ++id){
            if (has_leaf_state(id, factor)){
                LeafState state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
                bool is_goal_state = !g_goals_per_factor[factor].empty() ? is_a_leaf_goal_state[factor][id] : false;
                if (g_factors[factor].size() == 1){
                    cout << g_fact_names[g_factors[factor][0]][state[g_factors[factor][0]]] << " with price " << get_cost_of_state(id, factor) << (is_goal_state ? ", is goal" : "") << endl;
                } else {
                    state.dump_pddl();
                    cout << " with price " << get_cost_of_state(id, factor) << endl;
                }
            }
        }
    }
}

bool Prices::apply_symmetry_permutation(const LeavesPermutation &per) {
    // cout << " APPLY SYMMETRY TO: " << endl;
    // dump();
    //Copy prices and reinitialize affected factors
    vector<vector<int> > old_prices(prices);//g_factors.size());
    vector<unsigned int> old_number_states(number_states);
    
#ifndef NDEBUG
    vector<int> old_goal_cost(goal_cost);
#endif

    for (int factor : per.get_factors_affected()) {
	//old_prices[factor] = move(prices [factor]);
	number_states[factor] = 0;
	prices[factor] = vector<int>();
	goal_cost[factor] = INF;
    }
    
    for(int to_factor : per.get_factors_affected()) {
	int from_factor = per.get_from_factor(to_factor);

	// cout << " from: " << from_factor << " to " << to_factor << endl;

	assert(find(per.get_factors_affected().begin(), 
			 per.get_factors_affected().end(), to_factor)
	       != per.get_factors_affected().end());

	assert(find(per.get_factors_affected().begin(), 
			 per.get_factors_affected().end(), from_factor)
	       != per.get_factors_affected().end());

        size_t num_states = old_number_states[from_factor]; 
	for  (size_t id = 0; num_states > 0; ++ id){
	    assert (id < leaf_state_id_map[from_factor].size());
	    if (leaf_state_id_map[from_factor][id] == -1) {
		continue; 
	    }

	    size_t leaf_state = leaf_state_id_map[from_factor][id];
	    if (leaf_state >= old_prices[from_factor].size() ||
		old_prices[from_factor][leaf_state] == INF)  continue;

	    num_states--;
	    pair<int, int> to_pair = per.get_new_factor_state_by_old_factor_state(from_factor, id);
	    assert(to_pair.first == to_factor);
	    int to_val = to_pair.second;

	     // cout << "Set from_val: " << id << " from_factor " << from_factor
	     // 	 <<  "  to_val: " << to_val << " of "<<  to_factor << ": " << old_prices[from_factor][leaf_state] << endl;
	    
	     // LeafState state_print_from = g_state_registry->lookup_leaf_state(LeafStateID(id, from_factor));
	     // LeafState state_print_to = g_state_registry->lookup_leaf_state(LeafStateID(to_val, to_factor));

	     // state_print_from.dump_pddl(); 
	     // cout <<  " -> "; 
	     // state_print_to.dump_pddl(); 

	    add_state(LeafStateID(to_val, to_factor), old_prices[from_factor][leaf_state]);
	}
    }


    // cout << " NUMBER STATES: " << if_less << endl;
    // for (int i : old_number_states) cout << i << " "; cout << endl;
    // for (int i : number_states) cout << i << " ";  cout << endl << endl;

    // cout << " GOAL COST" << endl;
    // for (int i : old_goal_cost) cout << i << " "; cout << endl;
    // for (int i : goal_cost) cout << i << " ";  cout << endl << endl
    assert (accumulate(number_states.begin(), number_states.end(), 0) == accumulate(old_number_states.begin(), old_number_states.end(), 0));
    assert (accumulate(goal_cost.begin(), goal_cost.end(), 0) == accumulate(old_goal_cost.begin(), old_goal_cost.end(), 0));



    //cout << " APPLIED SYMMETRY: " << endl;
    //dump();

    return true; //We do not need to check whether the prices changed
		 //because the return value is only relevant if
		 //if_less is true and in that case we checked this in
		 //per.produce_less(prices)
}

unique_ptr<CompliantPathGraph> Prices::clone() const {
    return unique_ptr<CompliantPathGraph> (new Prices(this));
}


unique_ptr<CompliantPathGraph> Prices::get_permuted_cpg(const LeavesPermutation &per) const {
    Prices *res = new Prices(nullptr);
    for (size_t to_factor = 0; to_factor < g_factors.size(); ++to_factor){
	if(!per.is_affected(to_factor)) {
            res->goal_cost[to_factor] = goal_cost[to_factor];
            res->number_states[to_factor] = number_states[to_factor];
            res->prices[to_factor] = prices[to_factor];
	} else {
	    int from_factor = per.get_from_factor(to_factor);

	    assert(find(per.get_factors_affected().begin(), 
			per.get_factors_affected().end(), to_factor)
		   != per.get_factors_affected().end());

	    assert(find(per.get_factors_affected().begin(), 
			per.get_factors_affected().end(), from_factor)
		   != per.get_factors_affected().end());

	    size_t num_states = number_states[from_factor]; 
	    for  (size_t id = 0; num_states > 0; ++id){
		if (!has_leaf_state(id, from_factor)){
                    continue;
                }

		--num_states;
		pair<int, int> to_pair = per.get_new_factor_state_by_old_factor_state(from_factor, id);
		assert(to_pair.first == (int) to_factor);
		int to_val = to_pair.second;

		// cout << "Set from_val: " << id << " from_factor " << from_factor
		//      <<  "  to_val: " << to_val << " of "<<  to_factor << ": " << prices[from_factor][leaf_state] << endl;
            
		// LeafState state_print_from = g_state_registry->lookup_leaf_state(LeafStateID(id, from_factor));
		// LeafState state_print_to = g_state_registry->lookup_leaf_state(LeafStateID(to_val, to_factor));

		//   state_print_from.dump_pddl(); 
		//   cout <<  " -> "; 
		//   state_print_to.dump_pddl(); 

		res->add_state(LeafStateID(to_val, to_factor), get_cost_of_state(id, from_factor));
	    }
	}
    }


    // cout << " NUMBER STATES: " << endl;
    // for (int i : number_states) cout << i << " "; 
    // cout << endl;
    // for (int i : res->number_states) cout << i << " ";  
    // cout << endl << endl;

    // cout << " GOAL COST" << endl;
    // for (int i : goal_cost) cout << i << " "; 
    // cout << endl;
    // for (int i : res->goal_cost) cout << i << " ";  
    // cout << endl << endl;
    assert(accumulate(res->number_states.begin(), res->number_states.end(), 0) == accumulate(number_states.begin(), number_states.end(), 0));
    assert(accumulate(res->goal_cost.begin(), res->goal_cost.end(), 0) == accumulate(goal_cost.begin(), goal_cost.end(), 0));

    return unique_ptr<CompliantPathGraph>(res);
}
