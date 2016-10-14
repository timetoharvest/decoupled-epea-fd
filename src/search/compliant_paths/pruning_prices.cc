#include "pruning_prices.h"

#include "cpg_storage.h"
#include "../factoring.h"
#include "../globals.h"
#include "../operator.h"
#include "../priority_queue.h"
#include "../pruning/pruning.h"
#include "../pruning/simulation_relation.h"
#include "../search_space.h"
#include "../state_registry.h"
#include "../successor_generator.h"
#include "../utilities.h"

#include <limits>


using namespace std;


PruningPrices PruningPrices::tmp_p_price;

PruningPriceStorage* PruningPrices::cpg_storage;


void PruningPrices::register_cpg_storage(PruningPriceStorage *storage) {
    cpg_storage = storage;
}

PruningPrices::PruningPrices(const PruningPrices *tag) {
    if (tag == 0){
        number_states = vector<size_t>(g_factors.size(), 0);
        prices = vector<vector<int> >(g_factors.size());
        goal_cost = vector<int>(g_factors.size(), INF);
    } else {
        number_states = tag->number_states;
        prices = tag->prices;
        goal_cost = tag->goal_cost;
        // TODO why dont we copy effective_prices/frontier/.. here?
        // trying it out results in incomplete algorithm, just why??
    }
}

bool PruningPrices::add_effective_state(const LeafStateID &state, int cost) {
    size_t factor = state.get_factor();
    if (leaf_state_id_map[factor][state.hash()] == -1){ // get new index to prevent facts vector from being sparse
        leaf_state_id_map[factor][state.hash()] = curr_leaf_state_max_id[factor];
        ++curr_leaf_state_max_id[factor];
    }
    assert(leaf_state_id_map[factor][state.hash()] >= 0);
    size_t leaf_id = leaf_state_id_map[factor][state.hash()];

    if(effective_prices.empty()){
        if (has_leaf_state(state) && cost >= get_cost_of_state(state)) {
            return false;
        }
        effective_prices = prices;
        number_effective_states = number_states;
    }
    
    if (leaf_id >= effective_prices[factor].size()){
        effective_prices[factor].resize(leaf_id + 1, INF);
    }
    
    if (effective_prices[factor][leaf_id] == INF){
        effective_prices[factor][leaf_id] = cost;
        ++number_effective_states[factor];
        return true;
    } else if (effective_prices[factor][leaf_id] > cost){
        effective_prices[factor][leaf_id] = cost;
        return true;
    }
    return false;
}

