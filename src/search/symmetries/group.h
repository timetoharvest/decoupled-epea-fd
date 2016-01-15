#ifndef SYMMETRIES_GROUP_H
#define SYMMETRIES_GROUP_H
#include <vector>
#include "permutation.h"
#include "../state.h"
#include "../option_parser.h"


using namespace std;
using namespace __gnu_cxx;
typedef std::vector<short int> Trace;
typedef std::vector<Permutation> Gens; // the generators for the automorphism


class Group{
public:
    Group(const Options &opts);
    ~Group();
    void initialize();

    static void add_permutation(void*, unsigned int, const unsigned int *);

    // Direct product creation methods.
    void default_direct_product();

    void add_generator(Permutation gen);
    int get_num_generators() const;
    void dump_generators() const;

    Permutation compose_permutation(Trace&) const;
    const state_var_t* get_canonical_state(const state_var_t* state);
    void get_trace(const State& state, Trace& full_trace);

    void free_memory();

private:
     Gens generators;

    static state_var_t* original_state;
    static state_var_t* res;
    static bool safe_to_add_generators;

    static int num_identity_generators;
    static int stop_after_false_generated;

    const Permutation& get_permutation(int) const;
    void copy_buff(state_var_t* s1, const state_var_t* s2) const;
};

#endif
