#include "decoupled_permutation.h"

#include "../globals.h"
#include "../state_registry.h"
#include "../compliant_paths/compliant_path_graph.h"

#include <cassert>

using namespace std;

FactIndex DecoupledPermutation::findex_leaves;

DecoupledPermutation::DecoupledPermutation(const Permutation & p) {
     if (p.num_leafs_affected()) 
	leaves_permutation = unique_ptr<LeavesPermutation>(new LeavesPermutation(p));

    if (p.affects_center()) 
	center_permutation = unique_ptr<CenterPermutation>(new CenterPermutation(p));
}


LeavesPermutation::LeavesPermutation (const Permutation & per) 
    : Permutation(forceempty_tag()) {
    affected.assign (g_factors.size(), false);
   
    for (int v : per.vars_affected) {
	int factor = g_belongs_to_factor[v];
	if (factor >= 0 && !affected[factor]) {
	    vars_affected.push_back(factor);
	    affected[factor] = true;
	}
    }

    from_vars.assign(g_factors.size(), -1);
    for (size_t v = 0; v < per.from_vars.size(); ++v) {
	int from_v = per.from_vars[v];
	int factor  = g_belongs_to_factor[v];
	int from_factor  = g_belongs_to_factor[from_v];

	assert(from_vars[factor] == -1 || from_vars[factor] == from_factor);
	from_vars[factor] = from_factor;
    }

    value.resize(DecoupledPermutation::findex_leaves.length);
    inverse_value.resize(DecoupledPermutation::findex_leaves.length);

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

	    int symm_id = 0; //CompliantPathGraph::get_leaf_state_id (symm_leaf_state, symm_factor);

	    int index = DecoupledPermutation::findex_leaves.get_index_by_var_val_pair(factor, s);	    
	    int symm_index = DecoupledPermutation::findex_leaves.get_index_by_var_val_pair(symm_factor, symm_id);

	    //set_value(index, symm_index);
	    value [index] = symm_index;
	    inverse_value[symm_index] = index;
	}
    }

    finalize();
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
}


bool DecoupledPermutation::replace_if_less(vector<int> & center, CompliantPathGraph & leaves) const {
    if(center_permutation){
	cmp_t cmp = center_permutation->replace_if_less(center);
	if (cmp == cmp_t::LESS) {
	    leaves.apply_symmetry_permutation(*leaves_permutation, false);
	    return true;
	} else if (cmp == cmp_t::GREATER) {
	    return false;	    
	}
    }
    
    return (leaves.apply_symmetry_permutation(*leaves_permutation, true));    
}
