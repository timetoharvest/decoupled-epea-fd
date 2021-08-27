#include "lp_no_center_factoring.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../factoring.h"
#include "../lp/lp_solver.h"
#include "../lp/cplex_solver.h"
#include "../task_utils/causal_graph.h"
#include "../globals.h"
#include "../operator.h"
#include "../utils/timer.h"

#include <algorithm>
#include <iostream>


using namespace std;


static const string CPLEX_ERROR_MEMORY_1 = "CPLEX Error  1001: Out of memory.\n";
static const string CPLEX_ERROR_MEMORY_2 = "not enough memory";

namespace lp_no_center_factoring {
LPNoCenterFactoring::LPNoCenterFactoring(const Options &opts) : Factoring(opts),
        strategy(STRATEGY(opts.get_enum("strategy"))),
        factoring_type(FACTORING_TYPE(opts.get_enum("factoring_type"))),
        min_mobility(opts.get<int>("min_mobility")),
        min_flexibility(opts.get<float>("min_flexibility")),
        max_merge_steps(opts.get<int>("max_merge_steps")),
        cplex_time_limit(opts.get<int>("cplex_time_limit")),
        memory_limit(opts.get<int>("memory_limit")),
        lp_solver_type(lp::LPSolverType(opts.get_enum("lpsolver"))),
        timer(utils::Timer()) {
    profile = FACTORING_PROFILE::STAR;
    cout << "using LP factoring strategy" << endl;
}


Factoring::FactoredVars LPNoCenterFactoring::get_factoring() {

    FactoredVars factoring;
    timer.reset();

    switch (strategy) {
        case MML: factoring = get_max_mobile_leaves(); break;
        case MM: factoring = get_max_mobile_leaves(); break;
        case MF: factoring = get_max_mobile_leaves(); break;
        case MMF: factoring = get_max_mobile_leaves(); break;
        default: exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }

    return factoring;

}

vector<double> LPNoCenterFactoring::solve_lp(lp::LPObjectiveSense sense,
        vector<lp::LPVariable> &variables,
        vector<lp::LPConstraint> &constraints,
        bool fail_on_out_of_memory,
        bool solve_lp_relaxation,
        int time_limit) const {

    if (constraints.empty()){
        cout << "WARNING: no constraints specified. Adding dummy constraint to prevent errors." << endl;
        lp::LPConstraint dummy(0, 1);
        dummy.insert(0, 1);
        constraints.push_back(dummy);
    }

    if (solve_lp_relaxation && time_limit == -1){
        cout << "WARNING: time limit not supported when solving LP relaxation, limit will be ignored." << endl;
        time_limit = -1;
    }

#ifndef NDEBUG
    dump_lp(sense, variables, constraints);
#endif


#ifdef USE_CPLEX
    lp::CPLEXSolver solver(time_limit);
#else
    lp::LPSolver solver(lp_solver_type);
    if (time_limit != -1){
        cout << "WARNING: CPLEX not found, ignoring time limit." << endl;
        time_limit = -1;
    }
    fail_on_out_of_memory = !fail_on_out_of_memory; // HACK prevent unused variable error
#endif

    if (solve_lp_relaxation){
        for (auto &var : variables){
            var.is_integer = false;
        }
    }

#ifdef USE_CPLEX
    try {
#endif
        solver.load_problem(sense, variables, constraints);

        constraints = vector<lp::LPConstraint>(); // save memory TODO does this really make a difference?

        solver.print_statistics();

        cout << "LP construction time: " << timer << endl;

        timer.reset();

        if (time_limit == -1){
            solver.solve();
        } else {
#ifdef USE_CPLEX
            solver.populate();
#endif
        }
#ifdef USE_CPLEX
    } catch (const IloException &excpt) {
        string error_message = excpt.getMessage();
        if (!fail_on_out_of_memory && (error_message == CPLEX_ERROR_MEMORY_1 ||
                error_message == CPLEX_ERROR_MEMORY_2)) {
            // TODO refine the check
            cout << "Error while factoring planning task: CPLEX out of Memory" << endl;
            return vector<double>();
        } else {
            cout << "CPLEX error : " << error_message << endl;
            exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    }
#endif

    cout << "LP solving time: " << timer << endl;

    vector<double> solution;

    if (time_limit == -1) {
        if (!solver.has_optimal_solution()){
            cout << "no optimal solution to LP found, factoring failed" << endl;
            return solution;
        }
    } else {
#ifdef USE_CPLEX
        if (!solver.has_solution()){
            cout << "no solution to LP found, factoring failed" << endl;
            return solution;
        }
#endif
    }


    if (time_limit == -1){
        solution = solver.extract_solution();
    } else {
#ifdef USE_CPLEX
        solution = solver.extract_best_solution();
#endif
    }

    return solution;
}

void LPNoCenterFactoring::dump_lp(lp::LPObjectiveSense sense, const vector<lp::LPVariable> &vars, const vector<lp::LPConstraint> &constrs) const {
    if (sense == lp::LPObjectiveSense::MAXIMIZE){
        cout << "Linear Program: maximize objective value" << endl;
    } else {
        cout << "Linear Program: minimize objective value" << endl;
    }
    cout << vars.size() << " variables" << endl;
    cout << constrs.size() << " constraints" << endl;

    if (vars.size() > 100 || constrs.size() > 100){
        cout << "WARNING: not printing LP as it is too big" << endl;
        return;
    }

    cout << "variables: " << endl;
    int i = 0;
    for (auto &var : vars){
        cout << "x" << i++ << ": range " << var.lower_bound << " .. " << var.upper_bound << "; objective value " << var.objective_coefficient << (var.is_integer ? ", is integer variable" : "") << endl;
    }

    cout << endl;

    for (auto &constr : constrs){
        cout << constr.get_lower_bound() << " < ";
        for (size_t i = 0; i < constr.get_variables().size(); ++i){
             cout << constr.get_coefficients()[i] << " * x" << constr.get_variables()[i] << (i < constr.get_variables().size() - 1 ? " + " : "");
        }
        cout << " < " << constr.get_upper_bound() << endl;
    }
}

Factoring::FactoredVars LPNoCenterFactoring::get_max_mobile_leaves() const {

    utils::Timer timer;

    vector<lp::LPVariable> variables;
    vector<lp::LPConstraint> constraints;

    vector<ActionSchema> action_schemas = get_action_schemas();

    if (action_schemas.empty() || factoring_timer.is_expired()){
        // for trivially unsolvable task
        return FactoredVars();
    }

    vector<PotentialLeaf> potential_leaves = get_potential_leaves(action_schemas);

    if (potential_leaves.empty() || factoring_timer.is_expired()){
        return FactoredVars();
    }

    vector<set<size_t> > var_to_p_leaves(g_variable_domain.size());
    for (const PotentialLeaf &p_leaf : potential_leaves) {
        for (int var : p_leaf.vars) {
            var_to_p_leaves[var].insert(p_leaf.id);
        }
    }

    if (max_merge_steps > 0) {
        merge_potential_leaves(potential_leaves, var_to_p_leaves);
    }


    {

        vector<vector<bool> > p_leaves_cg(potential_leaves.size() - 1);
        for (size_t i = 0; i < p_leaves_cg.size(); ++i){
            p_leaves_cg[i].resize(potential_leaves.size() - i - 1, false);
        }

        for (int var = 0; var < (int) g_variable_domain.size(); ++var){
            // check if intersection non-empty
            for (int pot_leaf_1 : var_to_p_leaves[var]){
                for (int pot_leaf_2 : var_to_p_leaves[var]){
                    if (pot_leaf_1 < pot_leaf_2){
                        p_leaves_cg[pot_leaf_1][pot_leaf_2 - pot_leaf_1 - 1] = true;
                    }
                }
            }
        }

        for (size_t p_leaf = 0; p_leaf < potential_leaves.size(); ++p_leaf) {
            switch (strategy) {
            case MML:
                variables.push_back(lp::LPVariable(0.0, 1.0, 1.0, true));
                break;
            case MF:
                variables.push_back(lp::LPVariable(0.0, 1.0, potential_leaves[p_leaf].get_flexibility(), true));
                break;
            case MM:
                variables.push_back(lp::LPVariable(0.0, 1.0, potential_leaves[p_leaf].num_leaf_only_actions, true));
                break;
            case MMF:
                variables.push_back(lp::LPVariable(0.0, 1.0, potential_leaves[p_leaf].num_leaf_only_actions * potential_leaves[p_leaf].get_flexibility(), true));
                break;
            default: exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
            }
        }

        size_t num_constraints = 0;
        for (size_t p_leaf_1 = 0; p_leaf_1 < potential_leaves.size(); ++p_leaf_1) {
            for (size_t p_leaf_2 = p_leaf_1 + 1; p_leaf_2 < potential_leaves.size(); ++p_leaf_2) {
                // we need p_leaf_1 < p_leaf_2
                if (p_leaves_cg[p_leaf_1][p_leaf_2 - p_leaf_1 - 1]){
                    ++num_constraints;
                }
            }
        }

        if (memory_limit != -1 && num_constraints >= (size_t) memory_limit * 2500){
            // this limit corresponds to around 10 million constraints with 4GB memory limit
            // the maximum-sized ILP that could be solved in the experiments with this limit
            // had around 5 million constraints
            cerr << "Too many LP constraints, aborting: " << num_constraints << endl;
            return FactoredVars();
        }

        constraints.reserve(num_constraints);

        for (size_t p_leaf_1 = 0; p_leaf_1 < potential_leaves.size(); ++p_leaf_1) {
            for (size_t p_leaf_2 = p_leaf_1 + 1; p_leaf_2 < potential_leaves.size(); ++p_leaf_2) {
                // we need p_leaf_1 < p_leaf_2
                if (p_leaves_cg[p_leaf_1][p_leaf_2 - p_leaf_1 - 1]) {
                    lp::LPConstraint constraint(0.0, 1.0);
                    constraint.insert(p_leaf_1, 1.0);
                    constraint.insert(p_leaf_2, 1.0);
                    constraints.push_back(constraint);
                }
            }
        }

    }

    if (factoring_timer.is_expired()){
        return FactoredVars();
    }

    // save memory
    action_schemas = vector<ActionSchema>();
    var_to_p_leaves = vector<set<size_t> >();

    if (min_number_leaves > 2) {
        // adding the constraint typically increases runtime of CPLEX,
        // therefore, we only add it if we need strictly more than 2 leaves,
        // since 2 leaves are guaranteed by the elimination of potential leaves
        // in set_potential_leaves()
        lp::LPConstraint constraint(min_number_leaves, min(potential_leaves.size(), g_variable_domain.size()));
        for (size_t p_leaf = 0; p_leaf < potential_leaves.size(); ++p_leaf) {
            constraint.insert(p_leaf, 1.0);
        }
        constraints.push_back(constraint);
    }

    if (factoring_timer.is_expired()){
        return FactoredVars();
    }

    int cplex_limit = cplex_time_limit > 0 ? cplex_time_limit : -1;
    if (factoring_timer.get_remaining_time() != numeric_limits<double>::infinity()){
        // TODO: this check seems to work; make sure that this is actually guaranteed by the standard
        int f_limit = factoring_timer.get_remaining_time();
        cplex_limit = cplex_limit > 0 ? min(f_limit, cplex_limit) : f_limit;
    }

    vector<double> solution = solve_lp(lp::LPObjectiveSense::MAXIMIZE,
            variables, constraints, false, false, cplex_limit);

    FactoredVars factoring;

    if (solution.empty()) {
        return factoring;
    }

    // simple sanity check
    bool all_potential_leaves = true;
    bool leaves_overlap = false;
    vector<bool> leaf_variables(g_variable_domain.size(), false);
    for (size_t p_leaf = 0; p_leaf < potential_leaves.size(); ++p_leaf) {
        if (solution[p_leaf] == 1) {
            set<int> leaf;
            for (int var : potential_leaves[p_leaf].vars) {
                if (leaf_variables[var]){
                    leaves_overlap = true;
                }
                leaf_variables[var] = true;
                leaf.insert(var);
            }
            factoring.push_back(leaf);
        } else {
            all_potential_leaves = false;
        }
    }

    if (all_potential_leaves && leaves_overlap){
        // TODO: investigate this!
        // this tends to happen when CPLEX is running out of memory on the cluster,
        // when run with an external memory limit. could try to additionally set
        // the memory limit for CPLEX explicitly
        cerr << "ERROR: solution returned by CPLEX is not a valid factoring; all potential leaves become leaves, but do overlap." << endl;
        return FactoredVars();
    }

    return factoring;
}

vector<LPNoCenterFactoring::ActionSchema> LPNoCenterFactoring::get_action_schemas() const {
    vector<LPNoCenterFactoring::ActionSchema> action_schemas;
    utils::HashMap<std::vector<int>, size_t> scheme_loockup;
    for (const Operator &op : g_operators) {
        set<int> vars_tmp;
        for (const Condition &pre : op.get_preconditions()) {
            vars_tmp.insert(pre.var);
        }
        for (const Effect &eff : op.get_effects()) {
            vars_tmp.insert(eff.var);
        }
        vector<int> vars(vars_tmp.begin(), vars_tmp.end());
        sort(vars.begin(), vars.end());

        if (scheme_loockup.find(vars) == scheme_loockup.end()){
            scheme_loockup[vars] = action_schemas.size();
            action_schemas.push_back(ActionSchema(1, vars));
        } else {
            action_schemas[scheme_loockup[vars]].incr_num_action();
        }
    }
    if (min_mobility > 1 && max_merge_steps > 0) {
        // erase schemes with num_actions < min_num_actions
        action_schemas.erase(remove_if(action_schemas.begin(),
                    action_schemas.end(),
                    [&](const ActionSchema &schema){return schema.num_actions < min_mobility;}),
                action_schemas.end());
    }
    return action_schemas;
}

vector<LPNoCenterFactoring::PotentialLeaf> LPNoCenterFactoring::get_potential_leaves(const vector<ActionSchema> &action_schemas) const {
    assert(!action_schemas.empty());

    vector<LPNoCenterFactoring::PotentialLeaf> potential_leaves;

    {
        utils::HashMap<std::vector<int>, size_t> leaf_lookup;
        for (size_t as = 0; as < action_schemas.size(); ++as) {
            const ActionSchema &action_schema = action_schemas[as];
            uint64_t size = 1;
            for (int var : action_schema.vars){
                size *= g_variable_domain[var];
                if (max_leaf_size != 0 && size > max_leaf_size){
                    break;
                }
            }
            if (max_leaf_size != 0 && size > max_leaf_size){
                continue;
            }

            if (leaf_lookup.find(action_schema.vars) == leaf_lookup.end()){
                size_t s = potential_leaves.size();
                leaf_lookup[action_schema.vars] = s;
                potential_leaves.push_back(PotentialLeaf(s, action_schema.num_actions, action_schema.vars));
                potential_leaves[s].leaf_only_schemes.insert(as);
            } else {
                potential_leaves[leaf_lookup[action_schema.vars]].num_actions += action_schema.num_actions;
                potential_leaves[leaf_lookup[action_schema.vars]].leaf_only_schemes.insert(as);
            }
        }
    }

    cout << action_schemas.size() << " action schemes" << endl;
    cout << potential_leaves.size() << " potential leaves" << endl;

    vector<set<size_t> > var_to_p_leaves(g_variable_domain.size(), set<size_t>());

    for (const PotentialLeaf &p_leaf : potential_leaves) {
        for (int var : p_leaf.vars) {
            var_to_p_leaves[var].insert(p_leaf.id);
        }
    }

    // set number of leaf actions
    for (PotentialLeaf &p_leaf : potential_leaves) {
        assert(p_leaf.num_leaf_actions == 0);
        vector<bool> handled_leaves(potential_leaves.size(), false);
        for (int var : p_leaf.vars) {
            for (int index : var_to_p_leaves[var]) {
                if (!handled_leaves[index]){
                    handled_leaves[index] = true;
                    p_leaf.num_leaf_actions += potential_leaves[index].num_actions;
                }
            }
        }
        if (factoring_timer.is_expired()){
            return vector<PotentialLeaf>();
        }
    }

    if (min_mobility > 1 || min_flexibility > 0 ||
            strategy == STRATEGY::MF || strategy == STRATEGY::MM || strategy == STRATEGY::MMF) {
        // set the number of leaf-only actions
        for (const PotentialLeaf &p_leaf : potential_leaves) {
            set<int> superset_schemes;
            if (p_leaf.vars.size() == 1) {
                for (int index : var_to_p_leaves[p_leaf.vars[0]]) {
                    superset_schemes.insert(index);
                }
            } else {
                // since we consider all supersets of p_leaf, we can simply take p_leaf.vars[0],
                // since all supersets must also include that variable
                for (int index : var_to_p_leaves[p_leaf.vars[0]]) {
                    bool superset_schema = true;
                    for (int var_1 : p_leaf.vars) {
                        if (!binary_search(potential_leaves[index].vars.begin(), potential_leaves[index].vars.end(), var_1)){
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
                for (int leaf_only_schema : p_leaf.leaf_only_schemes) {
                    potential_leaves[index].leaf_only_schemes.insert(leaf_only_schema);
                }
            }
            if (factoring_timer.is_expired()){
                return vector<PotentialLeaf>();
            }
        }
    }

    if (min_mobility > 1 || min_flexibility > 0) {
        // erase effect schemes that does not satisfy requirements
        auto lambda_expr = [&](const PotentialLeaf &p_leaf){return p_leaf.num_leaf_only_actions < min_mobility || p_leaf.get_flexibility() < min_flexibility;};
        potential_leaves.erase(remove_if(potential_leaves.begin(), potential_leaves.end(), lambda_expr), potential_leaves.end());
        // adjust IDs
        size_t id = 0;
        for (PotentialLeaf &p_leaf : potential_leaves) {
            p_leaf.id = id;
            id++;
        }
        for (size_t var = 0; var < g_variable_domain.size(); ++var){
            var_to_p_leaves[var].clear();
        }
        for (const PotentialLeaf &p_leaf : potential_leaves) {
            for (int var : p_leaf.vars) {
                var_to_p_leaves[var].insert(p_leaf.id);
            }
        }
    }

    if (min_number_leaves >= 2 && !potential_leaves.empty()) {

        utils::Timer timer;
        cout << "Eliminating potential leaves... " << flush;

        size_t num_p_leaves = potential_leaves.size();
        vector<vector<bool> > p_leaves_cg(num_p_leaves - 1);
        for (size_t i = 0; i < p_leaves_cg.size(); ++i){
            p_leaves_cg[i].resize(num_p_leaves - i - 1, false);
        }

        for (int var = 0; var < (int) g_variable_domain.size(); ++var){
            // check if intersection non-empty
            for (int pot_leaf_1 : var_to_p_leaves[var]){
                for (int pot_leaf_2 : var_to_p_leaves[var]){
                    if (pot_leaf_1 < pot_leaf_2){
                        p_leaves_cg[pot_leaf_1][pot_leaf_2 - pot_leaf_1 - 1] = true;
                    }
                }
            }
            if (factoring_timer.is_expired()){
                cout << "timed out." << endl;
                return vector<PotentialLeaf>();
            }
        }

        vector<bool> fully_connected_p_leaves(num_p_leaves, false);
        for (size_t i_1 = 0; i_1 < num_p_leaves; ++i_1) {
            bool fully_connected = true;
            for (size_t i_2 = 0; i_2 < num_p_leaves; ++i_2) {
                if (i_1 < i_2) {
                    if (!p_leaves_cg[i_1][i_2 - i_1 - 1]){
                        fully_connected = false;
                        break;
                    }
                } else if (i_1 > i_2) {
                    if (!p_leaves_cg[i_2][i_1 - i_2 - 1]){
                        fully_connected = false;
                        break;
                    }
                }
            }
            if (fully_connected) {
                fully_connected_p_leaves[i_1] = true;
            }
        }
        auto is_fully_connected = [&](const PotentialLeaf &p_leaf){return fully_connected_p_leaves[p_leaf.id];};
        potential_leaves.erase(remove_if(potential_leaves.begin(),
                                    potential_leaves.end(),
                                    is_fully_connected),
                               potential_leaves.end());

        //adjust IDs
        int id = 0;
        for (PotentialLeaf &p_leaf : potential_leaves) {
            p_leaf.id = id;
            ++id;
        }

        cout << "done [" << timer << "]" << endl;

        cout << (num_p_leaves - potential_leaves.size()) << " potential leaves removed" << endl;
    }

    return potential_leaves;
}


void LPNoCenterFactoring::merge_potential_leaves(vector<PotentialLeaf> &potential_leaves,
        vector<set<size_t> > &var_to_p_leaves) const {

    queue<PotentialLeaf> open;
    for (PotentialLeaf &p_leaf : potential_leaves) {
        sort(p_leaf.vars.begin(), p_leaf.vars.end());
        if (p_leaf.vars.size() > 1) {
            open.push(p_leaf);
        }
    }

    int id = potential_leaves.size();
    vector<PotentialLeaf> merged_leaves;
    for (int iteration = 0; iteration < max_merge_steps; iteration++) {
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


void LPNoCenterFactoring::add_options_to_parser(OptionParser &parser) {

    vector<string> strategies;
    strategies.push_back("mml");
    strategies.push_back("mm");
    strategies.push_back("mf");
    strategies.push_back("mmf");
    parser.add_enum_option(
        "strategy",
        strategies,
        "TODO",
        "mml",
        vector<string>(),
        OptionFlags(false));

    vector<string> factoring_type;
    factoring_type.push_back("star");
    factoring_type.push_back("f");
    factoring_type.push_back("if");
    parser.add_enum_option(
        "factoring_type",
        factoring_type,
        "TODO",
        "star",
        vector<string>(),
        OptionFlags(false));

    parser.add_option<int>(
        "min_mobility",
        "TODO",
        "1"
    );

    parser.add_option<float>(
        "min_flexibility",
        "TODO",
        "0"
    );

    parser.add_option<int>(
        "max_merge_steps",
        "TODO",
        "0"
    );

    parser.add_option<int>(
        "cplex_time_limit",
        "CPLEX time limit in seconds",
        "-1"
    );

    parser.add_option<int>(
        "memory_limit",
        "CPLEX memory limit in MB",
        "-1"
    );

}

static Factoring *_parse_lp(OptionParser &parser) {
    // TODO docu
    Factoring::add_options_to_parser(parser);
    LPNoCenterFactoring::add_options_to_parser(parser);
    lp::add_lp_solver_option_to_parser(parser);

    Options opts = parser.parse();

    LPNoCenterFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new LPNoCenterFactoring(opts);
    }

    return factoring;
}

static Plugin<Factoring> _plugin_lp("lp_no_center", _parse_lp);
}
