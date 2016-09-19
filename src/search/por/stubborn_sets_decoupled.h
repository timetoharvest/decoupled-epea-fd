#ifndef STUBBORN_SETS_DECOUPLED_H
#define STUBBORN_SETS_DECOUPLED_H

#include "stubborn_sets_simple.h"

class CompliantPathGraph;
class Options;

namespace stubborn_sets_decoupled {    
class StubbornSetsDecoupled : public stubborn_sets_simple::StubbornSetsSimple {
    
    mutable const CompliantPathGraph *current_cpg;
    
    bool ignore_leaf_interference;
    
    void add_necessary_enabling_set(const std::pair<int, int> &fact);
    
    void mark_frontier_actions_as_stubborn(int factor);

    void compute_interference_relation();
protected:
    virtual void initialize_stubborn_set(const State &state) override;
    virtual void handle_stubborn_operator(const State &state, int op_no) override;
public:
    StubbornSetsDecoupled(const Options &opts);
    virtual ~StubbornSetsDecoupled() = default;
};
}

#endif
