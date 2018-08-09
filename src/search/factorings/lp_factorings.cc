#include "lp_factorings.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../factoring.h"
#include "../lp/lp_solver.h"
#include "../lp/cplex_solver.h"
#include "../task_utils/causal_graph.h"
#include "../globals.h"
#include "../operator.h"

#include <iostream>


using namespace std;

using namespace lp;


LPFactoring::LPFactoring(const Options &opts) : Factoring(opts) {
     profile = STAR;
     strategy = STRATEGY(opts.get_enum("strategy"));
     factoring_type = FACTORING_TYPE(opts.get_enum("factoring_type"));
     min_mobility = opts.get<int>("min_mob");
     min_flexibility = opts.get<float>("min_flex");
     max_merge_steps = opts.get<int>("max_merge");
     cout << "using LP factoring strategy" << endl;
}


Factoring::FactoredVars LPFactoring::get_factoring() {

    FactoredVars factoring;

    switch (strategy) {
      case MIS: factoring = get_mip_factoring(); break;
      case MIS_MOBILE: factoring = get_mip_factoring(); break;
      case MIS_LP_RELAX: factoring = get_mip_factoring(); break;
      case MML: factoring = get_max_mobile_leaves(); break;
      case MML_MODULAR: factoring = get_max_mobile_leaves_modular(); break;
      case MML_MODULAR_NO_SCHEMES: factoring = get_max_mobile_leaves_modular_no_schemes(); break;
      case MM: factoring = get_max_mobile_leaves(); break;
      case MF: factoring = get_max_mobile_leaves(); break;
      case MMF: factoring = get_max_mobile_leaves(); break;
      default: exit_with(utils::ExitCode::CRITICAL_ERROR);
    }

    return factoring;

}

Factoring::FactoredVars LPFactoring::get_mip_factoring() {

    CPLEXSolver mip_solver;
    vector<MIPVariable> variables;
    vector<LPConstraint> constraints;

    if (strategy == MIS_MOBILE) {
        vector<bool> mobile_var = get_mobile_vars();
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            if (mobile_var[var]){
                variables.push_back(MIPVariable(0.0,1.0,1.0,MIPVariableType::INT));
            } else {
                variables.push_back(MIPVariable(0.0,0.0,1.0,MIPVariableType::INT));
            }
            for (int suc : causal_graph::get_causal_graph().get_successors(var)) {
                LPConstraint constraint (0.0,1.0);
                constraint.insert(var,1.0);
                constraint.insert(suc,1.0);
                constraints.push_back(constraint);
            }
        }
    } else {
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            variables.push_back(MIPVariable(0.0,1.0,1.0,MIPVariableType::INT));
            for (int suc : causal_graph::get_causal_graph().get_successors(var)) {
                LPConstraint constraint (0.0,1.0);
                constraint.insert(var,1.0);
                constraint.insert(suc,1.0);
                constraints.push_back(constraint);
            }
        }
    }

    mip_solver.load_problem(LPObjectiveSense::MAXIMIZE,variables,constraints);
    if (strategy == MIS_LP_RELAX) {
        mip_solver.solveLPRelaxation();
    } else {
        mip_solver.solve();
    }


    //extract solution
    FactoredVars factoring;
    vector<double> solution = mip_solver.extract_solution();
    for (size_t var = 0; var < solution.size(); var++) {
        if (solution[var] > 0.5) {
            set<int> leaf;
            leaf.insert(var);
            factoring.push_back(leaf);
        }
    }
    return factoring;
}


