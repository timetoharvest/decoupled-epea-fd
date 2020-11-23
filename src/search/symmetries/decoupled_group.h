#ifndef SYMMETRIES_DECOUPLED_GROUP_H
#define SYMMETRIES_DECOUPLED_GROUP_H

#include "decoupled_permutation.h"

#include <vector>

class ExplicitStateCPG;
class DecoupledPermutation;
class Group;
class LexicographicOrdering;
class State;


struct DecoupledTrace {
    std::vector<const DecoupledPermutation *> trace;
    std::vector<int> canonical_center;
    std::unique_ptr<ExplicitStateCPG> canonical_cpg;
};

class DecoupledGroup {

    std::vector<std::unique_ptr<DecoupledPermutation>> generators_center;

    std::vector<std::unique_ptr<DecoupledPermutation>> generators_only_leaves;
     
    mutable std::vector<int> tmp_state;
    
public:
    DecoupledGroup(const Group &group);

    ~DecoupledGroup() = default;

    DecoupledTrace get_trace(const State &center,
                             const ExplicitStateCPG &leaves,
                             const LexicographicOrdering &lex_ordering) const;

    std::unique_ptr<DecoupledPermutation> compose_permutation(const DecoupledTrace &trace) const;

    const std::vector<int> & get_canonical_decoupled_state(const State &center, 
                                                           ExplicitStateCPG &leaves,
                                                           const LexicographicOrdering &lex_ordering) const;

    std::vector<const DecoupledPermutation *> get_stabilized_permutations(const State &center) const;
    
    void statistics () const;
    
};

#endif
