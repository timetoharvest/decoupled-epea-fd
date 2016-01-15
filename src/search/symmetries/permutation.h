#ifndef SYMMETRIES_PERMUTATION_H
#define SYMMETRIES_PERMUTATION_H
#include "../state.h"
#include <vector>
#include <fstream>

class Permutation{
public:
    Permutation();
    Permutation(const unsigned int*);
    Permutation(const Permutation&, bool invert=false);
    Permutation(const Permutation& perm1, const Permutation& perm2);
    ~Permutation();

    Permutation& operator =(const Permutation&);
    bool operator ==(const Permutation&) const;

    bool identity() const;
    // Michael: version for the trace_back
    void permute_state(const state_var_t* from_state, state_var_t* to_state);
    void print_cycle_notation() const;
    void set_value(int ind, int val);
    int get_value(int ind) const { return value[ind]; }
    int get_inverse_value(int ind) const { return inverse_value[ind]; }
    void dump();

    static unsigned int length;
    static std::vector<int> var_by_val;
    static std::vector<int> dom_sum_by_var;

    static int get_var_by_index(int val);
    static std::pair<int, state_var_t> get_var_val_by_index(const int ind);
    static int get_index_by_var_val_pair(const int var, const state_var_t val);

    std::pair<int, state_var_t> get_new_var_val_by_old_var_val(const int var, const state_var_t val);
    std::pair<int, state_var_t> get_old_var_val_by_new_var_val(const int var, const state_var_t val);

    bool replace_if_less(state_var_t*);

private:
    int* value;
    int* inverse_value;
    std::vector<int> vars_affected;
    std::vector<bool> affected;
    bool borrowed_buffer;
    // Need to keep the connection between affected vars, ie which var goes into which.
    std::vector<int> from_vars;
    state_var_t* buff_for_state_copy;
    // Affected vars by cycles
    std::vector<std::vector<int> > affected_vars_cycles;


    void set_affected(int ind, int val);

    void finalize();
    void _allocate();
    void _deallocate();
    void _copy_value_from_permutation(const Permutation&);
    void _inverse_value_from_permutation(const Permutation &perm);

};

#endif