Factoring::FactoredVars LPFactoring::get_max_mobile_leaves() {

    CPLEXSolver mip_solver;
    vector<MIPVariable> variables;
    vector<LPConstraint> constraints;

    set_action_schemes();
    set_potential_leaves(min_mobility, min_flexibility);
    if (max_merge_steps > 0) {
        merge_potential_leaves(max_merge_steps);
    }
    size_t num_p_leaves = potential_leaves.size();


    vector<bool> p_leaves_cg (num_p_leaves * num_p_leaves, false);

    for (PotentialLeaf &p_leaf : potential_leaves) {
        for (int var : p_leaf.vars) {
            for (int suc : causal_graph::get_causal_graph().get_successors(var)) {
                for (int i : var_to_p_leaves[suc]) {
                    if (p_leaf.vars != potential_leaves[i].vars) {
                        p_leaves_cg[p_leaf.id * num_p_leaves + potential_leaves[i].id] = true;
                    }
                }
            }
        }
    }

    for (size_t p_leaf = 0; p_leaf < num_p_leaves; p_leaf++) {
      switch (strategy) {
        case MML:
            variables.push_back(MIPVariable(0.0,1.0,1.0, MIPVariableType::INT));
            break;
        case MF:
            variables.push_back(MIPVariable(0.0,1.0,potential_leaves[p_leaf].get_flexibility(), MIPVariableType::INT));
            break;
        case MM:
            variables.push_back(MIPVariable(0.0,1.0,potential_leaves[p_leaf].num_leaf_only_actions, MIPVariableType::INT));
            break;
        case MMF:
            variables.push_back(MIPVariable(0.0,1.0,potential_leaves[p_leaf].num_leaf_only_actions * potential_leaves[p_leaf].get_flexibility(), MIPVariableType::INT));
            break;
        default: exit_with(utils::ExitCode::CRITICAL_ERROR);
      }
    }

    for (size_t p_leaf_1 = 0; p_leaf_1 < num_p_leaves; p_leaf_1++) {
        for (size_t p_leaf_2 = 0; p_leaf_2 < num_p_leaves; p_leaf_2++) {
            if (p_leaves_cg[p_leaf_1 * num_p_leaves + p_leaf_2]) {
              LPConstraint constraint (0.0,1.0);
              constraint.insert(p_leaf_1,1.0);
              constraint.insert(p_leaf_2,1.0);
              constraints.push_back(constraint);
            }
        }
    }

    if (variables.size() > 2000 && constraints.size() > 4500000) {
        exit_error("Number of LP variables and number of LP constraints exceed limit.", utils::ExitCode::OUT_OF_MEMORY);
    }

    mip_solver.load_problem(LPObjectiveSense::MAXIMIZE,variables,constraints);
    mip_solver.print_statistics();
    mip_solver.solve();

    FactoredVars factoring;

    vector<double> solution = mip_solver.extract_solution();
    for (size_t p_leaf = 0; p_leaf < num_p_leaves; p_leaf++) {
        if (solution[p_leaf] == 1.0) {
            set<int> leaf;
            for (int var : potential_leaves[p_leaf].vars) {
                leaf.insert(var);
            }
            factoring.push_back(leaf);
        }
    }

    return factoring;

}


