#include "permutation.h"
#include "../globals.h"
#include <iomanip>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>

using namespace std;
#include <sstream>

unsigned int Permutation::length;
vector<int> Permutation::var_by_val;
vector<int> Permutation::dom_sum_by_var;

void Permutation::_allocate() {
    borrowed_buffer = false;
    value = new int[length];
    inverse_value = new int[length];
    affected.assign(g_variable_domain.size(), false);
	vars_affected.clear();
	from_vars.assign(g_variable_domain.size(), -1);
	buff_for_state_copy = new state_var_t[g_variable_domain.size()];
	affected_vars_cycles.clear();
}

void Permutation::_deallocate() {
    if (!borrowed_buffer) {
        delete[] value;
        delete[] inverse_value;
        delete[] buff_for_state_copy;
    }
}

void Permutation::_copy_value_from_permutation(const Permutation &perm) {
    for (int i = 0; i < length; i++)
    	set_value(i, perm.get_value(i));
}

void Permutation::_inverse_value_from_permutation(const Permutation &perm) {
    for (int i = 0; i < length; i++)
		set_value(perm.get_value(i), i);
}

Permutation &Permutation::operator=(const Permutation &other) {
    if (this != &other) {
        if (borrowed_buffer) {
            _allocate();
        } else {
        	affected.assign(g_variable_domain.size(), false);
        	vars_affected.clear();
        	from_vars.assign(g_variable_domain.size(), -1);
        	affected_vars_cycles.clear();
        }
        _copy_value_from_permutation(other);
    }
    this->finalize();
    return *this;
}

Permutation::Permutation(){
	_allocate();
    for (int i = 0; i < length; i++)
    	set_value(i,i);
    finalize();
}


Permutation::Permutation(const unsigned int* full_permutation){
	_allocate();
	for (int i = 0; i < length; i++){
    	set_value(i,full_permutation[i]);
	}
	finalize();
}

Permutation::Permutation(const Permutation& perm, bool invert){
    _allocate();
    if (invert) {
    	_inverse_value_from_permutation(perm);
    } else {
        _copy_value_from_permutation(perm);
    }
    finalize();
}

// New constructor to use instead of * operator
Permutation::Permutation(const Permutation& perm1, const Permutation& perm2){
    _allocate();

	for (int i = 0; i < length; i++) {
		set_value(i, perm2.get_value(perm1.get_value(i)));
	}
	finalize();
}



Permutation::~Permutation(){
	_deallocate();
}

void Permutation::finalize(){
	// Sorting the vector of affected variables
	::sort(vars_affected.begin(), vars_affected.end());

	// Going over the vector from_vars of the mappings of the variables and finding cycles
	vector<bool> marked;
	marked.assign(g_variable_domain.size(), false);
	for (int i = 0; i < from_vars.size(); i++) {
		if (marked[i] || from_vars[i] == -1)
			continue;

		int current = i;
		marked[current] = true;
		vector<int> cycle;
		cycle.push_back(current);

        while (from_vars[current] != i){
        	current = from_vars[current];
        	marked[current] = true;
        	cycle.insert(cycle.begin(),current);
        }
        // Get here when from_vars[current] == i.
        affected_vars_cycles.push_back(cycle);
	}
}

bool Permutation::identity() const{
	return vars_affected.size() == 0;
}

bool Permutation::operator ==(const Permutation &other) const{

	for(int i = 0; i < length; i++) {
        if (get_value(i) != other.get_value(i)) return false;
    }

	return true;
}


void Permutation::print_cycle_notation() const {
	vector<int> done;
	for (int i = g_variable_domain.size(); i < length; i++){
		if (find(done.begin(), done.end(), i) == done.end()){
	        int current = i;
	        if(get_value(i) == i) continue; //don't print cycles of size 1

	        pair<int, state_var_t> varval = get_var_val_by_index(i);
	        cout<<"("<< g_fact_names[varval.first][(int) varval.second]  <<" ";

	        while(get_value(current) != i){
	            done.push_back(current);
	            current = get_value(current);

		        pair<int, state_var_t> currvarval = get_var_val_by_index(current);
	            cout<< g_fact_names[currvarval.first][(int) currvarval.second] <<" ";
	        }
	        done.push_back(current);
	        cout<<") ";
		}
	}
	cout << endl << "Variables:  ";
	for(int i = 0; i < vars_affected.size(); i++) cout << vars_affected[i] << "  ";
	cout << endl << "Variables permuted:  ";

	for(int i = 0; i < vars_affected.size(); i++) cout << from_vars[vars_affected[i]] << " -> " << vars_affected[i] << "  ";
	cout << endl;

	cout << "Affected variables by cycles: " << endl;
	for (int i=0; i < affected_vars_cycles.size(); i++) {
		cout << "( " ;
		for (int j=0; j < affected_vars_cycles[i].size(); j++) {
			cout << affected_vars_cycles[i][j] << " ";
		}
		cout << ")  ";
	}
	cout << endl;
}

