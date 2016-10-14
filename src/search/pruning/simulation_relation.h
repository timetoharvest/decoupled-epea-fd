#ifndef SIMULATION_RELATION_H
#define SIMULATION_RELATION_H

#include <vector>
#include <string>
#include <iostream>

#include "../ext/boost/dynamic_bitset.hpp"


class Condition;
class LeafStateID;


class SimulationRelation {
    
private:
    
    static std::vector<std::vector<std::vector<size_t> > > simulated_states;
    
    
    //For each operator, list of operators that dominate it in the
    //center
    std::vector<boost::dynamic_bitset<> > op_dominated_by;

    std::vector<std::vector<boost::dynamic_bitset<> > > relation;
    
    //Whether states are reachable after irrelevance pruning
    std::vector<boost::dynamic_bitset<>> reachable;

    bool precondition_dominance(const std::vector<const Condition*> & pre, 
                                const std::vector<const Condition*> & pre2) const;

    void compute_label_dominance();

    void compute_simulation (size_t factor);

    inline bool simulates(size_t factor, int s, int t) const {
        return relation[factor][s][t];
    }

    inline bool similar(size_t factor,  int s, int t) const {
        return relation[factor][s][t] && relation[factor][t][s];
    }

    inline void remove(size_t factor, int s, int t) {
        relation[factor][s][t] = false;
    }

    inline const std::vector<boost::dynamic_bitset<> > & get_relation(size_t factor) const {
        return relation[factor];
    }

    size_t num_equivalences(size_t factor) const;
    
    size_t num_simulations(size_t factor, bool ignore_equivalences) const;
    
    size_t num_states(size_t factor) const { 
        return relation[factor].size();
    }

    void dump(size_t factor) const;

    const std::string & get_name(const LeafStateID &id) const;

public:

    void init();

    void statistics() const;

    void set_list_dominated_states() const;

    void perform_leaf_irrelevance_pruning(bool only_remove_states = false);
    
    
    static const std::vector<std::vector<std::vector<size_t> > > get_simulation_relation() {
        return simulated_states;
    }

};

#endif