Factoring::FactoredVars LPFactoring::get_max_mobile_leaves_modular() {

    CPLEXSolver mip_solver;
    vector<MIPVariable> variables;
    vector<LPConstraint> constraints;

    FactoredVars mip_factoring = get_mip_factoring();

    vector<bool> mobile_vars = get_mobile_vars();
    bool mobile_factoring = true;
    for (set<int> &factor : mip_factoring) {
        for (int var : factor) {
            mobile_factoring = mobile_factoring && mobile_vars[var];
        }
    }
    if (mobile_factoring) {
        return mip_factoring;
    }

    int max_number_leaves = mip_factoring.size();

    set_action_schemes();
    vector<ActionSchema> schemes = act_schemes;
    size_t num_schemes = schemes.size();


    //x_ij indicating whether variable v_j is contained in leaf F_i
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            variables.push_back(MIPVariable(0.0,1.0,0.0,MIPVariableType::INT));
        }
    }
    //y_ik indicating whether action schema a_k affects only leaf F_i
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t schema = 0; schema < num_schemes; schema++) {
            variables.push_back(MIPVariable(0.0,1.0,0.0,MIPVariableType::INT));
        }
    }
    //z_i indicating whether leaf F_i contains at least a single variable
    for (int i = 0; i < max_number_leaves; i++) {
        variables.push_back(MIPVariable(0.0,1.0,1.0,MIPVariableType::INT));
    }


    //(1) every variable is contained in at most one leaf
    for (size_t var = 0; var < g_variable_domain.size(); var++) {
        LPConstraint constraint (0.0,1.0);
        for (int i = 0; i < max_number_leaves; i++) {
            constraint.insert(i * g_variable_domain.size() + var, 1.0);
        }
        constraints.push_back(constraint);
    }

    //(2) y_ik = 1 iff all variables that are affected by schema a_k are contained in leaf F_i
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t schema = 0; schema < num_schemes; schema++) {
            const vector<int> &eff_vars = schemes[schema].eff_vars;
            LPConstraint constraint (0.0,eff_vars.size() - 1.0);
            for (const int &eff_var : eff_vars) {
                constraint.insert(i * g_variable_domain.size() + eff_var, 1.0);
            }
            constraint.insert(max_number_leaves * g_variable_domain.size() + i * num_schemes + schema, (-1) * static_cast<int>(eff_vars.size()));
            constraints.push_back(constraint);
        }
    }

    if (factoring_type == STRICT || factoring_type == F || factoring_type == IF) {
        //(3) if a_k affects v_j and v_j is contained in F^L_i then a_k has only preconditions / effects on F^L_i or the center
        for (int i_1 = 0; i_1 < max_number_leaves; i_1++) {
            for (size_t schema = 0; schema < num_schemes; schema++) {
                const vector<int> &eff_vars = schemes[schema].eff_vars;
                const vector<int> &pre_vars = schemes[schema].pre_vars;
                for (const int &eff_var_1 : eff_vars) {
                    vector<double> coefficients (max_number_leaves * g_variable_domain.size(),0.0);
                    for (const int &eff_var_2 : eff_vars) {
                        coefficients[i_1 * g_variable_domain.size() + eff_var_2] +=  -1.0;
                        if (factoring_type == STRICT) {
                            for (int i_2 = 0; i_2 < max_number_leaves; i_2++) {
                                coefficients[i_2 * g_variable_domain.size() + eff_var_2] += 1.0;
                            }
                        }
                    }
                    for (const int &pre_var : pre_vars) {
                        coefficients[i_1 * g_variable_domain.size() + pre_var] += -1.0;
                        if (factoring_type == STRICT || factoring_type == F) {
                            for (int i_2 = 0; i_2 < max_number_leaves; i_2++) {
                              coefficients[i_2 * g_variable_domain.size() + pre_var] += 1.0;
                            }
                        }
                    }
                    coefficients[i_1 * g_variable_domain.size() + eff_var_1] += eff_vars.size() + pre_vars.size();
                    LPConstraint constraint (-1.0 * static_cast<int>(eff_vars.size() + pre_vars.size()), 0);
                    if (factoring_type == STRICT) {
                        constraint.set_upper_bound(eff_vars.size() + pre_vars.size());
                    }
                    if (factoring_type == F) {
                        constraint.set_upper_bound(pre_vars.size());
                    }
                    for (size_t x = 0; x < max_number_leaves * g_variable_domain.size(); x++) {
                        if (coefficients[x] != 0.0) {
                            constraint.insert(x,coefficients[x]);
                        }
                    }
                    constraints.push_back(constraint);
                }
            }
        }
    }

    if (factoring_type == GENERAL) {
        //(3.1) if action a_k does not affect center it only affects a single leaf
        for (size_t schema = 0; schema < num_schemes; schema++) {
            const vector<int> &eff_vars = schemes[schema].eff_vars;
            LPConstraint constraint (0.0,eff_vars.size()-1);
            for (int i = 0; i < max_number_leaves; i++) {
                for (const int &eff_var : eff_vars) {
                    constraint.insert(i * g_variable_domain.size() + eff_var, 1.0);
                }
                constraint.insert(max_number_leaves * g_variable_domain.size() + i * num_schemes + schema, -1.0);
            }
            constraints.push_back(constraint);
        }

        //(3.2) if action a_k only affects leaf F_i it has only preconditions on F_i or the center
        for (int i_1 = 0; i_1 < max_number_leaves; i_1++) {
            for (size_t schema = 0; schema < num_schemes; schema++) {
                const vector<int> &pre_vars = schemes[schema].pre_vars;
                LPConstraint constraint (0.0,pre_vars.size());
                for (int i_2 = 0; i_2 < max_number_leaves; i_2++) {
                    if (i_1 == i_2) {
                        continue;
                    }
                    for (const int &pre_var : pre_vars) {
                        constraint.insert(i_2 * g_variable_domain.size() + pre_var, 1.0);
                    }
                }
                constraint.insert(max_number_leaves * g_variable_domain.size() + i_1 * num_schemes + schema, pre_vars.size());
                constraints.push_back(constraint);
            }
        }
    }


    //(4) z_i = 1 iff F_i contains at least a single variable
    for (int i = 0; i < max_number_leaves; i++) {
        LPConstraint constraint (0.0, g_variable_domain.size()-1);
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            constraint.insert(i * g_variable_domain.size() + var, -1.0);
        }
        constraint.insert(max_number_leaves * g_variable_domain.size() + max_number_leaves * num_schemes + i, g_variable_domain.size());
        constraints.push_back(constraint);
    }


    //(5) if z_i = 1 leaf F^L_i is mobile, i.e. their exists an leaf-only action that affects F^L_i
    for (int i = 0; i < max_number_leaves; i++) {
        LPConstraint constraint (0.0, num_schemes);
        for (size_t schema = 0; schema < num_schemes; schema++) {
            constraint.insert(max_number_leaves * g_variable_domain.size() + i * num_schemes + schema, 1.0);
        }
        constraint.insert(max_number_leaves * g_variable_domain.size() + max_number_leaves * num_schemes + i, -1.0);
        constraints.push_back(constraint);
    }


    //solve problem
    mip_solver.load_problem(LPObjectiveSense::MAXIMIZE,variables,constraints);
    mip_solver.solve();
    mip_solver.print_statistics();

    //extract solution
    FactoredVars factoring;
    vector<double> solution = mip_solver.extract_solution();

    for (int i = 0; i < max_number_leaves; i++) {
        set<int> leaf;
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            if (solution.at(i * g_variable_domain.size() + var) == 1.0) {
                leaf.insert(var);
            }
        }
        if (leaf.empty()){
          continue;
        }
        factoring.push_back(leaf);
    }

    return factoring;

}



