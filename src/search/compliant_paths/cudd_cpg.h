#ifndef CUDD_CPG_H
#define CUDD_CPG_H

#include "symbolic_state_cpg.h"



class CuddCPG : public SymbolicStateCPG {
    friend class SymbolicStateCPG;

    static bool initialized;


    static std::unique_ptr<CompliantPathGraph> get_init_state_cpg();

    static void initialize();

protected:

    virtual std::unique_ptr<CompliantPathGraph> get_successor_via_center_action(const State &new_center_state, const Operator &op) const override;

    virtual DOMINANCE check_dominance(const State &other, uint64_t hash, int g_advantage = 0, DOMINANCE needed = DOMINANCE::NONE) override;

    virtual void store_new_cpg(const State &state) override;

    virtual void discard_new_cpg() const override;

    virtual bool is_hypercube_covered(const State &base_state, int curr_number_duplicates) const override;

public:

    virtual ~CuddCPG() = default;

    CuddCPG(){};

    CuddCPG(const CuddCPG &cpg);

    CuddCPG(CuddCPG &&other) = delete;

    CuddCPG& operator=(const CuddCPG &other) = delete;

    CuddCPG& operator=(CuddCPG &&other);


    virtual void populate_reached_leaf_facts(std::function<void(int, int)> f) const override;

    virtual void populate_cost_of_leaf_facts(std::function<void(int, int, int)> f) const override;

    virtual std::vector<std::pair<int, int> > get_leaf_fact_set(LeafStateHash id, LeafFactorID factor) const override;

    virtual bool goal_reachable(LeafFactorID factor) const override;

    virtual int get_goal_cost(LeafFactorID factor) const override;


    virtual bool is_applicable(const Operator &op) const override;

    virtual void remove_inapplicable_ops(std::vector<OperatorID> &ops) const override;

    virtual uint64_t get_hash() const override;

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;


    virtual void dump() const override;

    // returns an approximation of the number of bytes stored in memory of this CPG
    virtual size_t get_memory_footprint() const override;


    static void fill_in_leaf_actions(std::vector<OperatorID> &ops, std::vector<StateID> &states);

    static void print_statistics();

};

#endif