void Permutation::dump(){
	for(int i = 0; i < length; i++){
		if (get_value(i) != i)
			cout << setw(4) << i;
	}
	cout << endl;
	for(int i = 0; i < length; i++){
		if (get_value(i) != i)
			cout << setw(4) << get_value(i);
	}
	cout << endl;
}

///////////////////////////////////////////////////////////////////////////////
// Changes added by Michael on 30.1.12
int Permutation::get_var_by_index(int ind) {
	// In case of ind < g_variable_domain.size(), returns the index itself, as this is the variable part of the permutation.
	if (ind < g_variable_domain.size()) {
		cout << "=====> WARNING!!!! Check that this is done on purpose!" << endl;
		return ind;
	}
	return var_by_val[ind-g_variable_domain.size()];
}

pair<int, state_var_t> Permutation::get_var_val_by_index(int ind) {
	assert(ind>=g_variable_domain.size());
	int var =  var_by_val[ind-g_variable_domain.size()];
	int val = ind - dom_sum_by_var[var];
	assert(val >=0 && val<g_variable_domain.size());

	return make_pair(var, val);
}

int Permutation::get_index_by_var_val_pair(int var, state_var_t val) {
	return dom_sum_by_var[var] + val;
}

void Permutation::set_value(int ind, int val) {
	value[ind] = val;
	inverse_value[val] = ind;
	set_affected(ind, val);
}

void Permutation::set_affected(int ind, int val) {

	if (ind < g_variable_domain.size() || ind == val)
		return;


	int var = get_var_by_index(ind);
	int to_var = get_var_by_index(val);

	if (!affected[var]) {
		vars_affected.push_back(var);
		affected[var] = true;
	}
	if (!affected[to_var]) {
		vars_affected.push_back(to_var);
		affected[to_var] = true;
	}
	// Keeping the orig. var for each var.
	from_vars[to_var] = var;
}


pair<int, state_var_t> Permutation::get_new_var_val_by_old_var_val(const int var, const state_var_t val) {
	int old_ind = get_index_by_var_val_pair(var, val);
	int new_ind = get_value(old_ind);
	return get_var_val_by_index(new_ind);
}

pair<int, state_var_t> Permutation::get_old_var_val_by_new_var_val(const int var, const state_var_t val) {
	int new_ind = get_index_by_var_val_pair(var, val);
	int old_ind = get_inverse_value(new_ind);
	return get_var_val_by_index(old_ind);
}

//////////////////////////////////////////////////////////////////////////////////////////
// This method compares the state to the state resulting from permuting it.
// If the original state is bigger than the resulted one, it is rewritten with the latter and true is returned.
////////////////////  New version - no extra buffer is needed, faster copy ///////////////
bool Permutation::replace_if_less(state_var_t* state) {
	if (identity())
		return false;

	int from_here = vars_affected.size(); // Will be set to value below vars_affected.size() if there is a need to overwrite the state,
	// starting from that index in the vars_affected vector.

	// Going over the affected variables, comparing the resulted values with the state values.
	for(int i = vars_affected.size()-1; i>=0; i--) {
		int to_var =  vars_affected[i];
		int from_var = from_vars[to_var];

		pair<int, state_var_t> to_pair = get_new_var_val_by_old_var_val(from_var, state[from_var]);
		assert( to_pair.first == to_var);
		short to_val = to_pair.second;

		// Check if the values are the same, then continue to the next aff. var.
		if (to_val == state[to_var])
			continue;

		if (to_val < state[to_var])
			from_here = i;

		break;
	}
	if (from_here == vars_affected.size())
		return false;

	for(int i = 0; i < affected_vars_cycles.size(); i++) {
		if (affected_vars_cycles[i].size() == 1) {
			int var = affected_vars_cycles[i][0];
			state_var_t from_val = state[var];
			pair<int, state_var_t> to_pair = get_new_var_val_by_old_var_val(var, from_val);
			state[var] = to_pair.second;
			continue;
		}
		// Remembering one value to be rewritten last
		int last_var = affected_vars_cycles[i][affected_vars_cycles[i].size()-1];
		int last_val = state[last_var];

		for (int j=affected_vars_cycles[i].size()-1; j>0; j--) {
			// writing into variable affected_vars_cycles[i][j]
			int to_var = affected_vars_cycles[i][j];
			int from_var = affected_vars_cycles[i][j-1];
			state_var_t from_val = state[from_var];
			pair<int, state_var_t> to_pair = get_new_var_val_by_old_var_val(from_var, from_val);
			state[to_var] = to_pair.second;
		}
		// writing the last one
		pair<int, state_var_t> to_pair = get_new_var_val_by_old_var_val(last_var, last_val);
		state[affected_vars_cycles[i][0]] = to_pair.second;
	}

	return true;
}

void Permutation::permute_state(const state_var_t* from_state, state_var_t* to_state) {
	// Does not assume anything about to_state

	for(int from_var = 0; from_var < g_variable_domain.size(); from_var++) {
		state_var_t from_val = from_state[from_var];
		pair<int, state_var_t> to_pair = get_new_var_val_by_old_var_val(from_var, from_val);

		// Copying the values to the new state
		to_state[to_pair.first] = to_pair.second;
	}
}
