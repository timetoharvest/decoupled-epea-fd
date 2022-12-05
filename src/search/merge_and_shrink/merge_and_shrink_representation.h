#ifndef MERGE_AND_SHRINK_MERGE_AND_SHRINK_REPRESENTATION_H
#define MERGE_AND_SHRINK_MERGE_AND_SHRINK_REPRESENTATION_H

#include "../leaf_state_id.h"

#include <memory>
#include <unordered_map>
#include <vector>

class ExplicitStateCPG;
class GlobalState;
class LeafState;

namespace utils {
class LogProxy;
}

namespace merge_and_shrink {
class Distances;
class MergeAndShrinkRepresentation {
protected:
    int domain_size;
    std::vector<LeafFactorID> covered_leaf_ids;
public:
    explicit MergeAndShrinkRepresentation(int domain_size);
    virtual ~MergeAndShrinkRepresentation() = 0;
    
    virtual void get_contained_vars(
        std::vector<bool> &contained_vars) const = 0;
    virtual void set_covered_leaf_ids(
        const std::vector<bool> &contained_vars,
        std::vector<bool> &covered_vars) = 0;

    virtual std::vector<std::vector<std::vector<int>>> get_leaf_state_ids() const = 0;

    void store_all_affected_leaf_ids_in_root(const std::vector<bool> &contained_vars);

    int compute_decoupled_value_exact_icaps23(const GlobalState &state) const;
    int compute_decoupled_value_exact_compliant_merging(const GlobalState &state) const;
    int compute_decoupled_value_explicit(const GlobalState &state) const;

    int get_domain_size() const;

    // Store distances instead of abstract state numbers.
    virtual void set_distances(const Distances &) = 0;
    virtual void apply_abstraction_to_lookup_table(
        const std::vector<int> &abstraction_mapping) = 0;
    /*
      Return the value that state is mapped to. This is either an abstract
      state (if set_distances has not been called) or a distance (if it has).
      If the represented function is not total, the returned value is DEAD_END
      if the abstract state is PRUNED_STATE or if the (distance) value is INF.
    */
    virtual int get_value(const GlobalState &state) const = 0;
    virtual int get_value(const std::vector<int> &state) const = 0;
    virtual int get_value(const GlobalState &state, const LeafState &l_state) const = 0;

    virtual std::unordered_map<int, std::pair<int, std::vector<std::vector<std::pair<int, int>>>>> get_decoupled_value_exact_icaps23(
        const ExplicitStateCPG *prices,
        const std::vector<std::vector<int>> &reached_values_by_var) const = 0;
    virtual std::vector<std::pair<int, int>> get_decoupled_value_exact_compliant_merging(
            const GlobalState &state,
            const ExplicitStateCPG *prices) const = 0;

    /* Return true iff the represented function is total, i.e., does not map
       to PRUNED_STATE. */
    virtual bool is_total() const = 0;
    virtual void dump(utils::LogProxy &log) const = 0;
};


class MergeAndShrinkRepresentationLeaf : public MergeAndShrinkRepresentation {
    const int var_id;

    std::vector<int> lookup_table;
public:
    MergeAndShrinkRepresentationLeaf(int var_id, int domain_size);
    virtual ~MergeAndShrinkRepresentationLeaf() = default;

    virtual void get_contained_vars(
        std::vector<bool> &contained_vars) const override;
    virtual void set_covered_leaf_ids(
        const std::vector<bool> &contained_vars, std::vector<bool> &covered_vars) override;

    virtual std::vector<std::vector<std::vector<int>>> get_leaf_state_ids() const override;

    virtual void set_distances(const Distances &) override;
    virtual void apply_abstraction_to_lookup_table(
        const std::vector<int> &abstraction_mapping) override;
    virtual int get_value(const GlobalState &state) const override;
    virtual int get_value(const std::vector<int> &state) const override;
    virtual int get_value(const GlobalState &state, const LeafState &l_state) const override;

    virtual std::unordered_map<int, std::pair<int, std::vector<std::vector<std::pair<int, int>>>>> get_decoupled_value_exact_icaps23(
        const ExplicitStateCPG *prices,
        const std::vector<std::vector<int>> &reached_values_by_var) const override;
    virtual std::vector<std::pair<int, int>> get_decoupled_value_exact_compliant_merging(
            const GlobalState &state,
            const ExplicitStateCPG *prices) const override;

    virtual bool is_total() const override;
    virtual void dump(utils::LogProxy &log) const override;
};


class MergeAndShrinkRepresentationMerge : public MergeAndShrinkRepresentation {
    std::unique_ptr<MergeAndShrinkRepresentation> left_child;
    std::unique_ptr<MergeAndShrinkRepresentation> right_child;
    std::vector<std::vector<int>> lookup_table;
public:
    MergeAndShrinkRepresentationMerge(
        std::unique_ptr<MergeAndShrinkRepresentation> left_child,
        std::unique_ptr<MergeAndShrinkRepresentation> right_child);
    virtual ~MergeAndShrinkRepresentationMerge() = default;

    virtual void get_contained_vars(
        std::vector<bool> &contained_vars) const override;
    virtual void set_covered_leaf_ids(
        const std::vector<bool> &contained_vars,
        std::vector<bool> &covered_vars) override;

    virtual std::vector<std::vector<std::vector<int>>> get_leaf_state_ids() const override;

    virtual void set_distances(const Distances &distances) override;
    virtual void apply_abstraction_to_lookup_table(
        const std::vector<int> &abstraction_mapping) override;
    virtual int get_value(const GlobalState &state) const override;
    virtual int get_value(const std::vector<int> &state) const override;
    virtual int get_value(const GlobalState &state, const LeafState &l_state) const override;

    virtual std::unordered_map<int, std::pair<int, std::vector<std::vector<std::pair<int, int>>>>> get_decoupled_value_exact_icaps23(
        const ExplicitStateCPG *prices,
        const std::vector<std::vector<int>> &reached_values_by_var) const override;
    virtual std::vector<std::pair<int, int>> get_decoupled_value_exact_compliant_merging(
            const GlobalState &state,
            const ExplicitStateCPG *prices) const override;

    virtual bool is_total() const override;
    virtual void dump(utils::LogProxy &log) const override;
};
}

#endif
