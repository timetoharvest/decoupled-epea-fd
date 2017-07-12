#ifndef SIMULATION_RELATION_H
#define SIMULATION_RELATION_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "../leaf_state_id.h"

#include <vector>
#include <string>
#include <iostream>


class Condition;


class SimulationRelation {
    
private:
    
    static std::vector<std::vector<std::vector<LeafStateHash> > > simulated_states;
    
    
    //For each operator, list of operators that dominate it in the
    //center
    std::vector<boost::dynamic_bitset<> > op_dominated_by;

    std::vector<std::vector<boost::dynamic_bitset<> > > relation;
    
    //Whether states are reachable after irrelevance pruning
    std::vector<boost::dynamic_bitset<>> reachable;

    bool precondition_dominance(const std::vector<const Condition*> & pre, 
                                const std::vector<const Condition*> & pre2) const;

    void compute_label_dominance();

    void compute_simulation (LeafFactorID factor);

    inline bool simulates(LeafFactorID factor, LeafStateHash s, LeafStateHash t) const {
        return relation[factor][s][t];
    }

    inline bool similar(LeafFactorID factor, LeafStateHash s, LeafStateHash t) const {
        return relation[factor][s][t] && relation[factor][t][s];
    }

    inline void remove(LeafFactorID factor, LeafStateHash s, LeafStateHash t) {
        relation[factor][s][t] = false;
    }

    inline const std::vector<boost::dynamic_bitset<> > & get_relation(LeafFactorID factor) const {
        return relation[factor];
    }

    size_t num_equivalences(LeafFactorID factor) const;
    
    size_t num_simulations(LeafFactorID factor, bool ignore_equivalences) const;
    
    size_t num_states(LeafFactorID factor) const { 
        return relation[factor].size();
    }

    void dump(LeafFactorID factor) const;

    const std::string & get_name(LeafStateHash id, LeafFactorID factor) const;

public:

    void init();

    void statistics() const;

    void set_list_dominated_states() const;

    void perform_leaf_irrelevance_pruning(bool only_remove_states = false);
    
    
    static const std::vector<std::vector<std::vector<LeafStateHash> > >& get_simulation_relation() {
        return simulated_states;
    }

};

#endif
