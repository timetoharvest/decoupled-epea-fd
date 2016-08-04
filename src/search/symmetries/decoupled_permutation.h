#ifndef SYMMETRIES_DECOUPLED_PERMUTATION_H
#define SYMMETRIES_DECOUPLED_PERMUTATION_H

#include <memory>

#include "permutation.h" 

class LeavesPermutation : public Permutation {
public:
    LeavesPermutation (const Permutation & per);
};


class CenterPermutation : public Permutation {
public:
    CenterPermutation (const Permutation & per);
};

class DecoupledPermutation {

public:
    static FactIndex findex_leaves;

    DecoupledPermutation(const Permutation & per);
    ~DecoupledPermutation() = default;

    bool replace_if_less(std::vector<int> & center, CompliantPathGraph & leaves) const;

private:
    std::unique_ptr<CenterPermutation> center_permutation;
    std::unique_ptr <LeavesPermutation> leaves_permutation;
};



#endif
