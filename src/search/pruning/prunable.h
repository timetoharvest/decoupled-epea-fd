#ifndef PRUNABLE_H
#define PRUNABLE_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "simulation_relation.h"

#include <vector>
#include <memory>

class State;
class LeavesPermutation;

class Prunable {
    
protected:
    
    static bool store_frontier;
    
    static bool propagate_goal_prices;            
    
public:
    
    virtual ~Prunable() = default;
    
    virtual int get_goal_cost(int factor) const = 0;
    
    virtual bool has_effective_leaf_state(int id, int factor) const = 0;
    
    virtual int get_effective_cost_of_state(int id, int factor) const = 0;

    virtual size_t get_number_effective_states(int factor) const = 0;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier() = 0;

    virtual std::unique_ptr<Prunable> get_permuted_cpg(const LeavesPermutation &permutation) const = 0;  

    
    // returns the duplicate counter for this base state and compliant path graph 
    // i.e. the counter of the state subsuming that or the current max + 1
    int check_dominance(const State &base_state, int curr_number_duplicates, 
                        int g_value, const State &predecessor);
    
    // returns the duplicate counter for this base state and compliant path graph 
    // i.e. the counter of the state subsuming that or the current max + 1
    // this method will try to apply symmetries so that the given state is dominated
    // by an existing one
    int check_symmetric_dominance(const State &base_state, int curr_number_duplicates, 
                                  int g_value, const State &predecessor);
    
    static bool do_pruning() {
        return store_frontier || propagate_goal_prices || !SimulationRelation::simulated_states.empty();
    }
    
    static void set_store_frontier() {
        store_frontier = true;
    }

    static void set_propagate_goal_prices() {
        propagate_goal_prices = true;
    }

};

#endif