CompliantPathGraph& PruningPrices::update_price_tags(const State &base_state,
                                           const Operator *op,
                                           const State &predecessor) const {
    
    if (g_factoring->get_profile() != FORK && op != 0){
        assert(false); // not supported, yet
        tmp_p_price = PruningPrices(0);                  // when updating initial state => op == 0
        tmp_p_price.apply_center_op_to_leaves(*this, *op);
    } else {                
        tmp_p_price = PruningPrices(this);
    }
    // the rest of this function works on tmp_p_price!
    
    
#ifdef DEBUG_SEARCH
    cout << "+++++++++++++++ STARTING RPG" << endl;
#endif
    
    assert(leaf_tags_to_cache.empty());
    
    if (pruning->use_frontier()){
        PruningPrices &tag = cpg_storage->pruning_prices[predecessor];
        if (tag.frontier.empty()){
            tmp_p_price.compute_cost_frontier();
        } else {
            tmp_p_price.frontier = vector<boost::dynamic_bitset<> >(g_factors.size(), boost::dynamic_bitset<>());
            for (size_t factor = 0; factor < g_factors.size(); ++factor){
                if (tag.frontier[factor].size() != 0){
                    tmp_p_price.frontier[factor] = tag.frontier[factor];
                } else {
                    tmp_p_price.compute_cost_frontier(factor);
                }
            }
        }
    }

    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        
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
            if (tmp_p_price.number_states[factor] == 1){
                size_t leaf_id = 0;
                for (;;){
                    // TODO there should be a better way to find the state
                    if (tmp_p_price.has_leaf_state(LeafStateID(leaf_id, factor))){
                        break;
                    }
                    ++leaf_id;
                }
                if (leaf_price_cache[factor].size() > leaf_id && leaf_price_cache[factor][leaf_id] != StateID::no_state){                    
                    const PruningPrices &tag_to_copy = cpg_storage->pruning_prices[g_state_registry->lookup_state(leaf_price_cache[factor][leaf_id])];
                    
                    int cost_delta = tmp_p_price.get_cost_of_state(LeafStateID(leaf_id, factor));
                    cost_delta -= tag_to_copy.get_cost_of_state(LeafStateID(leaf_id, factor));
                    
                    size_t num_states = tag_to_copy.get_number_states(factor) - 1;
                    for (size_t fact = 0 ; fact < number_leaf_states; fact++){
                        if (fact != leaf_id && tag_to_copy.has_leaf_state(LeafStateID(fact, factor))){
                            int cost = tag_to_copy.get_cost_of_state(LeafStateID(fact, factor));
                            tmp_p_price.add_state(LeafStateID(fact, factor), cost + cost_delta);
                            if (--num_states == 0){
                                break;
                            }
                        }       
                    }
                    continue;
                } else {
                    leaf_tags_to_cache.push_back(LeafStateID(leaf_id, factor));
                }
            }
        }
        
        vector<int> best_prices(number_leaf_states, numeric_limits<int>::max());
        
        bool change = true;

        while (change) {
           
            change = false;
            
            for (size_t id = 0; id < g_state_registry->size(factor); ++id){
                
                if (tmp_p_price.has_leaf_state(LeafStateID(id, factor))){

                    if (pruning->use_frontier() && !tmp_p_price.frontier[factor][id]){
                        // TODO should probably only do this if simulation/propagation is disabled.. Why?
                        continue;
                    }
                
                    int cost = tmp_p_price.get_cost_of_state(LeafStateID(id, factor));
                    if (best_prices[id] <= cost && !pruning->use_frontier()){
                        continue;
                    }
                    
                    best_prices[id] = cost;
                    
                    assert(id < leaf_state_successors[factor].size());

                    bool all_applicable = pruning->use_frontier() && !pruning->propagate_goal_prices();
                    for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
                        const Operator *op = leaf_state_successors[factor][id][o].first;
                        if (is_ifork_leaf || op->is_center_applicable(base_state)){
                            size_t successor = leaf_state_successors[factor][id][o].second;
                            bool added = tmp_p_price.add_state(LeafStateID(successor, factor), 
                                                   cost + get_adjusted_action_cost(*op, cost_type));
                            change |= added;
                            if (pruning->use_frontier() && added){
                                tmp_p_price.frontier[factor][successor] = true;
                            }
                        } else if (pruning->use_frontier() && !pruning->propagate_goal_prices()) {
                            all_applicable = false;
                            int new_cost = cost + get_adjusted_action_cost(*op, cost_type);
                            int successor = leaf_state_successors[factor][id][o].second;
                            if (!tmp_p_price.has_leaf_state(LeafStateID(successor, factor)) || tmp_p_price.get_cost_of_state(LeafStateID(successor, factor)) > new_cost){
                                tmp_p_price.frontier[factor][id] = true;
                            }
                        }
                    }
                    if (all_applicable){
                        tmp_p_price.frontier[factor][id] = false;
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

    if(pruning->propagate_simulation_prices()){
        tmp_p_price.propagate_dominance_prices();
    }

    if(pruning->propagate_goal_prices()) {
        tmp_p_price.goal_price_propagation();
    }

    return tmp_p_price;
}

bool PruningPrices::has_effective_leaf_state(const LeafStateID &id) const {
    if (effective_prices.empty()){
        return has_leaf_state(id);
    } else {
        size_t factor = id.get_factor();
        size_t lid = id.hash();
        if (leaf_state_id_map[factor][lid] == -1){
            return false;
        }
        size_t leaf_id = leaf_state_id_map[factor][lid];
        return leaf_id < effective_prices[factor].size() && effective_prices[factor][leaf_id] != INF;
    }
}

int PruningPrices::get_effective_cost_of_state(const LeafStateID &id) const {
    if (effective_prices.empty()){
        return get_cost_of_state(id);
    } else {
        size_t factor = id.get_factor();
        assert((size_t) leaf_state_id_map[factor][id.hash()] < effective_prices[factor].size());
        return effective_prices[factor][leaf_state_id_map[factor][id.hash()]];
    }
}

size_t PruningPrices::get_number_effective_states(size_t factor) const {
    if (number_effective_states.empty()){
        return get_number_states(factor);
    } else {
        return number_effective_states[factor];
    }
}

const vector<boost::dynamic_bitset<> > & PruningPrices::get_frontier() {
    assert(pruning->use_frontier());
    if (frontier.empty()){
        compute_cost_frontier();
#ifdef DEBUG_PRUNING
        cout << "not precomputed" << endl;
#endif
    } else {
        for (size_t factor = 0; factor < g_factors.size(); ++factor){
            if (frontier[factor].size() == 0){
                compute_cost_frontier(factor);
            }
        }
#ifdef DEBUG_PRUNING
        cout << "precomputed" << endl;
#endif
    }
    return frontier;
}

void PruningPrices::store_new_graph(const State &state) const {
    discard_new_graph();
    cpg_storage->store_cpg(state, *this);
}

CompliantPathGraph& PruningPrices::get_init_state_prices() {
    PruningPrices prices(0);
        
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        prices.add_state(LeafStateID(0, factor), 0);
    }
        
    prices.update_price_tags(g_state_registry->get_initial_state(), 0, g_state_registry->get_initial_state());
    
    return tmp_p_price;
}

void PruningPrices::goal_price_propagation() {
    assert(g_factoring->get_profile() == FORK);
     
    for (size_t factor = 0; factor < g_factors.size(); ++factor) {
        
//      cout << "Propagate: " << factor << endl;
        
        vector<bool> closed(g_state_registry->size(factor), false);
        
        boost::dynamic_bitset<> sim_frontier;
        if(pruning->use_frontier()) {
            if (pruning->propagate_simulation_prices()){
                sim_frontier = frontier[factor];
            }
            frontier[factor].reset();  
        }

        //Alvaro: Given the size of leaf factors I think a bucketqueue
        //should work better but requires some implementation
        //effort. Probably worth it to reduce overhead
        HeapQueue<int, compare_pair_first_greater<int> > open; //Alvaro: Use a heap_queue? 
        
        for (size_t s : leaf_goal_states[factor]) {
            //cout << s << " is goal " << endl; 
            open.push(goal_cost[factor], s);
        }

        while (!open.empty()) {
            pair<int, int> entry = open.pop();
            int state = entry.second;
            
            if (closed[state]){
                continue;
            }
            
            int value = entry.first;

//          cout << "Propagating s " << g_fact_names[g_factors[factor][0]][g_state_registry->lookup_leaf_state(LeafStateID(state, factor))[g_factors[factor][0]]] << " with value " << value << endl; 
            
            closed[state] = true;
            
            if (value != INF) {
                add_effective_state(LeafStateID(state, factor), value);
            }

            for (auto transition : leaf_state_predecessors[factor][state]) {
                size_t t = transition.second;
                
                if (closed[t]) {
                    continue;
                }

                int action_cost = get_adjusted_action_cost(*transition.first, cost_type);
                int propagated_val = (value == INF ? numeric_limits<int>::max() : std::max(0, value - action_cost));

                int previous_t_val = !has_effective_leaf_state(LeafStateID(t, factor)) ? numeric_limits<int>::max() :
                                      get_effective_cost_of_state(LeafStateID(t, factor));

                int vt = min(previous_t_val, propagated_val);
                open.push((vt == numeric_limits<int>::max() ? INF : vt), t);
                
                if (pruning->use_frontier() && has_leaf_state(LeafStateID(t, factor)) && get_cost_of_state(LeafStateID(t, factor)) < propagated_val) {
                    if (!pruning->propagate_simulation_prices() || sim_frontier[t]){
                        frontier[factor][t] = true;
                    }
                }
            }
        }
//      assert(goal_cost[factor] == INF || number_propagated_states[factor] == g_state_registry->size(factor));
        // this assertion is not true! in case we have dead-end (i.e. irrelevant) leaf states that are not reached, no price is propagated to them
        
        //assert(!pruning->use_frontier() || ((previous_frontier^frontier[factor])&frontier[factor]).none());
    }
}

void PruningPrices::propagate_dominance_prices() {
    assert(g_factoring->get_profile() == FORK);
    
    const vector<vector<vector<size_t> > > &simulated_states = SimulationRelation::get_simulation_relation();
    
    for (size_t factor = 0; factor < simulated_states.size(); ++factor){
        for (size_t s = 0; s < simulated_states[factor].size(); ++s) {
            if(has_effective_leaf_state(LeafStateID(s, factor))){
                int cost_s = get_effective_cost_of_state(LeafStateID(s, factor));
                for (size_t t : simulated_states[factor][s]) {
                    if (!has_effective_leaf_state(LeafStateID(t, factor)) || get_effective_cost_of_state(LeafStateID(t, factor)) > cost_s) {
//                         cout << "Propagate cost " << cost_s << " from " << s << " to " << t << endl;
                        if (add_effective_state(LeafStateID(t, factor), cost_s) && pruning->use_frontier()){
                            frontier[factor][t] = false;
                        }
                    }
                }
            }
        }
//      assert(goal_cost[factor] != 0 || number_propagated_states[factor] == g_state_registry->size(factor));
    }  
}

void PruningPrices::compute_cost_frontier() {
    assert(g_factoring->get_profile() == FORK);
    
    // frontier is only stored for states for which the dominance was already checked at some point
    //assert(!frontier.empty() || g_state_registry->state_duplicate_counter[center.get_id().hash()] == 0);
    
    frontier.resize(g_factors.size());
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        compute_cost_frontier(factor);
    }
}

