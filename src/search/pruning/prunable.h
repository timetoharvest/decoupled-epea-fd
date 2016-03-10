#ifndef PRUNABLE_H
#define PRUNABLE_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "simulation_relation.h"

#include <vector>

class Prunable {
    
protected:
    
    static bool store_frontier;
    
    static bool propagate_goal_prices;            
    
public:
    
    virtual ~Prunable() = default;
    
    // TODO it's a bit ugly, that some of these functions are shared with CompliantPathGraph
    
    virtual int get_goal_cost(int factor) const = 0;
    
    virtual bool has_effective_leaf_state(int id, int factor) const = 0;
    
    virtual int get_cost_of_state(int id, int factor) const = 0;
    
    virtual int get_effective_cost_of_state(int id, int factor) const = 0;
    
    virtual size_t get_number_states(int factor) const = 0;

    virtual size_t get_number_effective_states(int factor) const = 0;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier() = 0;
    
    
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
