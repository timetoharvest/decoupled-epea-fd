#include "decoupled_permutation.h"

#include "../globals.h"
#include "../state_registry.h"
#include "../compliant_paths/compliant_path_graph.h"
#include "../compliant_paths/pricing_function.h"

#include <cassert>

using namespace std;

FactIndex LeavesPermutation::findex_leaves;

DecoupledPermutation::DecoupledPermutation(const Permutation & p) {
   
    if(!p.num_leafs_affected()) {
	cout <<"YES " << endl;
	if (p.num_leafs_affected())  
	    leaves_permutation = unique_ptr<LeavesPermutation>(new LeavesPermutation(p));
    
	if (p.affects_center()) 
	    center_permutation = unique_ptr<CenterPermutation>(new CenterPermutation(p));
    }

}

LeavesPermutation::LeavesPermutation (const Permutation & per)  {
    affected.assign (g_factors.size(), false);
   
    for (int v : per.vars_affected) {
	int factor = g_belongs_to_factor[v];
	if (factor >= 0 && !affected[factor]) {
	    factors_affected.push_back(factor);
	    affected[factor] = true;
	}
    }

    from_factors.assign(g_factors.size(), -1);
    for (size_t v = 0; v < per.from_vars.size(); ++v) {
	int from_v = per.from_vars[v];
	int factor  = g_belongs_to_factor[v];
	int from_factor  = g_belongs_to_factor[from_v];

	if (factor == -1 || from_v == -1) continue;
	assert(from_factor != -1);

	assert(from_factors[factor] == -1 || from_factors[factor] == from_factor);
	from_factors[factor] = from_factor;
    }

    value.resize(findex_leaves.length);
    inverse_value.resize(findex_leaves.length);
 
    for(size_t factor = 0; factor < g_factors.size(); ++factor){
	for(size_t s = 0; s < g_state_registry->size(factor); ++s) {
	    LeafState l = g_state_registry->lookup_leaf_state(LeafStateID(s, factor));
	    int symm_factor = -1;
	    vector<int> symm_leaf_state;

	    for(int v : g_factors[factor]) {
		auto mapped_to = per.get_new_var_val_by_old_var_val(v, l[v]);
		int symm_var = mapped_to.first;
		int symm_val = mapped_to.second;
		assert(symm_factor == -1 || symm_factor ==  g_belongs_to_factor[symm_var]);

		symm_factor =  g_belongs_to_factor[symm_var];
		if(symm_leaf_state.empty()) {
		    symm_leaf_state.resize(g_factors[symm_factor].size());
		}
		symm_leaf_state[g_new_index[symm_var]] = symm_val;
	    }

	    int symm_id = CompliantPathGraph::get_leaf_state_id(symm_leaf_state, symm_factor);

	    int index = findex_leaves.get_index_by_var_val_pair(factor, s);
	    int symm_index = findex_leaves.get_index_by_var_val_pair(symm_factor, symm_id);

	    //set_value(index, symm_index);
	    value [index] = symm_index;
	    inverse_value[symm_index] = index;
	}
    }


    // Sorting the vector of affected factoriables
    ::sort(factors_affected.begin(), factors_affected.end());
    // Going over the vector from_factors of the mappings of the variables and finding cycles
    vector<bool> marked;
    marked.assign(from_factors.size(), false);
    for (size_t i = 0; i < from_factors.size(); i++) {
	if (marked[i] || from_factors[i] == -1)
	    continue;

	int current = i;
	marked[current] = true;
	vector<int> cycle;
	cycle.push_back(current);
	
	while (from_factors[current] != (int)i){
	    current = from_factors[current];
	    marked[current] = true;
	    cycle.insert(cycle.begin(),current);
        }

        // Get here when from_factors[current] == i.
        affected_factors_cycles.push_back(cycle);
    }
}

CenterPermutation::CenterPermutation (const Permutation & per) : 
    Permutation(per) {
    //Remove leaf vars from vars_affected
    vars_affected.erase(std::remove_if(vars_affected.begin(), 
				       vars_affected.end(),
				       [&](int x){ 
					   if(g_belongs_to_factor[x] >= 0) {
					       affected[x] = false;
					       return true;
					   }
					   return false;}),
			vars_affected.end());

    for(size_t i = 0; i < from_vars.size(); ++i) {
	if (from_vars[i] == -1 || g_belongs_to_factor[from_vars[i]] >= 0) {
	    from_vars[i] = -1;
	}
    }

    affected_vars_cycles.clear();

    finalize();

    // print_cycle_notation();
}


bool DecoupledPermutation::replace_if_less(vector<int> &center, CompliantPathGraph &leaves) const {
    if(center_permutation){
	cmp_t cmp = center_permutation->replace_if_less(center);
	if (cmp == cmp_t::LESS) {
	    if (leaves_permutation)
		leaves.apply_symmetry_permutation(*leaves_permutation, false);
	    return true;
	} else if (cmp == cmp_t::GREATER) {
	    return false;	    
	}
    }
    
    return (leaves_permutation && leaves.apply_symmetry_permutation(*leaves_permutation, true));    
}


bool LeavesPermutation::produces_less (const Prices & /*prices*/ ) const {
    // Going over the affected variables, comparing the resulted values with the state values.
    // for(int i = vars_affected.size()-1; i>=0; i--) {
    // 	int to_factor =  vars_affected[i];
    // 	int from_factor = from_vars[to_factor];

    //     // TODO: This iteration order is inneficient because we may
    //     // iterate over many INF prices. Think of a better way to
    //     // iterate over leaf states that goes for the relevant prices
    // 	for (int s = 0; s < FACTOR_SIZE[to_factor]; ++s) { 
    // 	    pair<int, int> from_pair = get_old_var_val_by_new_var_val(to_factor, s);

    // 	    assert( from_pair.first == from_factor);
    // 	    int from_val = from_pair.second;

    // 	    // Check if the prices are the same, then continue to the next aff. var.
    // 	    if (prices[from_factor][from_val] < prices[to_factor][s]) {
    // 		return true;
    // 	    } else if (prices[from_factor][from_val] > prices[to_factor][s]) {
    // 		return false;
    // 	    }
    // 	}
    // }
    return false; 
}


pair<int, int> LeavesPermutation::get_new_factor_state_by_old_factor_state(const int factor, const int state) const {
	int old_ind = findex_leaves.get_index_by_var_val_pair(factor, state);
	int new_ind = value[old_ind];
	return findex_leaves.get_var_val_by_index(new_ind);
}

pair<int, int> LeavesPermutation::get_old_factor_state_by_new_factor_state(const int factor, const int state) const {
	int new_ind = findex_leaves.get_index_by_var_val_pair(factor, state);
	int old_ind = inverse_value[new_ind];
	return findex_leaves.get_var_val_by_index(old_ind);
}
