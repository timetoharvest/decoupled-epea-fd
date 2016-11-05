#include "decoupled_group.h"

#include "../compliant_paths/compliant_path_graph.h"
#include "lexicographic_ordering.h"
#include "group.h"
#include "../state_registry.h"


DecoupledGroup::DecoupledGroup(const Group &group) {
    // Ensure that all symmetry leaf states exists
    for(size_t factor = 0; factor < g_factors.size(); ++factor){
        for(size_t s = 0; s < g_state_registry->size(factor); ++s) {
            for(const Permutation &per : group.generators) {
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
                CompliantPathGraph::get_leaf_state_id(symm_leaf_state, symm_factor);
            }
        }
    }

    LeavesPermutation::findex_leaves.init_decoupled();

    for(const Permutation &per : group.generators){
        DecoupledPermutation *gen = new DecoupledPermutation(per);
        generators.push_back(unique_ptr<DecoupledPermutation>(gen));
        if(gen->affects_center()){
            generators_center.push_back(gen);
        } else {
            generators_only_leaves.push_back(gen);
        }
    }

    tmp_state.resize(g_variable_domain.size(), -1);
}


const vector<int> & DecoupledGroup::get_canonical_decoupled_state(const State &center, 
								  CompliantPathGraph &leaves, 
								  const LexicographicOrdering &lex_ordering) const {


    for (int var : g_center_vars) {
        tmp_state[var] = center[var];
    }

    // cout << endl << endl<< "start" << endl;
    // for (size_t i = 0; i < g_center_vars.size(); ++i) cout << g_fact_names[g_center_vars[i]][center[g_center_vars[i]]] << "       ";
    // cout << endl;
    // leaves.dump();

    // Center state optimization
    bool changed = true;
    vector<DecoupledPermutation *> generators_stable_center;
    while (changed) {
        changed = false;
        generators_stable_center.clear();
        for (auto gen : generators_center) {
            cmp_t cmp = gen->replace_if_less_center(tmp_state, leaves); 
            if (cmp == cmp_t::LESS){
                changed = true;
            } else if (cmp == cmp_t::EQUAL && gen->affects_leaves()){
                generators_stable_center.push_back(gen);
            }
        }
    }

    // Leaves optimization
    if(lex_ordering.optimize_leaves()){
        // Leaves optimization
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto & gen : generators_only_leaves) {
                changed |= gen->replace_if_less_leaves(leaves, lex_ordering);
            }
            for (const auto & gen : generators_stable_center) {
                changed |= gen->replace_if_less_leaves(leaves, lex_ordering);
            }
        }
    }

    // cout << "done" << endl;
    // for (size_t i = 0; i < g_center_vars.size(); ++i) cout << g_fact_names[g_center_vars[i]][tmp_state[g_center_vars[i]]] << "       ";
    // cout << endl;
    // leaves.dump();
    // cout << endl << endl;

    return tmp_state;
}

const std::vector<DecoupledPermutation *> DecoupledGroup::get_stabilized_permutations(const State &center) const {
    std::vector<DecoupledPermutation *> permutations = generators_only_leaves;
    for (auto gen : generators_center) {
        if (gen->affects_leaves() && gen->stabilizes_center(center)) {
            permutations.push_back(gen);
        } 
    }

    return permutations;
}


void DecoupledGroup::statistics() const {
    // for (int i = 0; i < get_num_generators(); i++) {
    //     cout << "Generator " << i << endl;
	// << " center: " <<  generators[i].affects_center()
	// << " num_leafs: " << generators[i].num_leafs_affected() << endl;
    // }
}


DecoupledTrace DecoupledGroup::get_trace(const State &center, 
                                const CompliantPathGraph &leaves, 
                                const LexicographicOrdering &lex_ordering) const {
    
    DecoupledTrace res;

    res.canonical_cpg = leaves.clone();
    res.canonical_center.resize(g_variable_domain.size(), -1);
    for (int var : g_center_vars) {
        res.canonical_center[var] = center[var];
    }

    // Center state optimization
    bool changed = true;
    vector<DecoupledPermutation *> generators_stable_center;
    while (changed) {
        changed = false;
        generators_stable_center.clear();
        for (auto gen : generators_center) {
            cmp_t cmp = gen->replace_if_less_center(res.canonical_center, *(res.canonical_cpg)); 
            if (cmp == cmp_t::LESS) {
                res.trace.push_back(gen);
                changed = true;
            } else if (cmp == cmp_t::EQUAL && gen->affects_leaves()) {
                generators_stable_center.push_back(gen);
            }
        }
    }

    // Leaves optimization
    if(lex_ordering.optimize_leaves()){
        // Leaves optimization
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto gen : generators_only_leaves) {
                if(gen->replace_if_less_leaves(*(res.canonical_cpg), lex_ordering)) {
                    changed = true;
                    res.trace.push_back(gen);
                }
            }
            for (const auto & gen : generators_stable_center) {
                if (gen->replace_if_less_leaves(*(res.canonical_cpg), lex_ordering)) {
                    changed = true;
                    res.trace.push_back(gen);   
                }
            }
        }
    }

    // cout << "done" << endl;
    // for (size_t i = 0; i < g_center_vars.size(); ++i) cout << g_fact_names[g_center_vars[i]][tmp_state[g_center_vars[i]]] << "       ";
    // cout << endl;
    // leaves.dump();
    // cout << endl << endl;

    return res;

    // bool changed = true;
    // while (changed) {
    //     changed = false;
    //     for (size_t i = 0; i < generators.size(); ++i) {
    //         if(generators[i]->replace_if_less(tmp_state, *leaves_copy, lex_ordering)) {
    // 		changed = true;
    // 		res.push_back(i);
    // 	    }
    //     }
    // }

}

DecoupledPermutation DecoupledGroup::compose_permutation(const DecoupledTrace &trace) const {
    unique_ptr<DecoupledPermutation> new_perm(new DecoupledPermutation());
    for (auto perm  : trace.trace) {
        new_perm.reset(new DecoupledPermutation(*new_perm, *perm));
    }
    return *new_perm;
}