void PruningPrices::compute_cost_frontier(size_t factor) {
    assert(g_factoring->get_profile() == FORK);
    
    boost::dynamic_bitset<>(g_state_registry->size(factor), false).swap(frontier[factor]);
    for (size_t id = 0; id < leaf_state_successors[factor].size(); ++id){
        if (!has_leaf_state(LeafStateID(id, factor))){
            continue;
        }
        for (size_t o = 0; o < leaf_state_successors[factor][id].size(); ++o){
            int successor = leaf_state_successors[factor][id][o].second;
            if (!has_effective_leaf_state(LeafStateID(successor, factor))){
                frontier[factor][id] = true;
                break;
            }
            int action_cost = get_adjusted_action_cost(*leaf_state_successors[factor][id][o].first, cost_type);
            int new_cost = get_effective_cost_of_state(LeafStateID(id, factor)) + action_cost;
            if (get_effective_cost_of_state(LeafStateID(successor, factor)) > new_cost){
                frontier[factor][id] = true;
                break;
            }
        }
    }
}

void PruningPrices::dump() const {
    for (size_t factor = 0; factor < g_factors.size(); ++factor){
        cout << "factor " << factor << " goal_cost = " << goal_cost[factor] << endl;
        for (size_t id = 0; id < g_state_registry->size(factor); ++id){
            if (has_leaf_state(LeafStateID(id, factor))){
                LeafState state = g_state_registry->lookup_leaf_state(LeafStateID(id, factor));
                bool is_frontier = pruning->use_frontier() ? frontier[factor][id] : false;
                if (g_factors[factor].size() == 1){
                    cout << g_fact_names[g_factors[factor][0]][state[g_factors[factor][0]]] << (is_frontier ? " frontier state " : " ") << "with price " << get_cost_of_state(LeafStateID(id, factor)) << endl;
                } else {
                    state.dump_pddl();
                    cout << (is_frontier ? "frontier state " : "") << "with price " << get_cost_of_state(LeafStateID(id, factor)) << endl;
                }
            }
        }
    }
}

unique_ptr<CompliantPathGraph> PruningPrices::clone() const {
    return unique_ptr<CompliantPathGraph> (new PruningPrices(this));
}

