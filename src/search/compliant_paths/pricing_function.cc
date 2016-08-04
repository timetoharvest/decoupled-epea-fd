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

#include <limits>


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

Prunable& Prices::update_price_tags(const State &base_state,
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
                    
                    if (!has_pre_on_factor || op.is_applicable(lid)){
                        
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
    cpg_storage->store_cpg(state, &tmp_price);
}

void Prices::create_init_state_prices(const State &init_state) {
    if (do_pruning()){
        PruningPrices::create_init_state_prices(init_state);
    } else {
        Prices prices(0);
            
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            prices.add_state(LeafStateID(0, factor), 0);
        }
            
        prices.update_price_tags(g_state_registry->get_initial_state(), 0, g_state_registry->get_initial_state());
        
        tmp_price.store_new_graph(init_state);
    }
}

void Prices::dump() const {
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        cout << "factor " << factor << " goal_cost = " << goal_cost[factor] << endl;
        for (size_t id = 0; id < g_state_registry->size(factor); ++id){
            if (has_leaf_state(id, factor)){
                LeafState state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
                if (g_factors[factor].size() == 1){
                    cout << g_fact_names[g_factors[factor][0]][state[g_factors[factor][0]]] << " with price " << get_cost_of_state(id, factor) << endl;
                } else {
                    state.dump_pddl();
                    cout << " with price " << get_cost_of_state(id, factor) << endl;
                }
            }
        }
    }
}

