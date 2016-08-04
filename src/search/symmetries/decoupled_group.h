#ifndef SYMMETRIES_DECOUPLED_GROUP_H
#define SYMMETRIES_DECOUPLED_GROUP_H

#include <vector>
#include "decoupled_permutation.h"

class CompliantPathGraph;
class Group;
class State;

class DecoupledGroup{
public:
    DecoupledGroup(const Group & group);
    ~DecoupledGroup() = default;

    const std::vector<int> & get_canonical_decoupled_state(const State & center, 
							   CompliantPathGraph & leaves)  const;    
    
private:
    std::vector<DecoupledPermutation> generators;
     
    mutable std::vector<int> tmp_state;
};

#endif