Factoring::FactoredVars LPFactoring::get_max_mobile_leaves_modular_no_schemes() {

    CPLEXSolver mip_solver;
    vector<MIPVariable> variables;
    vector<LPConstraint> constraints;

    int max_number_leaves = get_mip_factoring().size();


    //x_ij indicating whether variable v_j is contained in leaf F_i
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            variables.push_back(MIPVariable(0.0,1.0,0.0,MIPVariableType::INT));
        }
    }
    //y_ik indicating whether action a_k is a leaf only action and affects leaf F_i
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t op = 0; op < g_operators.size(); op++) {
            variables.push_back(MIPVariable(0.0,1.0,0.0,MIPVariableType::INT));
        }
    }
    //z_i indicating whether leaf F_i contains at least a single variable
    for (int i = 0; i < max_number_leaves; i++) {
        variables.push_back(MIPVariable(0.0,1.0,1.0,MIPVariableType::INT));
    }


    //(1) every variable is contained in at most one leaf
    for (size_t var = 0; var < g_variable_domain.size(); var++) {
        LPConstraint constraint (0.0,1.0);
        for (int i = 0; i < max_number_leaves; i++) {
            constraint.insert(i * g_variable_domain.size() + var, 1.0);
        }
        constraints.push_back(constraint);
    }

    //(2) y_ik = 1 iff all variables that are affected by a_k are contained in leaf F_i
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t op = 0; op < g_operators.size(); op++) {
            const vector<Effect> &effects = g_operators[op].get_effects();
            LPConstraint constraint (0.0,effects.size() - 1.0);
            for (const Effect &eff : effects) {
                constraint.insert(i * g_variable_domain.size() + eff.var, 1.0);
            }
            constraint.insert(max_number_leaves * g_variable_domain.size() + i * g_operators.size() + op, (-1) * static_cast<int>(effects.size()));
            constraints.push_back(constraint);
        }
    }


    if (factoring_type == STRICT || factoring_type == F || factoring_type == IF) {
        //(3) if a_k affects v_j and v_j is contained in F^L_i then a_k has only preconditions / effects on F^L_i or the center
        for (int i_1 = 0; i_1 < max_number_leaves; i_1++) {
            for (size_t op = 0; op < g_operators.size(); op++) {
                const vector<Effect> &effects = g_operators[op].get_effects();
                const vector<Condition> &preconditions = g_operators[op].get_preconditions();
                for (const Effect &eff_1 : effects) {
                    vector<double> coefficients (max_number_leaves * g_variable_domain.size(),0.0);
                    for (const Effect &eff_2 : effects) {
                        coefficients[i_1 * g_variable_domain.size() + eff_2.var] +=  -1.0;
                        if (factoring_type == STRICT) {
                            for (int i_2 = 0; i_2 < max_number_leaves; i_2++) {
                                coefficients[i_2 * g_variable_domain.size() + eff_2.var] += 1.0;
                            }
                        }
                    }
                    for (const Condition &pre : preconditions) {
                        coefficients[i_1 * g_variable_domain.size() + pre.var] += -1.0;
                        if (factoring_type == STRICT || factoring_type == F) {
                            for (int i_2 = 0; i_2 < max_number_leaves; i_2++) {
                              coefficients[i_2 * g_variable_domain.size() + pre.var] += 1.0;
                            }
                        }
                    }
                    coefficients[i_1 * g_variable_domain.size() + eff_1.var] += effects.size() + preconditions.size();
                    LPConstraint constraint (-1.0 * static_cast<int>(effects.size() + preconditions.size()), 0);
                    if (factoring_type == STRICT) {
                        constraint.set_upper_bound(effects.size() + preconditions.size());
                    }
                    if (factoring_type == F) {
                        constraint.set_upper_bound(preconditions.size());
                    }
                    for (size_t x = 0; x < max_number_leaves * g_variable_domain.size(); x++) {
                        if (coefficients[x] != 0.0) {
                            constraint.insert(x,coefficients[x]);
                        }
                    }
                    constraints.push_back(constraint);
                }
            }
        }
    }

    if (factoring_type == GENERAL) {
        //(3.1) if action a_k does not affect center it only affects a single leaf
        for (size_t op = 0; op < g_operators.size(); op++) {
            const vector<Effect> &effects = g_operators[op].get_effects();
            LPConstraint constraint (0.0,effects.size()-1);
            for (int i = 0; i < max_number_leaves; i++) {
                for (const Effect &eff : effects) {
                    constraint.insert(i * g_variable_domain.size() + eff.var, 1.0);
                }
                constraint.insert(max_number_leaves * g_variable_domain.size() + i * g_operators.size() + op, -1.0);
            }
            constraints.push_back(constraint);
        }

        //(3.2) if action a_k only affects leaf F_i it has only preconditions on F_i or the center
        for (int i_1 = 0; i_1 < max_number_leaves; i_1++) {
            for (size_t op = 0; op < g_operators.size(); op++) {
                const vector<Condition> &preconditions = g_operators[op].get_preconditions();
                LPConstraint constraint (0.0,preconditions.size());
                for (int i_2 = 0; i_2 < max_number_leaves; i_2++) {
                    if (i_1 == i_2) {
                        continue;
                    }
                    for (const Condition &pre : preconditions) {
                        constraint.insert(i_2 * g_variable_domain.size() + pre.var, 1.0);
                    }
                }
                constraint.insert(max_number_leaves * g_variable_domain.size() + i_1 * g_operators.size() + op, preconditions.size());
                constraints.push_back(constraint);
            }
        }
    }


    //(4) z_i = 1 iff F_i contains at least a single variable
    for (int i = 0; i < max_number_leaves; i++) {
        LPConstraint constraint (0.0, g_variable_domain.size()-1);
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            constraint.insert(i * g_variable_domain.size() + var, -1.0);
        }
        constraint.insert(max_number_leaves * g_variable_domain.size() + max_number_leaves * g_operators.size() + i, g_variable_domain.size());
        constraints.push_back(constraint);
    }


    //(5) if z_i = 1 leaf F^L_i is mobile, i.e. their exists an leaf-only action that affects F^L_i
    for (int i = 0; i < max_number_leaves; i++) {
        LPConstraint constraint (0.0, g_operators.size());
        for (size_t op = 0; op < g_operators.size(); op++) {
            constraint.insert(max_number_leaves * g_variable_domain.size() + i * g_operators.size() + op, 1.0);
        }
        constraint.insert(max_number_leaves * g_variable_domain.size() + max_number_leaves * g_operators.size() + i, -1.0);
        constraints.push_back(constraint);
    }


    //solve problem
    mip_solver.load_problem(LPObjectiveSense::MAXIMIZE,variables,constraints);
    mip_solver.solve();

    mip_solver.print_statistics();

    //extract solution
    FactoredVars factoring;
    vector<double> solution = mip_solver.extract_solution();


    for (int i = 0; i < max_number_leaves; i++) {
        set<int> leaf;
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            if (solution.at(i * g_variable_domain.size() + var) == 1.0) {
                leaf.insert(var);
            }
        }
        if (leaf.empty()){
          continue;
        }
        factoring.push_back(leaf);
    }

    return factoring;

}


