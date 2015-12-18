#ifndef SIMULATION_RELATION_H
#define SIMULATION_RELATION_H

#include <vector>
#include <string>
#include <iostream>

#include "../ext/boost/dynamic_bitset.hpp"

class Condition;


class SimulationRelation {
protected:
    //For each operator, list of operators that dominate it in the
    //center
    std::vector<boost::dynamic_bitset<> > op_dominated_by;

    std::vector<std::vector<boost::dynamic_bitset<> > > relation;

    bool precondition_dominance(const std::vector<const Condition*> & pre, 
                                const std::vector<const Condition*> & pre2) const;

    void compute_label_dominance();

    void compute_simulation (int factor);

    inline bool simulates (int factor, int s, int t) const {
        return relation[factor][s][t];
    }

    inline bool similar (int factor,  int s, int t) const {
        return relation[factor][s][t] && relation[factor][t][s];
    }

    inline void remove (int factor, int s, int t) {
        relation[factor][s][t] = false;
    }

    inline const std::vector<boost::dynamic_bitset<> > & get_relation (int factor) const {
        return relation[factor];
    }

    int num_equivalences(int factor) const;
    int num_simulations(int factor, bool ignore_equivalences) const;
    int num_states(int factor) const { 
        return relation[factor].size();
    }

    void dump(int factor) const;

    const std::string & get_name(int factor, int s) const;

public:    
    void init();

    void statistics() const;

    void set_list_dominated_states() const;

    void perform_leaf_irrelevance_pruning() const;

};

#endif
