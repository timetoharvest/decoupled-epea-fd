#include "lp_factorings.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../factoring.h"
#include "../lp/mip_solver.h"
#include "../task_utils/causal_graph.h"
#include "../globals.h"
#include "../operator.h"

#include <iostream>


using namespace std;


LPFactoring::LPFactoring(const Options &opts) : Factoring(opts) {
     profile = STAR;
     strategy = STRATEGY(opts.get_enum("strategy"));
     factoring_type = FACTORING_TYPE(opts.get_enum("factoring_type"));
     cout << "using LP factoring strategy" << endl;
}

Factoring::FactoredVars LPFactoring::get_factoring() {

    FactoredVars factoring;

    switch (strategy) {
      case MIS: factoring = get_mip_factoring(false); break;
      case MIS_MOBILE: factoring = get_mip_factoring(true); break;
      case MAX_MOBILE_LEAVES: factoring = get_max_mobile_leaves(); break;
      default: exit_with(utils::ExitCode::CRITICAL_ERROR);
    }

    return factoring;

}

Factoring::FactoredVars LPFactoring::get_mip_factoring(bool mobile_leaves) {

    mip::MIPSolver mip_solver (mip::MIPSolverType::CPLEX);
    vector<mip::MIPVariable> variables;
    vector<mip::MIPConstraint> constraints;

    if (mobile_leaves) {
        vector<bool> mobile_var = get_mobile_vars();
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            if (mobile_var[var]){
                variables.push_back(mip::MIPVariable(0.0,1.0,1.0,mip::MIPVariableType::INT));
            } else {
                variables.push_back(mip::MIPVariable(0.0,0.0,1.0,mip::MIPVariableType::INT));
            }
            for (int suc : causal_graph::get_causal_graph().get_successors(var)) {
                mip::MIPConstraint constraint (0.0,1.0);
                constraint.insert(var,1.0);
                constraint.insert(suc,1.0);
                constraints.push_back(constraint);
            }
        }
    } else {
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            variables.push_back(mip::MIPVariable(0.0,1.0,1.0,mip::MIPVariableType::INT));
            for (int suc : causal_graph::get_causal_graph().get_successors(var)) {
                mip::MIPConstraint constraint (0.0,1.0);
                constraint.insert(var,1.0);
                constraint.insert(suc,1.0);
                constraints.push_back(constraint);
            }
        }
    }

    mip_solver.load_problem(mip::MIPObjectiveSense::MAXIMIZE,variables,constraints);
    mip_solver.solve();

    //extract solution
    FactoredVars factoring;
    vector<double> solution = mip_solver.extract_solution();
    for (size_t var = 0; var < solution.size(); var++) {
        if (solution.at(var) == 1.0) {
            set<int> leaf;
            leaf.insert(var);
            factoring.push_back(leaf);
        }
    }

    return factoring;

}


