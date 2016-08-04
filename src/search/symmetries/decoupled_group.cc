#include "decoupled_group.h"

#include "group.h"
#include "../state_registry.h"

#include "../compliant_paths/compliant_path_graph.h"

DecoupledGroup::DecoupledGroup(const Group & group) {
    
    //Ensure that all symmetry leaf states exists
    for(const Permutation & per : group.generators) {
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
		//CompliantPathGraph::get_leaf_state_id (symm_leaf_state, symm_factor);
	    }
	}
    }


    DecoupledPermutation::findex_leaves.init_decoupled();

    for(const Permutation & per : group.generators) {
	generators.push_back(unique_ptr<DecoupledPermutation>(new DecoupledPermutation(per)));
    }    

}


const vector<int> & DecoupledGroup::get_canonical_decoupled_state(const State & center, 
								  CompliantPathGraph & leaves)  const{
    for (size_t i = 0; i < g_center_vars.size(); ++i) {
	tmp_state[i] = center[g_center_vars[i]];
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto & gen : generators) {
	    changed |= gen->replace_if_less(tmp_state, leaves);
        }
    }
    return tmp_state;

}




