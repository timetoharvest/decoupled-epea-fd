#ifndef SYMMETRIES_DECOUPLED_PERMUTATION_H
#define SYMMETRIES_DECOUPLED_PERMUTATION_H

#include <memory>

#include "permutation.h" 

class LexicographicOrdering;
class Prices;

class LeavesPermutation {
    
protected:
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
    static FactIndex findex_leaves;

    LeavesPermutation (const Permutation &per);
    
    LeavesPermutation (const LeavesPermutation &per, 
		       const LeavesPermutation &per2);
    
    LeavesPermutation (const LeavesPermutation &per, bool invert);

    ~LeavesPermutation() = default;


    bool produces_less(const Prices &prices) const;

    const std::vector<int> & get_factors_affected() const {
	return factors_affected;
    }

    int get_from_factor(int f) const {
	return from_factors[f];
    }

    std::pair<int, int> get_new_factor_state_by_old_factor_state(const int factor, 
								 const int state) const;
    std::pair<int, int> get_old_factor_state_by_new_factor_state(const int factor, 
								 const int state) const ;


    cmp_t cmp_num_states (const CompliantPathGraph &cpg) const;
    cmp_t cmp_goal_cost (const CompliantPathGraph &cpg) const;
    cmp_t cmp_prices (const CompliantPathGraph &cpg) const;
    
};


class CenterPermutation : public Permutation {
    
public:
    CenterPermutation (const CenterPermutation &p1, 
		       const CenterPermutation &p2);

    CenterPermutation (const Permutation &per, bool invert = false);
    
    virtual ~CenterPermutation() = default;

};

class DecoupledPermutation {
    
    std::unique_ptr<CenterPermutation> center_permutation;
    std::unique_ptr<LeavesPermutation> leaves_permutation;

public:
    DecoupledPermutation() {}
    DecoupledPermutation(const Permutation &per);
    DecoupledPermutation(const DecoupledPermutation &per);
    DecoupledPermutation(const DecoupledPermutation &per, bool invert);
    DecoupledPermutation(const DecoupledPermutation &perm1, const DecoupledPermutation &perm2);

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

    bool stable_center(std::vector<int> &center) const;

    void permute_decoupled_state(std::vector<int> &center,
				 CompliantPathGraph &leaves) const;
                                 
    void permute_center_state(std::vector<int> &center) const;

    void invert();

};



#endif
