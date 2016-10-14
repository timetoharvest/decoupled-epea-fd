#ifndef PRUNABLE_H
#define PRUNABLE_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "simulation_relation.h"

#include <vector>
#include <set>
#include <memory>


class LeafStateID;
class CompliantPathGraph;
class CPGStorage;
class State;


class Prunable {
    
protected:
    
    static bool store_frontier;
    
    static bool propagate_goal_prices;            
    
public:
    
    virtual ~Prunable() = default;
    
    // TODO it's a bit ugly, that some of these functions are shared with CompliantPathGraph
    
    virtual int get_goal_cost(size_t factor) const = 0;
    
    virtual bool has_effective_leaf_state(const LeafStateID &id) const = 0;
    
    virtual int get_cost_of_state(const LeafStateID &id) const = 0;
    
    virtual int get_effective_cost_of_state(const LeafStateID &id) const = 0;
    
    virtual size_t get_number_states(size_t factor) const = 0;

    virtual size_t get_number_effective_states(size_t factor) const = 0;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier() = 0;

    virtual std::unique_ptr<CompliantPathGraph> clone() const = 0;
    
    // returns the duplicate counter for this base state and compliant path graph 
    // i.e. the counter of the state subsuming that or the current max + 1
    int check_dominance(const State &base_state, int curr_number_duplicates, int g_value, const State &predecessor);

    
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