Factoring::FactoredVars LPFactoring::get_max_mobile_leaves() {

    mip::MIPSolver mip_solver (mip::MIPSolverType::CPLEX);
    vector<mip::MIPVariable> variables;
    vector<mip::MIPConstraint> constraints;

    int max_number_leaves = get_mip_factoring(false).size();


    //x_ij indicating whether variable v_j is contained in leaf F_i
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            variables.push_back(mip::MIPVariable(0.0,1.0,0.0,mip::MIPVariableType::INT));
        }
    }
    //y_ik indicating whether action a_k is a leaf only action and affects leaf F_i
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t op = 0; op < g_operators.size(); op++) {
            variables.push_back(mip::MIPVariable(0.0,1.0,0.0,mip::MIPVariableType::INT));
        }
    }
    //z_i indicating whether leaf F_i contains at least a single variable
    for (int i = 0; i < max_number_leaves; i++) {
        variables.push_back(mip::MIPVariable(0.0,1.0,1.0,mip::MIPVariableType::INT));
    }


    //(1) every variable is contained in at most one leaf
    for (size_t var = 0; var < g_variable_domain.size(); var++) {
        mip::MIPConstraint constraint (0.0,1.0);
        for (int i = 0; i < max_number_leaves; i++) {
            constraint.insert(i * g_variable_domain.size() + var, 1.0);
        }
        constraints.push_back(constraint);
    }

    //(2) if y_ik = 1 then all variables that are affected by a_k are contained in leaf F_i
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t op = 0; op < g_operators.size(); op++) {
            const vector<Effect> &effects = g_operators[op].get_effects();
            mip::MIPConstraint constraint (0.0,effects.size());
            for (const Effect &eff : effects) {
                constraint.insert(i * g_variable_domain.size() + eff.var, 1.0);
            }
            constraint.insert(max_number_leaves * g_variable_domain.size() + i * g_operators.size() + op, (-1) * static_cast<int>(effects.size()));
            constraints.push_back(constraint);
        }
    }

    //(3) if all variables that are affected by a_k are contained in leaf F_i then y_ik = 1
    for (int i = 0; i < max_number_leaves; i++) {
        for (size_t op = 0; op < g_operators.size(); op++) {
            const vector<Effect> &effects = g_operators[op].get_effects();
            mip::MIPConstraint constraint (0.0,effects.size()-1);
            for (const Effect &eff : effects) {
                constraint.insert(i * g_variable_domain.size() + eff.var, 1.0);
            }
            constraint.insert(max_number_leaves * g_variable_domain.size() + i * g_operators.size() + op, -1.0);
            constraints.push_back(constraint);
        }
    }

    if (factoring_type == STRICT_STAR) {
        //(4) (5) if a_k affects v_j and v_j is contained in F^L_i then a_k has only preconditions / effects on F^L_i or the center
        for (int i_1 = 0; i_1 < max_number_leaves; i_1++) {
            for (size_t op = 0; op < g_operators.size(); op++) {
                const vector<Effect> &effects = g_operators[op].get_effects();
                const vector<Condition> &preconditions = g_operators[op].get_preconditions();
                for (const Effect &eff_1 : effects) {
                    vector<double> coefficients (max_number_leaves * g_variable_domain.size(),0.0);
                    for (const Effect &eff_2 : effects) {
                        coefficients[i_1 * g_variable_domain.size() + eff_2.var] +=  -1.0;
                        for (int i_2 = 0; i_2 < max_number_leaves; i_2++) {
                            coefficients[i_2 * g_variable_domain.size() + eff_2.var] += 1.0;
                        }
                    }
                    for (const Condition &pre : preconditions) {
                        coefficients[i_1 * g_variable_domain.size() + pre.var] += -1.0;
                        for (int i_2 = 0; i_2 < max_number_leaves; i_2++) {
                            coefficients[i_2 * g_variable_domain.size() + pre.var] += 1.0;
                        }
                    }
                    coefficients[i_1 * g_variable_domain.size() + eff_1.var] += effects.size() + preconditions.size();
                    mip::MIPConstraint constraint (-1.0 * static_cast<int>(effects.size() + preconditions.size()), effects.size() + preconditions.size());
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

    if (factoring_type == GENERAL_STAR) {
        //(4) if action a_k does not affect center it only affects a single leaf
        for (size_t op = 0; op < g_operators.size(); op++) {
            const vector<Effect> &effects = g_operators[op].get_effects();
            mip::MIPConstraint constraint (0.0,effects.size()-1);
            for (int i = 0; i < max_number_leaves; i++) {
                for (const Effect &eff : effects) {
                    constraint.insert(i * g_variable_domain.size() + eff.var, 1.0);
                }
                constraint.insert(max_number_leaves * g_variable_domain.size() + i * g_operators.size() + op, -1.0);
            }
            constraints.push_back(constraint);
        }

        //(5) if action a_k only affects leaf F_i it has only preconditions on F_i or the center
        for (int i_1 = 0; i_1 < max_number_leaves; i_1++) {
            for (size_t op = 0; op < g_operators.size(); op++) {
                const vector<Condition> &preconditions = g_operators[op].get_preconditions();
                mip::MIPConstraint constraint (0.0,preconditions.size());
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


    //(6) z_i = 1 iff F_i contains at least a single variable
    for (int i = 0; i < max_number_leaves; i++) {
        mip::MIPConstraint constraint (0.0, g_variable_domain.size()-1);
        for (size_t var = 0; var < g_variable_domain.size(); var++) {
            constraint.insert(i * g_variable_domain.size() + var, -1.0);
        }
        constraint.insert(max_number_leaves * g_variable_domain.size() + max_number_leaves * g_operators.size() + i, g_variable_domain.size());
        constraints.push_back(constraint);
    }


    //(7) if z_i = 1 leaf F^L_i is mobile, i.e. their exists an leaf-only action that affects F^L_i
    for (int i = 0; i < max_number_leaves; i++) {
        mip::MIPConstraint constraint (0.0, g_operators.size());
        for (size_t op = 0; op < g_operators.size(); op++) {
            constraint.insert(max_number_leaves * g_variable_domain.size() + i * g_operators.size() + op, 1.0);
        }
        constraint.insert(max_number_leaves * g_variable_domain.size() + max_number_leaves * g_operators.size() + i, -1.0);
        constraints.push_back(constraint);
    }


    //solve problem
    mip_solver.load_problem(mip::MIPObjectiveSense::MAXIMIZE,variables,constraints);
    mip_solver.solve();

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

vector<ActionSchema> LPFactoring::get_action_schemes() {
    vector<ActionSchema> schemes;
    vector<int> pre_var;
    vector<int> eff_var;
    bool duplicate = false;
    for (const Operator &op : g_operators) {
        const std::vector<Condition> &preconditions = op.get_preconditions();
        const std::vector<Effect> &effects = op.get_effects();
        for (vector<ActionSchema>::reverse_iterator i = schemes.rbegin(); i != schemes.rend(); i++) {
            duplicate = true;
            pre_var = i->pre_var;
            eff_var = i->eff_var;
            if (pre_var.size() == preconditions.size() && eff_var.size() == effects.size()) {
                for (size_t p = 0; p < preconditions.size(); p++) {
                    if (pre_var[p] != preconditions[p].var) {
                        duplicate = false;
                        break;
                    }
                }
                for (size_t e = 0; e < effects.size(); e++) {
                    if (eff_var[e] != effects[e].var) {
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
            pre_var.clear();
            eff_var.clear();
            for (const Condition &pre : preconditions) {
                pre_var.push_back(pre.var);
            }
            for (const Effect &eff : effects) {
                eff_var.push_back(eff.var);
            }
            ActionSchema schema (1, pre_var, eff_var);
            schemes.push_back(schema);
        }
    }
    return schemes;
}



void LPFactoring::add_options_to_parser(OptionParser &parser) {
    parser.add_option<bool>(
            "some_option_name",
            "TODO",
            "false"
    );

    vector<string> strategies;
    strategies.push_back("mis");
    strategies.push_back("mis_mobile");
    strategies.push_back("max_mobile_leaves");
    parser.add_enum_option(
        "strategy",
        strategies,
        "TODO",
        "mis",
        vector<string>(),
        OptionFlags(false));

    vector<string> factoring_type;
    factoring_type.push_back("strict_star");
    factoring_type.push_back("general_star");
    parser.add_enum_option(
        "factoring_type",
        factoring_type,
        "TODO",
        "general_star",
        vector<string>(),
        OptionFlags(false));
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
