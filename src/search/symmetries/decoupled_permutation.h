#ifndef SYMMETRIES_DECOUPLED_PERMUTATION_H
#define SYMMETRIES_DECOUPLED_PERMUTATION_H

#include "permutation.h"

#include <memory>

class LexicographicOrdering;
class Operator;
class Prices;

class LeavesPermutation {
    friend class DecoupledGroup;
    
private:
    static FactIndex findex_leaves;
    
    const Permutation *permutation;
    
    std::vector<int> value;
    std::vector<int> inverse_value;
    std::vector<int> factors_affected;
    std::vector<bool> affected;

    std::vector<int> from_factors;

    // Affected factors by cycles
    std::vector<std::vector<int> > affected_factors_cycles;

    void finalize();
    void set_value(int ind, int val);
    
public:

    LeavesPermutation(const Permutation &per);    
    LeavesPermutation(const LeavesPermutation &per, 
                      const LeavesPermutation &per2);    
    LeavesPermutation(const LeavesPermutation &per, bool invert);
    
    ~LeavesPermutation() = default;

    const std::vector<int> & get_factors_affected() const {
        return factors_affected;
    }

    bool is_affected(int factor) const {
        return affected[factor];
    }

    int get_from_factor(int f) const {
        return from_factors[f];
    }

    std::pair<int, int> get_new_factor_state_by_old_factor_state(int factor, 
                                                                 int state) const;
                                                                
    std::pair<int, int> get_old_factor_state_by_new_factor_state(int factor, 
                                                                 int state) const;
    cmp_t cmp_num_states(const CompliantPathGraph &cpg) const;
    
    cmp_t cmp_goal_cost(const CompliantPathGraph &cpg) const;
    
    cmp_t cmp_prices(const CompliantPathGraph &cpg) const;
    
    bool is_symmetric_op(const Operator *op, const Operator *op2) const;
    
    void print_cycle_notation() const;

};


class CenterPermutation : public Permutation {
    
public:
    CenterPermutation (const CenterPermutation &p1, 
                       const CenterPermutation &p2);
    CenterPermutation (const Permutation &per, bool invert = false);
    
    ~CenterPermutation() = default;

    bool is_symmetric_op(const Operator *op, const Operator *op2) const;

};

class DecoupledPermutation {
    
    std::unique_ptr<CenterPermutation> center_permutation;
    std::unique_ptr<LeavesPermutation> leaves_permutation;

public:
    DecoupledPermutation() {}
    explicit DecoupledPermutation(const Permutation &per);
    DecoupledPermutation(const DecoupledPermutation &per);
    DecoupledPermutation(const DecoupledPermutation &per, bool invert);
    DecoupledPermutation(const DecoupledPermutation &perm1, 
                         const DecoupledPermutation &perm2);

    ~DecoupledPermutation() = default;

    bool affects_center() const {
        return center_permutation.get() != nullptr;
    }

    bool affects_leaves() const {
        return leaves_permutation.get() != nullptr;
    }

    bool replace_if_less(std::vector<int> &center, 
                         CompliantPathGraph &leaves, 
                         const LexicographicOrdering &lex_ordering) const;

    cmp_t replace_if_less_center(std::vector<int> &center, 
				CompliantPathGraph &leaves) const;
    
    bool replace_if_less_leaves(CompliantPathGraph &leaves, 
				const LexicographicOrdering &lex_ordering) const;

    bool stabilizes_center(const State &center) const;

    void permute_decoupled_state(std::vector<int> &center,
                                 CompliantPathGraph &leaves) const;
                                 
    void permute_center_state(std::vector<int> &center) const;
    
    void print_cycle_notation() const;
    
    const Operator * find_matching_min_cost_permuted_op(const Operator *op, 
							const std::vector<const Operator*> &ops) const;
    
    const LeavesPermutation & get_leaves_permutation() const {
        return *leaves_permutation;
    }

};



#endif
