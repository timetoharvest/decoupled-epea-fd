#ifndef SYMMETRIES_PERMUTATION_H
#define SYMMETRIES_PERMUTATION_H

#include "../state.h"

#include <vector>
#include <fstream>

enum class cmp_t {LESS, EQUAL, GREATER};

class CompliantPathGraph;

class FactIndex {
//Alvaro: Putting all the static data members in one class, so that it
//is easier to have two separated instances for center and leaves.
public: 
    int initial_index;
    unsigned int length;
    std::vector<int> var_by_val;
    std::vector<int> dom_sum_by_var;

    void init();
    void init_decoupled();

    int get_var_by_index(int val) const;
    std::pair<int, int> get_var_val_by_index(const int ind) const;
    int get_index_by_var_val_pair(const int var, const int val) const;
};

class Permutation {
    friend class LeavesPermutation;
    friend class CenterPermutation;
    
protected:
    
    std::vector<int> value;
    std::vector<int> inverse_value;
    std::vector<int> vars_affected;
    std::vector<bool> affected;
    
    bool borrowed_buffer;// TODO apparently, this is never used?!
    
    // Need to keep the connection between affected vars, ie which var goes into which.
    std::vector<int> from_vars;

    // Affected vars by cycles
    std::vector<std::vector<int> > affected_vars_cycles;

    void set_affected(int ind, int val);

    void finalize();
    void _allocate();
    void _deallocate();
    void _copy_value_from_permutation(const Permutation &perm);
    void _inverse_value_from_permutation(const Permutation &perm);
    
public:
    static FactIndex findex;
    static void init_fact_index();

    Permutation();
    Permutation(const unsigned int*);
    Permutation(const Permutation &perm, bool invert = false);
    Permutation(const Permutation &perm1, const Permutation& perm2);
    virtual ~Permutation() = default;

    Permutation& operator=(const Permutation &perm); // TODO do we really need this?
    
    bool operator==(const Permutation&) const; // TODO do we really need this?

    bool identity() const;
    
    // Michael: version for the trace_back
    void permute_state(const State &from_state, std::vector<int> &to_state) const;
    
    void print_cycle_notation() const;
    
    void set_value(int ind, int val);
    
    int get_value(int ind) const { 
        return value[ind];
    }
    
    int get_inverse_value(int ind) const {
        return inverse_value[ind];
    }
    
    void dump() const;

    std::pair<int, int> get_new_var_val_by_old_var_val(const int var, const int val) const;
    std::pair<int, int> get_old_var_val_by_new_var_val(const int var, const int val) const ;

    cmp_t replace_if_less(std::vector<int> &state) const;
    bool stabilizes (const State &state) const;
    void permute(std::vector<int> &state) const;

    bool affects_center() const;
    int num_leafs_affected() const;
    
};

#endif
