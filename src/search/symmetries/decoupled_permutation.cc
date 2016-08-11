#include "decoupled_permutation.h"

#include "lexicographic_ordering.h"

#include "../globals.h"
#include "../state_registry.h"
#include "../compliant_paths/compliant_path_graph.h"
#include "../compliant_paths/pricing_function.h"

#include <cassert>

using namespace std;

FactIndex LeavesPermutation::findex_leaves;

DecoupledPermutation::DecoupledPermutation(const Permutation & p) { 
    if (p.num_leafs_affected())  
	leaves_permutation = unique_ptr<LeavesPermutation>(new LeavesPermutation(p));
    
    if (p.affects_center()) 
	center_permutation = unique_ptr<CenterPermutation>(new CenterPermutation(p));
}

DecoupledPermutation::DecoupledPermutation(const DecoupledPermutation & per) : 
    center_permutation(new CenterPermutation(*(per.center_permutation))), 
    leaves_permutation(new LeavesPermutation(*(per.leaves_permutation))) {
    }

DecoupledPermutation::DecoupledPermutation(const DecoupledPermutation & per, bool invert) : 
    center_permutation(new CenterPermutation(*(per.center_permutation), invert)), 
    leaves_permutation(new LeavesPermutation(*(per.leaves_permutation), invert)) {
    }


DecoupledPermutation::DecoupledPermutation(const DecoupledPermutation & p1, const DecoupledPermutation & p2) { 
    if (p1.center_permutation && p2.center_permutation) 
	center_permutation = unique_ptr<CenterPermutation>(new CenterPermutation(*(p1.center_permutation), *(p2.center_permutation)));
    else if (p1.center_permutation) 
	center_permutation = unique_ptr<CenterPermutation>(new CenterPermutation(*(p1.center_permutation)));
    else if (p2.center_permutation) 
	center_permutation = unique_ptr<CenterPermutation>(new CenterPermutation(*(p2.center_permutation)));

    if (p1.leaves_permutation && p2.leaves_permutation) 
	leaves_permutation = unique_ptr<LeavesPermutation>(new LeavesPermutation(*(p1.leaves_permutation), *(p2.leaves_permutation)));
    else if (p1.leaves_permutation) 
	leaves_permutation = unique_ptr<LeavesPermutation>(new LeavesPermutation(*(p1.leaves_permutation)));
    else if (p2.leaves_permutation) 
	leaves_permutation = unique_ptr<LeavesPermutation>(new LeavesPermutation(*(p2.leaves_permutation)));
    
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

    finalize();
    //cout << "Factor permutation: "; for (int f : from_factors) cout << f << " "; cout << endl;
    //cout << "Value permutation: "; for (int v : value) cout << v << " "; cout << endl;
}


void LeavesPermutation::finalize(){

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

LeavesPermutation::LeavesPermutation (const LeavesPermutation & perm1, 
				      const LeavesPermutation & perm2)  {
    for (size_t i = 0; i < findex_leaves.length; i++) {
	set_value(i, perm2.value[perm1.value[i]]);
    }
    finalize();
}

LeavesPermutation::LeavesPermutation (const LeavesPermutation & perm, 
				      bool invert)  {
    if(invert) {
	for (size_t i = 0; i < findex_leaves.length; i++)
	    set_value(perm.value[i], i);
    } else {
	for (size_t i = 0; i < findex_leaves.length; i++)
	    set_value(i, perm.value[i]);
    }

    finalize();
}

void LeavesPermutation::set_value(int ind, int val) {
	value[ind] = val;
	inverse_value[val] = ind;

	int var = findex_leaves.get_var_by_index(ind);
	int to_var = findex_leaves.get_var_by_index(val);

	if (!affected[var]) {
	    factors_affected.push_back(var);
	    affected[var] = true;
	}
	if (!affected[to_var]) {
	    factors_affected.push_back(to_var);
	    affected[to_var] = true;
	}
	// Keeping the orig. var for each var.
	from_factors[to_var] = var;
}

CenterPermutation::CenterPermutation (const Permutation & per, bool invert) : 
    Permutation(per, invert) {
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
}

CenterPermutation::CenterPermutation (const CenterPermutation & p1, 
				      const CenterPermutation & p2) : 
    Permutation(p1, p2) {
}



bool DecoupledPermutation::replace_if_less(vector<int> &center, CompliantPathGraph &leaves,
					   const LexicographicOrdering & lex_ordering) const {
    if(center_permutation){
	cmp_t cmp = center_permutation->replace_if_less(center);
	if (cmp == cmp_t::LESS) {
	    if (leaves_permutation)
		leaves.apply_symmetry_permutation(*leaves_permutation);
	    return true;
	} else if (cmp == cmp_t::GREATER) {
	    return false;	    
	}
    }
    
    return (leaves_permutation && 
	    lex_ordering.produces_less(*leaves_permutation, leaves) && 
	    leaves.apply_symmetry_permutation(*leaves_permutation));    
}



void DecoupledPermutation::permute_decoupled_state(vector<int> &center, 
						   CompliantPathGraph &leaves) const {
    if(center_permutation)
	center_permutation->permute(center);
    
    if (leaves_permutation)
	leaves.apply_symmetry_permutation(*leaves_permutation);
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


cmp_t LeavesPermutation::cmp_num_states (const CompliantPathGraph & cpg) const {
    for(int to_factor : factors_affected) {
	int from_factor = from_factors[to_factor];

	if(cpg.get_number_states(from_factor) < cpg.get_number_states(to_factor)) return cmp_t::GREATER;
	if(cpg.get_number_states(from_factor) > cpg.get_number_states(to_factor)) return cmp_t::LESS;
    }
    return cmp_t::EQUAL;
}

cmp_t LeavesPermutation::cmp_goal_cost (const CompliantPathGraph & cpg) const {
    for(int to_factor : factors_affected) {
	int from_factor = from_factors[to_factor];

	if(cpg.get_goal_cost(from_factor) < cpg.get_goal_cost(to_factor)) return cmp_t::GREATER;
	if(cpg.get_goal_cost(from_factor) > cpg.get_goal_cost(to_factor)) return cmp_t::LESS;
    }
    return cmp_t::EQUAL;
}

cmp_t LeavesPermutation::cmp_prices (const CompliantPathGraph & cpg) const {
    for(int to_factor : factors_affected) {	
	int from_factor = from_factors[to_factor];
	assert (g_state_registry->size(to_factor) == g_state_registry->size(from_factor));

	for  (size_t from_id = 0; from_id < g_state_registry->size(from_factor); ++from_id){
	    pair<int, int> to_pair = get_new_factor_state_by_old_factor_state(from_factor,
									      from_id);
	    assert(to_pair.first == to_factor);
	    int to_id = to_pair.second;

	    bool has_from = cpg.has_leaf_state (from_id, from_factor); 
	    bool has_to = cpg.has_leaf_state (from_id, from_factor); 

	    if(!has_from && has_to) return cmp_t::LESS;
	    if(has_from && !has_to) return cmp_t::GREATER;
	    
	    int cost_from = cpg.get_cost_of_state(from_id, from_factor);
	    int cost_to = cpg.get_cost_of_state(to_id, to_factor);

	    if(cost_from < cost_to) return cmp_t::GREATER;
	    if(cost_from > cost_to) return cmp_t::LESS;
	}
    }
    return cmp_t::EQUAL;
}
