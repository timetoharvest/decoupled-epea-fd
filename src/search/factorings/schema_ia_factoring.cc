#include "schema_ia_factoring.h"

#include "../task_utils/causal_graph.h"
#include "../operator.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../utils/hash.h"

#include <set>
#include <vector>

using namespace std;

namespace schema_ia_factoring {

SchemaIAFactoring::SchemaIAFactoring(const Options &opts) : Factoring(opts) {
    profile = STAR;
    cout << "using greedy no-center factoring strategy" << endl;
}

inline size_t get_num_leaves(const vector<bool> &center, const vector<vector<bool> > &schema_cg) {
    size_t n = center.size();
    for (size_t s1 = 0; s1 < center.size(); ++s1){
        if (!center[s1]){
            for (size_t s2 = 0; s2 < center.size(); ++s2){
                if (schema_cg[s1][s2] && s1 != s2 && !center[s2]){
                    --n;
                    break;
                }
            }
        } else {
            --n;
        }
    }
    return n;
}

Factoring::FactoredVars SchemaIAFactoring::get_factoring() {
    // TODO allow empty center and do like now, but with action schema (pre+eff vars) and only intersection

    vector<EffectSchema> eff_schemas = get_effect_schemas();

    vector<vector<size_t> > var_to_schemas(g_variable_domain.size());
    for (size_t i = 0; i < eff_schemas.size(); ++i){
        for (int var : eff_schemas[i].vars){
            var_to_schemas[var].push_back(i);
        }
    }

    vector<vector<bool> > schema_cg(eff_schemas.size(), vector<bool>(eff_schemas.size(), false));

    // check intersection
    for (size_t s1 = 0; s1 < eff_schemas.size(); ++s1){
        for (int var : eff_schemas[s1].vars){
            for (size_t s2 : var_to_schemas[var]){
                schema_cg[s1][s2] = true;
            }
        }
    }

    const causal_graph::CausalGraph &cg = causal_graph::get_causal_graph();
    // check dependencies
    for (size_t s1 = 0; s1 < eff_schemas.size(); ++s1){
        for (int var : eff_schemas[s1].vars){
            for (int pre : cg.get_predecessors(var)){
                for (size_t s2 : var_to_schemas[pre]){
                    schema_cg[s1][s2] = true;
                }
            }
            for (int succ : cg.get_successors(var)){
                for (size_t s2 : var_to_schemas[succ]){
                    schema_cg[s1][s2] = true;
                }
            }
        }
    }

    // TODO alternatively do sorting according to # leaf-only actions + combination of the two?
    // another alternative would be to sort by increasing number of blocking leaves
    vector<vector<size_t> > schemas_by_num_neighors;
    for (size_t s1 = 0; s1 < eff_schemas.size(); ++s1){
        size_t neighbors = 0;
        for (size_t s2 = 0; s2 < eff_schemas.size(); ++s2){
            if (schema_cg[s1][s2] && s1 != s2){
                ++neighbors;
            }
        }
        if (schemas_by_num_neighors.size() <= neighbors){
            schemas_by_num_neighors.resize(neighbors + 1);
        }
        schemas_by_num_neighors[neighbors].push_back(s1);
    }

    vector<bool> center(eff_schemas.size(), false);
    size_t max_num_leaves = get_num_leaves(center, schema_cg);
    for (int n = schemas_by_num_neighors.size() - 1; n >= 0; --n){
        for (size_t s : schemas_by_num_neighors[n]){
            if (!center[s]){
                center[s] = true;
                size_t new_num_leaves = get_num_leaves(center, schema_cg);
                if (new_num_leaves > max_num_leaves){
                    max_num_leaves = new_num_leaves;
                } else {
                    center[s] = false;
                    for (size_t s2 = 0; s2 < eff_schemas.size(); ++s2){
                        if (schema_cg[s][s2] && s != s2){
                            center[s2] = true;
                        }
                    }
                }
            }
        }
    }

    FactoredVars factoring;

    for (size_t s = 0; s < eff_schemas.size(); ++s){
        if (!center[s]){
            factoring.push_back(set<int>(eff_schemas[s].vars.begin(), eff_schemas[s].vars.end()));
        }
    }

    return factoring;
}

vector<SchemaIAFactoring::EffectSchema> SchemaIAFactoring::get_effect_schemas() const {
    vector<EffectSchema> effect_schemas;
    utils::HashMap<std::vector<int>, size_t> scheme_loockup;
    for (const Operator &op : g_operators) {
        vector<int> vars;
        for (const Effect &eff : op.get_effects()) {
            vars.push_back(eff.var);
        }
        sort(vars.begin(), vars.end());

        if (scheme_loockup.find(vars) == scheme_loockup.end()){
            scheme_loockup[vars] = effect_schemas.size();
            effect_schemas.push_back(EffectSchema(1, vars));
        } else {
            effect_schemas[scheme_loockup[vars]].incr_num_action();
        }
    }
    return effect_schemas;
}

//void SchemaIAFactoring::merge_potential_leaves(vector<PotentialLeaf> &potential_leaves,
//        vector<set<size_t> > &var_to_p_leaves) const {
//
//    queue<PotentialLeaf> open;
//    for (PotentialLeaf &p_leaf : potential_leaves) {
//        sort(p_leaf.vars.begin(), p_leaf.vars.end());
//        if (p_leaf.vars.size() > 1) {
//            open.push(p_leaf);
//        }
//    }
//
//    int id = potential_leaves.size();
//    vector<PotentialLeaf> merged_leaves;
//    for (int iteration = 0; iteration < max_merge_steps; iteration++) {
//        while (!open.empty()) {
//            PotentialLeaf p_leaf = open.front();
//            open.pop();
//            set<int> overlapping_leaves;
//            for (int var : p_leaf.vars) {
//                for (int index : var_to_p_leaves[var]) {
//                    overlapping_leaves.insert(index);
//                }
//            }
//
//            for (int i : overlapping_leaves) {
//                vector<int> vars = potential_leaves[i].vars;
//                if (vars.size() == 1 || includes(p_leaf.vars.begin(), p_leaf.vars.end(), vars.begin(), vars.end()) || includes(vars.begin(), vars.end(), p_leaf.vars.begin(), p_leaf.vars.end())) {
//                    continue;
//                }
//                vector<int> merged_vars (p_leaf.vars.size() + vars.size());
//                auto it = set_union(p_leaf.vars.begin(), p_leaf.vars.end(), vars.begin(), vars.end(), merged_vars.begin());
//                merged_vars.resize(it - merged_vars.begin());
//
//                //duplicate check
//                bool dublicate = false;
//                for (int j : var_to_p_leaves[merged_vars[0]]) {
//                    if (potential_leaves[j].vars == merged_vars) {
//                       dublicate = true;
//                       break;
//                    }
//                }
//                if (!dublicate) {
//                    for (PotentialLeaf &merged_leaf : merged_leaves) {
//                        if (merged_leaf.vars == merged_vars) {
//                           dublicate = true;
//                           break;
//                        }
//                    }
//                }
//                if (dublicate) {
//                    continue;
//                }
//
//                PotentialLeaf merged_leaf (id, 0, merged_vars);
//
//                //calculate number of leaf actions and leaf-only actions
//                set<int> overlapping_with_merged_leaf;
//                for (int var : merged_vars) {
//                    for (int j : var_to_p_leaves[var]) {
//                        overlapping_with_merged_leaf.insert(j);
//                    }
//                }
//                for (int j : overlapping_with_merged_leaf) {
//                    merged_leaf.num_leaf_actions += potential_leaves[j].num_actions;
//                    if (includes(merged_vars.begin(), merged_vars.end(), potential_leaves[j].vars.begin(), potential_leaves[j].vars.end())) {
//                        merged_leaf.num_leaf_only_actions += potential_leaves[j].num_actions;
//                    }
//                }
//                if (p_leaf.get_flexibility() > merged_leaf.get_flexibility() || potential_leaves[i].get_flexibility() > merged_leaf.get_flexibility()) {
//                    continue;
//                }
//
//                merged_leaves.push_back(merged_leaf);
//                id++;
//            }
//        }
//        for (PotentialLeaf &merged_leaf : merged_leaves) {
//            potential_leaves.push_back(merged_leaf);
//            open.push(merged_leaf);
//            for (int var : merged_leaf.vars) {
//                var_to_p_leaves[var].insert(merged_leaf.id);
//            }
//        }
//        merged_leaves.clear();
//    }
//}

static Factoring *_parse_schema_ia_factoring(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);

    Options opts = parser.parse();

    SchemaIAFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new SchemaIAFactoring(opts);
    }

    return factoring;
}

static Plugin<Factoring> _plugin_lp("schema_ia", _parse_schema_ia_factoring);

}