//returns a vector that indicates for each variable v whether a leaf factor consisting of v is mobile
vector<bool> LPFactoring::get_mobile_vars() {

    vector<bool> mobile_var (g_variable_domain.size(), false);

    for (const Operator &op : g_operators){
        const vector<Effect> effects = op.get_effects();
        if (effects.size() == 1) {
            mobile_var[effects[0].var] = true;
        }
    }

    return mobile_var;

}


void LPFactoring::set_action_schemes(int min_num_actions) {
    vector<int> pre_vars;
    vector<int> eff_vars;
    bool duplicate = false;
    for (const Operator &op : g_operators) {
        const std::vector<Condition> &preconditions = op.get_preconditions();
        const std::vector<Effect> &effects = op.get_effects();
        for (vector<ActionSchema>::reverse_iterator i = act_schemes.rbegin(); i != act_schemes.rend(); i++) {
            duplicate = true;
            pre_vars = i->pre_vars;
            eff_vars = i->eff_vars;
            if (pre_vars.size() == preconditions.size() && eff_vars.size() == effects.size()) {
                for (size_t p = 0; p < preconditions.size(); p++) {
                    if (pre_vars[p] != preconditions[p].var) {
                        duplicate = false;
                        break;
                    }
                }
                for (size_t e = 0; e < effects.size(); e++) {
                    if (eff_vars[e] != effects[e].var) {
                        duplicate = false;
                        break;
                    }
                }
            } else {
                duplicate = false;
            }
            if (duplicate) {
                i->incr_num_action();
                break;
            }
        }
        if (!duplicate) {
            pre_vars.clear();
            eff_vars.clear();
            for (const Condition &pre : preconditions) {
                pre_vars.push_back(pre.var);
            }
            for (const Effect &eff : effects) {
                eff_vars.push_back(eff.var);
            }
            ActionSchema schema (1, pre_vars, eff_vars);
            act_schemes.push_back(schema);
        }
    }
    if (min_num_actions > 1) {
        //erase schemes with num_actions < min_num_actions
        act_schemes.erase(remove_if(act_schemes.begin(), act_schemes.end(), [&min_num_actions](ActionSchema schema){return schema.num_actions < min_num_actions;}), act_schemes.end());
    }
}


