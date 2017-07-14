#ifndef STUBBORN_SETS_DECOUPLED_H
#define STUBBORN_SETS_DECOUPLED_H

#include "stubborn_sets_simple.h"

class CompliantPathGraph;
class LeafFactorID;
class Options;

namespace stubborn_sets_decoupled {
class StubbornSetsDecoupled : public stubborn_sets_simple::StubbornSetsSimple {
    
    // several special cases allowing easier handling/stronger pruning
    bool is_fork_factoring;
    bool is_ifork_factoring;
    bool has_ifork_leaf;
    bool is_single_var_ifork_factoring;
    
    std::vector<bool> is_fork_leaf;
    std::vector<bool> is_ifork_leaf;
    
    bool goal_ingoing_transitions;
    bool special_case_optimizations;
    bool interference_no_mutex_pre;
    
    mutable const CompliantPathGraph *current_cpg;
    
    
    // a frontier action is an inapplicable leaf action that reduces 
    // the price of a leaf state
    void mark_frontier_actions_as_stubborn(LeafFactorID factor);
    
    void mark_reached_enabling_set_as_stubborn(LeafFactorID factor);

protected:
    virtual void add_interfering(size_t op_no) override;
    virtual void compute_interference_relation() override;
    virtual void initialize_stubborn_set(const State &state) override;
    virtual void handle_stubborn_operator(const State &state, size_t op_no) override;
public:
    StubbornSetsDecoupled(const Options &opts);
    virtual ~StubbornSetsDecoupled() = default;
};
}

#endif
