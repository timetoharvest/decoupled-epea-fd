#ifndef SYMMETRIES_DECOUPLED_PERMUTATION_H
#define SYMMETRIES_DECOUPLED_PERMUTATION_H

#include <memory>

#include "permutation.h" 

class Prices;

class LeavesPermutation {
public:
    static FactIndex findex_leaves;

    LeavesPermutation (const Permutation & per);
    ~LeavesPermutation() = default;


    bool produces_less (const Prices & prices) const;

    const std::vector<int> & get_factors_affected() const {
	return factors_affected;
    }

    int get_from_factor(int f) const {
	return from_factors[f];
    }

    std::pair<int, int> get_new_factor_state_by_old_factor_state(const int factor, const int state) const;
    std::pair<int, int> get_old_factor_state_by_new_factor_state(const int factor, const int state) const ;

protected:
    std::vector<int> value;
    std::vector<int> inverse_value;
    std::vector<int> factors_affected;
    std::vector<bool> affected;

    std::vector<int> from_factors;

    // Affected factors by cycles
    std::vector<std::vector<int> > affected_factors_cycles;
};


class CenterPermutation : public Permutation {
public:
    CenterPermutation (const Permutation & per);
};

class DecoupledPermutation {

public:

    DecoupledPermutation(const Permutation & per);
    ~DecoupledPermutation() = default;

    bool replace_if_less(std::vector<int> & center, CompliantPathGraph & leaves) const;

private:
    std::unique_ptr<CenterPermutation> center_permutation;
    std::unique_ptr <LeavesPermutation> leaves_permutation;
};



#endif