void LPFactoring::set_potential_leaves(int min_mobility, float min_flexibility, int min_num_actions) {

    int id = 0;
    for (ActionSchema action_schema : act_schemes) {
        bool duplicate = false;
        for (vector<PotentialLeaf>::reverse_iterator i = potential_leaves.rbegin(); i != potential_leaves.rend(); i++) {
            if (action_schema.eff_vars == i->vars) {
                i->num_actions += action_schema.num_actions;
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            PotentialLeaf p_leaf (id, action_schema.num_actions, action_schema.eff_vars);
            potential_leaves.push_back(p_leaf);
            id++;
        }
    }

    var_to_p_leaves.resize(g_variable_domain.size(), set<int>());

    for (PotentialLeaf &p_leaf : potential_leaves) {
        for (int var : p_leaf.vars) {
            var_to_p_leaves[var].insert(p_leaf.id);
        }
    }

    //set number of leaf actions
    for (PotentialLeaf &p_leaf : potential_leaves) {
        set<int> overlapping_schemes;
        for (int var : p_leaf.vars) {
            for (int index : var_to_p_leaves[var]) {
                overlapping_schemes.insert(index);
            }
        }
        int num_leaf_actions = 0;
        for (int index : overlapping_schemes) {
            num_leaf_actions += potential_leaves[index].num_actions;
        }
        p_leaf.num_leaf_actions = num_leaf_actions;
    }

    //set number of leaf-only actions
    for (PotentialLeaf &p_leaf : potential_leaves) {
        std::set<int> superset_schemes;
        if (p_leaf.vars.size() == 1) {
            int var = (p_leaf.vars)[0];
            for (int index : var_to_p_leaves[var]) {
                superset_schemes.insert(index);
            }
        } else {
            int var = p_leaf.vars[0];
            for (int index : var_to_p_leaves[var]) {
                bool superset_schema = true;
                for (int var_1 : p_leaf.vars) {
                    bool var_contained = false;
                    for (int var_2 : potential_leaves[index].vars) {
                        if (var_1 == var_2) {
                            var_contained = true;
                            break;
                        }
                    }
                    if (!var_contained) {
                        superset_schema = false;
                        break;
                    }
                }
                if (superset_schema) {
                    superset_schemes.insert(index);
                }
            }
        }
        for (int index : superset_schemes) {
            potential_leaves[index].num_leaf_only_actions += p_leaf.num_actions;
        }
    }

    if (min_mobility > 1 || min_flexibility > 0 || min_num_actions > 1) {
        //erase effect schemes that does not satisfy requirements
        auto lambda_expr = [&](PotentialLeaf p_leaf){return p_leaf.num_actions < min_num_actions || p_leaf.num_leaf_only_actions < min_mobility || p_leaf.get_flexibility() < min_flexibility;};
        potential_leaves.erase(remove_if(potential_leaves.begin(), potential_leaves.end(), lambda_expr), potential_leaves.end());
        //adjust IDs
        id = 0;
        for (PotentialLeaf &p_leaf : potential_leaves) {
            p_leaf.id = id;
            id++;
        }
        for (size_t var = 0; var < g_variable_domain.size(); var++){
            var_to_p_leaves[var].clear();
        }
        for (PotentialLeaf &p_leaf : potential_leaves) {
            for (int var : p_leaf.vars) {
                var_to_p_leaves[var].insert(p_leaf.id);
            }
        }
    }
}


void LPFactoring::merge_potential_leaves(int max_iterations) {

    queue<PotentialLeaf> open;
    for (PotentialLeaf &p_leaf : potential_leaves) {
        sort(p_leaf.vars.begin(), p_leaf.vars.end());
        if (p_leaf.vars.size() > 1) {
            open.push(p_leaf);
        }
    }

    int id = potential_leaves.size();
    vector<PotentialLeaf> merged_leaves;
    for (int iteration = 0; iteration < max_iterations; iteration++) {
        while (!open.empty()) {
            PotentialLeaf p_leaf = open.front();
            open.pop();
            set<int> overlapping_leaves;
            for (int var : p_leaf.vars) {
                for (int index : var_to_p_leaves[var]) {
                    overlapping_leaves.insert(index);
                }
            }

            for (int i : overlapping_leaves) {
                vector<int> vars = potential_leaves[i].vars;
                if (vars.size() == 1 || includes(p_leaf.vars.begin(), p_leaf.vars.end(), vars.begin(), vars.end()) || includes(vars.begin(), vars.end(), p_leaf.vars.begin(), p_leaf.vars.end())) {
                    continue;
                }
                vector<int> merged_vars (p_leaf.vars.size() + vars.size());
                auto it = set_union(p_leaf.vars.begin(), p_leaf.vars.end(), vars.begin(), vars.end(), merged_vars.begin());
                merged_vars.resize(it - merged_vars.begin());

                //duplicate check
                bool dublicate = false;
                for (int j : var_to_p_leaves[merged_vars[0]]) {
                    if (potential_leaves[j].vars == merged_vars) {
                       dublicate = true;
                       break;
                    }
                }
                if (!dublicate) {
                    for (PotentialLeaf &merged_leaf : merged_leaves) {
                        if (merged_leaf.vars == merged_vars) {
                           dublicate = true;
                           break;
                        }
                    }
                }
                if (dublicate) {
                    continue;
                }

                PotentialLeaf merged_leaf (id, 0, merged_vars);

                //calculate number of leaf actions and leaf-only actions
                set<int> overlapping_with_merged_leaf;
                for (int var : merged_vars) {
                    for (int j : var_to_p_leaves[var]) {
                        overlapping_with_merged_leaf.insert(j);
                    }
                }
                for (int j : overlapping_with_merged_leaf) {
                    merged_leaf.num_leaf_actions += potential_leaves[j].num_actions;
                    if (includes(merged_vars.begin(), merged_vars.end(), potential_leaves[j].vars.begin(), potential_leaves[j].vars.end())) {
                        merged_leaf.num_leaf_only_actions += potential_leaves[j].num_actions;
                    }
                }
                if (p_leaf.get_flexibility() > merged_leaf.get_flexibility() || potential_leaves[i].get_flexibility() > merged_leaf.get_flexibility()) {
                    continue;
                }

                merged_leaves.push_back(merged_leaf);
                id++;
            }
        }
        for (PotentialLeaf &merged_leaf : merged_leaves) {
            potential_leaves.push_back(merged_leaf);
            open.push(merged_leaf);
            for (int var : merged_leaf.vars) {
                var_to_p_leaves[var].insert(merged_leaf.id);
            }
        }
        merged_leaves.clear();
    }
}


void LPFactoring::add_options_to_parser(OptionParser &parser) {

    vector<string> strategies;
    strategies.push_back("mis");
    strategies.push_back("mis_mobile");
    strategies.push_back("mis_lp_relax");
    strategies.push_back("mml");
    strategies.push_back("mml_modular");
    strategies.push_back("mml_modular_no_schemes");
    strategies.push_back("mm");
    strategies.push_back("mf");
    strategies.push_back("mmf");
    parser.add_enum_option(
        "strategy",
        strategies,
        "TODO",
        "mis",
        vector<string>(),
        OptionFlags(false));

    vector<string> factoring_type;
    factoring_type.push_back("general");
    factoring_type.push_back("strict");
    factoring_type.push_back("f");
    factoring_type.push_back("if");
    parser.add_enum_option(
        "factoring_type",
        factoring_type,
        "TODO",
        "strict_star",
        vector<string>(),
        OptionFlags(false));

    parser.add_option<int>(
        "min_mob",
        "TODO",
        "1"
    );

    parser.add_option<float>(
        "min_flex",
        "TODO",
        "0"
    );

    parser.add_option<int>(
        "max_merge",
        "TODO",
        "0"
    );

}

static Factoring *_parse_lp(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    LPFactoring::add_options_to_parser(parser);

    Options opts = parser.parse();

    LPFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new LPFactoring(opts);
    }

    return factoring;
}


static Plugin<Factoring> _plugin_lp("lp_factoring", _parse_lp);
