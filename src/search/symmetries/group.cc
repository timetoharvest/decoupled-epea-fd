#include "group.h"
#include "../globals.h"
#include <iostream>

#include <fstream>
#include <algorithm>

using namespace std;

state_var_t* Group::original_state;
state_var_t* Group::res;

bool Group::safe_to_add_generators;
int Group::num_identity_generators;
int Group::stop_after_false_generated;

Group::Group(const Options &opts) {
    stop_after_false_generated = opts.get<int>("stop_after_false_generated");
}

void Group::initialize() {
    if (!original_state)
        original_state = new state_var_t[g_variable_domain.size()];

    if (!res)
        res = new state_var_t[g_variable_domain.size()];

    safe_to_add_generators = true;
    num_identity_generators = 0;
}

Group::~Group(){
    //Empty - all vectors expected to be deleted by default destructor of vector
}


void Group::free_memory() {
    if (original_state) {
        delete original_state;
        original_state = 0;
    }
    if (res) {
        delete res;
        res = 0;
    }

    // Removing the permutations
    generators.clear();
}


/**
 * Add new permutation to the list of permutations
 * The function will be called from bliss
 */
void Group::add_permutation(void* param, unsigned int, const unsigned int * full_perm){
    if (!safe_to_add_generators) {
        cout << "Not safe to add permutations at this point!" << endl;
        exit_with(EXIT_CRITICAL_ERROR);
    }

    Permutation perm(full_perm);
    //Only if we have non-identity permutation we need to save it into the list of generators
    if(!perm.identity()){
        ((Group*) param)->add_generator(perm);
    } else {
        num_identity_generators++;
        if (num_identity_generators > stop_after_false_generated) {
            cout << endl << "Problems with generating symmetry group! Too many false generators." << endl;
            cout<<"Number of generators: 0"<<endl;
            exit_with(EXIT_CRITICAL_ERROR);
        }
    }
}

void Group::add_generator(Permutation gen) {
    if (!safe_to_add_generators) {
        cout << "Not safe to add permutations at this point!" << endl;
        exit_with(EXIT_CRITICAL_ERROR);
    }

    generators.push_back(gen);
#ifdef DEBUGMODE
    cout << "Added generator number " << get_num_generators();
#endif

}

int Group::get_num_generators() const {
    return generators.size();
}

void Group::default_direct_product(){
    safe_to_add_generators = false;  // From this point on it is not safe to add generators
//    dump_generators();
}

Permutation Group::compose_permutation(Trace& perm_index) const {
    Permutation new_perm;
    for (int i = 0; i < perm_index.size(); ++i) {
        new_perm = Permutation(new_perm, get_permutation(perm_index[i]));

    }
    return new_perm;
}

const Permutation& Group::get_permutation(int index) const {
    return generators[index];
}

const state_var_t* Group::get_canonical_state(const state_var_t* state) {
    int size = get_num_generators();
    if (size == 0)
        return state;

    copy_buff(original_state, state);
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i=0; i < size; i++) {
            if (generators[i].replace_if_less(original_state)) {
                changed =  true;
            }
        }
    }
    return original_state;
}


void Group::dump_generators() const {
    if (get_num_generators() == 0)
        return;
    for (int i = 0; i < get_num_generators(); i++) {
        cout << "Generator " << i << endl;
        get_permutation(i).print_cycle_notation();
    }

    cout << "Extra group info:" << endl;
    cout << "Permutation length: " << Permutation::length << endl;
    cout << "Permutation variables by values (" << g_variable_domain.size() << "): " << endl;
    for (int i = g_variable_domain.size(); i < Permutation::length; i++)
        cout << Permutation::get_var_by_index(i) << "  " ;
    cout << endl;
}

void Group::copy_buff(state_var_t* s1, const state_var_t* s2) const{
    for(int i = 0; i < g_variable_domain.size(); ++i) s1[i] = s2[i];
}

void Group::get_trace(const State& state, Trace& full_trace) {
    int size = get_num_generators();
    if (size == 0)
        return;

    for(int i = 0; i < g_variable_domain.size(); ++i)
        original_state[i] = state[i];
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i=0; i < size; i++) {
            if (generators[i].replace_if_less(original_state)) {
                full_trace.push_back(i);
                changed = true;
            }
        }
    }
}
