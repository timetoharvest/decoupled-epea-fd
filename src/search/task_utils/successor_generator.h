#ifndef TASK_UTILS_SUCCESSOR_GENERATOR_H
#define TASK_UTILS_SUCCESSOR_GENERATOR_H

#include "../leaf_state_id.h"

#include <memory>
#include <vector>

class CompliantPathGraph;
class LeafState;
class OperatorID;
class State;

namespace successor_generator {
class GeneratorBase;

class SuccessorGenerator {
    std::unique_ptr<GeneratorBase> root;

    // removes all operators from applicable_ops, whose leaf preconditions are not
    // reached in cpg
    void remove_inapplicable_ops(const CompliantPathGraph &cpg, std::vector<OperatorID> &applicable_ops) const;

public:
    explicit SuccessorGenerator();
    explicit SuccessorGenerator(LeafFactorID factor);
    explicit SuccessorGenerator(LeafFactorID factor, bool only_keep_all_leaf_preconditions);
    /*
      We cannot use the default destructor (implicitly or explicitly)
      here because GeneratorBase is a forward declaration and the
      incomplete type cannot be destroyed.
    */
    ~SuccessorGenerator();

    void generate_applicable_ops(
            const State &state, std::vector<OperatorID> &applicable_ops) const;

    // used to generate successors when reconstructing decoupled plans in orbit search
    void generate_applicable_ops(
                const State &state, const CompliantPathGraph &cpg,
                std::vector<OperatorID> &applicable_ops) const;

    // used in decoupled search to compute leaf state spaces (leaf actions)
    void generate_applicable_ops(
            const LeafState &state, std::vector<OperatorID> &applicable_ops) const;

    // used in decoupled search to compute leaf state spaces (center actions w. leaf pre/eff)
    void generate_applicable_ops_ignore_outside_pre(
            const LeafState &state, std::vector<OperatorID> &applicable_ops) const;

    void remove_never_applicable_center_ops(LeafFactorID factor);
};
}

#endif
