#ifndef SYMMETRIES_DECOUPLED_GROUP_H
#define SYMMETRIES_DECOUPLED_GROUP_H

#include <vector>
#include "decoupled_permutation.h"

class LexicographicOrdering;
class CompliantPathGraph;
class Group;
class State;

typedef std::vector<short int> Trace;

class DecoupledGroup{
    
    std::vector<std::unique_ptr<DecoupledPermutation> > generators;

    std::vector<DecoupledPermutation *> generators_center;
    std::vector<DecoupledPermutation *> generators_only_leaves;
     
    mutable std::vector<int> tmp_state;
    
public:
    DecoupledGroup(const Group & group);
    ~DecoupledGroup() = default;

    Trace get_trace(const State &center, const CompliantPathGraph &leaves, 
		    const LexicographicOrdering &lex_ordering) const;

    DecoupledPermutation compose_permutation(const Trace &trace) const;

    const std::vector<int> & 
	get_canonical_decoupled_state(const State &center, 
				      CompliantPathGraph &leaves, 
				      const LexicographicOrdering &lex_ordering)  const;    
    
    void statistics () const;
    
};

#endif
