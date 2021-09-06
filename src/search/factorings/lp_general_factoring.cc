#include "lp_general_factoring.h"

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
#include <math.h>


using namespace std;

static const string CPLEX_ERROR_MEMORY_1 = "CPLEX Error  1001: Out of memory.\n";
static const string CPLEX_ERROR_MEMORY_2 = "not enough memory";

namespace lp_general_factoring {
void LPGeneralFactoring::PotentialLeaf::add_leaf_only_schema(size_t action_schema) {
    if (std::find(action_schemes.begin(),
            action_schemes.end(),
            action_schema) == action_schemes.end()){
        action_schemes.push_back(action_schema);
        num_actions += action_schemas[action_schema].num_actions;
        bool all_in = true;
        for (int pre : action_schemas[action_schema].pre_vars){
            if (!std::binary_search(vars.begin(), vars.end(), pre)){
                all_in = false;
                break;
            }
        }
        if (all_in){
            self_mobile_as.push_back(action_schema);
        }
    }
}

LPGeneralFactoring::LPGeneralFactoring(const Options &opts) : Factoring(opts),
        strategy(STRATEGY(opts.get_enum("strategy"))),
        min_mobility(opts.get<int>("min_mobility")),
        min_flexibility(opts.get<float>("min_flexibility")),
        add_cg_sccs_(opts.get<bool>("add_cg_sccs")),
        max_merge_steps(opts.get<int>("max_merge_steps")),
        merge_overlapping(opts.get<bool>("merge_overlapping")),
        merge_dependent(opts.get<bool>("merge_dependent")),
        cplex_time_limit(opts.get<int>("cplex_time_limit")),
        memory_limit(opts.get<int>("memory_limit")),
        lp_solver_type(lp::LPSolverType(opts.get_enum("lpsolver"))),
        timer(utils::Timer()),
        infty(-1) {
    profile = STAR;
    cout << "using LP factoring strategy" << endl;

    if (max_merge_steps > 0 && !merge_dependent && !merge_overlapping){
        cerr << "At least one of \"merge_dependent\" or \"merge_overlapping\" needs to be set when merging leaves." << endl;
        exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }

#ifdef USE_CPLEX
    lp::CPLEXSolver solver;
#else
    lp::LPSolver solver(lp_solver_type);
#endif
    infty = solver.get_infinity();
}


Factoring::FactoredVars LPGeneralFactoring::get_factoring() {
    if (min_flexibility > 0){
        cout << "WARNING: minimum flexibility not supported, yet." << endl;
    }

    timer.reset();

    return compute_factoring();
}

vector<double> LPGeneralFactoring::solve_lp(lp::LPObjectiveSense sense,
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

    cout << "LP objective value: " << solver.get_objective_value() << endl;

    if (time_limit == -1){
        solution = solver.extract_solution();
    } else {
#ifdef USE_CPLEX
        solution = solver.extract_best_solution();
#endif
    }

    return solution;
}

void LPGeneralFactoring::dump_lp(lp::LPObjectiveSense sense, const vector<lp::LPVariable> &vars, const vector<lp::LPConstraint> &constrs) const {
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

bool LPGeneralFactoring::check_num_constraints_and_time(const vector<lp::LPConstraint> &constraints) const {
    if (memory_limit != -1 && constraints.size() >= (size_t) memory_limit * 2500){
        // this limit corresponds to around 10 million constraints with 4GB memory limit
        // the maximum-sized ILP that could be solved in the experiments with this limit
        // had around 5 million constraints
        cerr << "Too many LP constraints, aborting: " << constraints.size() << endl;
        return false;
    }
    if (factoring_timer.is_expired()){
        exit_if_timeout();
        return false;
    }
    return true;
}

inline void get_combinations(const vector<size_t> &act_schemas,
        vector<vector<size_t> > &combinations,
        vector<size_t> &current,
        size_t i = 0) {
    if (i == act_schemas.size()){
        combinations.push_back(current);
        current.pop_back();
        return;
    }
    for (size_t as_num = i; as_num < act_schemas.size(); ++as_num){
        current.push_back(act_schemas[as_num]);
        get_combinations(act_schemas, combinations, current, as_num + 1);
    }
    if (i != 0){
        combinations.push_back(current);
        current.pop_back();
    }
}

inline double get_log(size_t num_actions) {
    return log(max(1.0001, (double) num_actions));
}

Factoring::FactoredVars LPGeneralFactoring::compute_factoring() const {

    vector<lp::LPVariable> variables;
    vector<lp::LPConstraint> constraints;

    init_action_schemas();
    filter_action_schemas();

    if (action_schemas.empty()){
        // mostly for trivially unsolvable task?
        cerr << "ERROR: No action schemas." << endl;
        return FactoredVars();
    }
    if (factoring_timer.is_expired()){
        exit_if_timeout();
        return FactoredVars();
    }

    vector<PotentialLeaf> potential_leaves = get_potential_leaves();

    if (potential_leaves.empty()){
        cout << "No potential leaves." << endl;
        return FactoredVars();
    }
    if (factoring_timer.is_expired()){
        exit_if_timeout();
        return FactoredVars();
    }

    if (add_cg_sccs_){
        add_cg_sccs(potential_leaves);
    }

    if (max_merge_steps > 0) {
        merge_potential_leaves(potential_leaves);
    }


    variables.reserve(potential_leaves.size() + 1.5 * action_schemas.size() + g_variable_domain.size());

    // LP variables
    vector<double> min_leaf_mobility;
    vector<size_t> non_self_mobile_leaf_actions;
    if (strategy == MM_APPROX){
        min_leaf_mobility.resize(potential_leaves.size(), 0.0);
        non_self_mobile_leaf_actions.resize(potential_leaves.size(), 0);
    }
    // binary var for each potential leaf; 1 => becomes leaf, 0 => not
    for (const auto &pleaf : potential_leaves) {
        double min_mob = 0.0;
        if (strategy == MM_APPROX){
            non_self_mobile_leaf_actions[pleaf.id] = pleaf.num_actions;
            if (!pleaf.self_mobile_as.empty()){
                size_t mob = 0;
                for (size_t act_schema : pleaf.self_mobile_as){
                    mob += action_schemas[act_schema].num_actions;
                    non_self_mobile_leaf_actions[pleaf.id] -= action_schemas[act_schema].num_actions;
                }
                min_mob = get_log(mob);
                min_leaf_mobility[pleaf.id] = min_mob;
            }
        }
        variables.push_back(lp::LPVariable(0.0, 1.0, min_mob, true));
    }
    vector<vector<size_t>> mob_var_id(potential_leaves.size());
    // binary var for each action schema of each potential leaf; 1 => is mobile, 0 => not
    for (const PotentialLeaf &pleaf : potential_leaves){
        for (size_t act_schema : pleaf.action_schemes){
            mob_var_id[pleaf.id].push_back(variables.size());
            switch (strategy) {
            case MML: variables.push_back(lp::LPVariable(0.0, 1.0, 1.0, true)); break;
            case MM_OPT: variables.push_back(lp::LPVariable(0.0, 1.0, 0.0, true)); break;
            case MM_APPROX:
                if (find(pleaf.self_mobile_as.begin(), pleaf.self_mobile_as.end(), act_schema) == pleaf.self_mobile_as.end()){
                    variables.push_back(lp::LPVariable(0.0,
                            1.0,
                            action_schemas[act_schema].num_actions *
                            (get_log(pleaf.num_actions) - min_leaf_mobility[pleaf.id]) /
                            non_self_mobile_leaf_actions[pleaf.id],
                            true));
                }
                break;
                //        MF: variables.push_back(lp::LPVariable(0.0, 1.0, , true)); break; // TODO
            default: cerr << "unknown strategy: " << strategy << endl; exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
            }
        }
    }
    vector<vector<size_t>> as_combs;
    if (strategy == MM_OPT){
        as_combs.resize(potential_leaves.size());
        // binary var for each combination of mobile action schemas of each potential leaf; 1 => combination is mobile, 0 => not
        for (const PotentialLeaf &pleaf : potential_leaves){
            vector<vector<size_t>> combinations;
            vector<size_t> tmp;
            get_combinations(pleaf.action_schemes, combinations, tmp);
            for (const auto &act_schemas : combinations){
                as_combs[pleaf.id].push_back(variables.size());
                size_t num_actions = 0;
                for (size_t as : act_schemas){
                    num_actions += action_schemas[as].num_actions;
                }
                variables.push_back(lp::LPVariable(0.0, 1.0, get_log(num_actions), true));
            }
        }
    }
    size_t vars_offset = variables.size();
    // binary var for each FDR variable; 1 => is in center, 0 => not
    for (size_t var = 0; var < g_variable_domain.size(); ++var){
        variables.push_back(lp::LPVariable(0.0, 1.0, 0.0, true));
    }

    if (strategy == MM_OPT){
        // for each potential leaf, exactly one combination of its action schemas is mobile
        for (const PotentialLeaf &pleaf : potential_leaves){
            size_t i = 0;
            vector<vector<size_t>> combinations;
            vector<size_t> tmp;
            get_combinations(pleaf.action_schemes, combinations, tmp);
            for (size_t c = 0; c < combinations.size(); ++c){
                lp::LPConstraint constraint(-infty, 0.0);
                constraint.insert(pleaf.id, -1.0);
                constraint.insert(as_combs[pleaf.id][i], 1.0 + combinations[c].size());
                for (size_t as : combinations[c]){
                    // TODO build a map for this
                    size_t id = find(pleaf.action_schemes.begin(), pleaf.action_schemes.end(), as) - pleaf.action_schemes.begin();
                    constraint.insert(mob_var_id[pleaf.id][id], -1.0);
                }
                constraints.push_back(constraint);

                if (combinations[c].size() < pleaf.action_schemes.size()){
                    lp::LPConstraint constraint1(-infty, pleaf.action_schemes.size() - combinations[c].size());
                    constraint1.insert(as_combs[pleaf.id][i], pleaf.action_schemes.size() - combinations[c].size());
                    for (size_t as_num = 0; as_num < pleaf.action_schemes.size(); ++as_num){
                        // TODO avoid searching
                        if (find(combinations[c].begin(), combinations[c].end(), pleaf.action_schemes[as_num]) == combinations[c].end()){
                            constraint1.insert(mob_var_id[pleaf.id][as_num], 1.0);
                        }
                    }
                    constraints.push_back(constraint1);
                }
                ++i;
            }
        }
    }

    if (!check_num_constraints_and_time(constraints)){
        return FactoredVars();
    }


    // non-empty intersection between potential leaves
    {
        vector<vector<bool> > pleaf_intersect(potential_leaves.size() - 1);
        for (size_t i = 0; i < pleaf_intersect.size(); ++i){
            pleaf_intersect[i].resize(potential_leaves.size() - i - 1, false);
        }
        for (int var = 0; var < (int) g_variable_domain.size(); ++var){
            for (int pot_leaf_1 : var_to_p_leaves[var]){
                for (int pot_leaf_2 : var_to_p_leaves[var]){
                    if (pot_leaf_1 < pot_leaf_2){
                        pleaf_intersect[pot_leaf_1][pot_leaf_2 - pot_leaf_1 - 1] = true;
                    }
                }
            }
        }
        for (size_t p_leaf_1 = 0; p_leaf_1 < potential_leaves.size(); ++p_leaf_1) {
            for (size_t p_leaf_2 = p_leaf_1 + 1; p_leaf_2 < potential_leaves.size(); ++p_leaf_2) {
                // we need p_leaf_1 < p_leaf_2
                if (pleaf_intersect[p_leaf_1][p_leaf_2 - p_leaf_1 - 1]) {
                    lp::LPConstraint constraint(0.0, 1.0);
                    constraint.insert(p_leaf_1, 1.0);
                    constraint.insert(p_leaf_2, 1.0);
                    constraints.push_back(constraint);
                }
            }
        }
    }

    if (!check_num_constraints_and_time(constraints)){
        return FactoredVars();
    }


    // at least one action schema needs to be mobile in each leaf
    for (size_t pleaf = 0; pleaf < potential_leaves.size(); ++pleaf) {
        lp::LPConstraint constraint(-infty, 0.0);
        constraint.insert(pleaf, 1.0);
        for (size_t as_num = 0; as_num < potential_leaves[pleaf].action_schemes.size(); ++as_num) {
            constraint.insert(mob_var_id[pleaf][as_num], -1.0);
        }
        constraints.push_back(constraint);
    }


    // set center vars if potential leaf is not a leaf
    for (size_t pleaf = 0; pleaf < potential_leaves.size(); ++pleaf) {
        lp::LPConstraint constraint(-infty, potential_leaves[pleaf].vars.size());
        constraint.insert(pleaf, potential_leaves[pleaf].vars.size());
        for (int var : potential_leaves[pleaf].vars) {
            constraint.insert(vars_offset + var, 1.0);
        }
        constraints.push_back(constraint);
    }

    if (!check_num_constraints_and_time(constraints)){
        return FactoredVars();
    }


    // set action schemas mobile if (1) it is part of a leaf, and
    // (2) precondition variables outside the leaf are in the center
    for (size_t pleaf = 0; pleaf < potential_leaves.size(); ++pleaf){
        if (strategy == MML && !potential_leaves[pleaf].self_mobile_as.empty()){
            continue;
        }
        for (size_t as_num = 0; as_num < potential_leaves[pleaf].action_schemes.size(); ++as_num) {
            set<int> outside_pre_vars;
            for (int var : action_schemas[potential_leaves[pleaf].action_schemes[as_num]].pre_vars){
                if (!binary_search(potential_leaves[pleaf].vars.begin(),
                        potential_leaves[pleaf].vars.end(),
                        var)){
                    outside_pre_vars.insert(var);
                }
            }

            // action schema is mobile only if it is a leaf and all pre variables are center
            lp::LPConstraint constraint(-infty, 0.0);
            constraint.insert(pleaf, -1.0);
            constraint.insert(mob_var_id[pleaf][as_num], outside_pre_vars.size() + 1.0);
            for (int var : outside_pre_vars){
                constraint.insert(vars_offset + var, -1.0);
            }
            constraints.push_back(constraint);
        }
    }

    if (!check_num_constraints_and_time(constraints)){
        return FactoredVars();
    }

    // save memory
    action_schemas = vector<ActionSchema>();

    if (min_number_leaves >= 2) {
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

    if (!check_num_constraints_and_time(constraints)){
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

    if (solution.empty()) {
        cerr << "ERROR: no solution found by LP solver." << endl;
        exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }

    FactoredVars factoring;

    bool leaves_overlap = false;
    vector<bool> is_leaf_var(g_variable_domain.size(), false);
    for (size_t p_leaf = 0; p_leaf < potential_leaves.size(); ++p_leaf) {
        if (solution[p_leaf] == 1) {
            set<int> leaf;
            for (int var : potential_leaves[p_leaf].vars) {
                if (is_leaf_var[var]){
                    leaves_overlap = true;
                    // simple sanity check
                }
                is_leaf_var[var] = true;
                leaf.insert(var);
            }
            factoring.push_back(leaf);
        }
    }

    if (leaves_overlap){
        // TODO: investigate this!
        // this tends to happen when CPLEX is running out of memory on the cluster,
        // when run with an external memory limit. could try to additionally set
        // the memory limit for CPLEX explicitly
        cerr << "ERROR: solution returned by LP solver is not a valid factoring: leaves do overlap." << endl;
        exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }

    return factoring;
}

void LPGeneralFactoring::filter_action_schemas() const {
    if (min_mobility > 1 && max_merge_steps == 0) {
        // erase schemes with num_actions < min_num_actions
        action_schemas.erase(remove_if(action_schemas.begin(),
                    action_schemas.end(),
                    [&](const ActionSchema &schema){return schema.num_actions < min_mobility;}),
                action_schemas.end());
    }
}

vector<LPGeneralFactoring::PotentialLeaf> LPGeneralFactoring::get_potential_leaves() const {
    assert(!action_schemas.empty());

    vector<PotentialLeaf> potential_leaves;

    {
        utils::HashMap<std::vector<int>, size_t> leaf_lookup;
        for (size_t as = 0; as < action_schemas.size(); ++as) {
            const ActionSchema &action_schema = action_schemas[as];
            uint64_t size = 1;
            for (int var : action_schema.eff_vars){
                size *= g_variable_domain[var];
                if (max_leaf_size != 0 && size > max_leaf_size){
                    break;
                }
            }
            if (max_leaf_size != 0 && size > max_leaf_size){
                continue;
            }

            if (leaf_lookup.find(action_schema.eff_vars) == leaf_lookup.end()){
                size_t s = potential_leaves.size();
                leaf_lookup[action_schema.eff_vars] = s;
                potential_leaves.push_back(PotentialLeaf(s, action_schema.eff_vars));
                potential_leaves[s].add_leaf_only_schema(as);
            } else {
                potential_leaves[leaf_lookup[action_schema.eff_vars]].add_leaf_only_schema(as);
            }
        }
    }

    cout << action_schemas.size() << " action schemes" << endl;
    cout << potential_leaves.size() << " potential leaves" << endl;

    var_to_p_leaves = vector<set<size_t> >(g_variable_domain.size(), set<size_t>());

    for (const PotentialLeaf &p_leaf : potential_leaves) {
        for (int var : p_leaf.vars) {
            var_to_p_leaves[var].insert(p_leaf.id);
        }
    }

    // set the number of leaf-only actions
    for (const PotentialLeaf &p_leaf : potential_leaves) {
        set<size_t> superset_schemes;
        if (p_leaf.vars.size() == 1) {
            for (size_t index : var_to_p_leaves[p_leaf.vars[0]]) {
                superset_schemes.insert(index);
            }
        } else {
            // since we consider all supersets of p_leaf, we can simply take p_leaf.vars[0],
            // since all supersets must also include that variable
            for (size_t index : var_to_p_leaves[p_leaf.vars[0]]) {
                bool superset_schema = true;
                for (int var : p_leaf.vars) {
                    if (!binary_search(potential_leaves[index].vars.begin(), potential_leaves[index].vars.end(), var)){
                        superset_schema = false;
                        break;
                    }
                }
                if (superset_schema) {
                    superset_schemes.insert(index);
                }
            }
        }
        for (size_t index : superset_schemes) {
            for (size_t leaf_only_schema : p_leaf.action_schemes) {
                potential_leaves[index].add_leaf_only_schema(leaf_only_schema);
            }
        }
        if (factoring_timer.is_expired()){
            exit_if_timeout();
            return vector<PotentialLeaf>();
        }
    }

    if (min_mobility > 1) {
        // erase effect schemes that does not satisfy requirements
        auto lambda_expr = [&](const PotentialLeaf &p_leaf){return p_leaf.num_actions < min_mobility;};
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
        for (const PotentialLeaf &pleaf : potential_leaves) {
            for (int var : pleaf.vars) {
                var_to_p_leaves[var].insert(pleaf.id);
            }
        }
    }

    return potential_leaves;
}

void LPGeneralFactoring::add_cg_sccs(vector<PotentialLeaf> &potential_leaves) const {

    FactoredVars sccs = get_sccs();

    if (sccs.size() == 1){
        return;
    }

    utils::HashSet<std::vector<int>> leaf_lookup;

    for (const PotentialLeaf &pleaf : potential_leaves){
        leaf_lookup.insert(pleaf.vars);
    }

    size_t added = 0;
    for (const auto &scc : sccs) {
        uint64_t size = 1;
        for (int var : scc){
            size *= g_variable_domain[var];
            if (max_leaf_size != 0 && size > max_leaf_size){
                break;
            }
        }
        if (max_leaf_size != 0 && size > max_leaf_size){
            continue;
        }

        vector<int> vars(scc.begin(), scc.end());
        sort(vars.begin(), vars.end());

        if (leaf_lookup.find(vars) == leaf_lookup.end()){
            size_t s = potential_leaves.size();
            for (int var : scc) {
                var_to_p_leaves[var].insert(s);
            }

            set<size_t> subset_schemes;
            // since we consider all supersets of p_leaf, we can simply take p_leaf.vars[0],
            // since all supersets must also include that variable
            for (int scc_var : scc){
                for (size_t index : var_to_p_leaves[scc_var]) {
                    if (index >= potential_leaves.size() - added){
                        continue;
                    }
                    bool subset_schema = true;
                    for (int var : potential_leaves[index].vars) {
                        if (find(scc.begin(), scc.end(), var) == scc.end()){
                            subset_schema = false;
                            break;
                        }
                    }
                    if (subset_schema) {
                        subset_schemes.insert(index);
                    }
                }
            }

            potential_leaves.push_back(PotentialLeaf(s, vars));

            for (size_t pleaf : subset_schemes){
                assert(pleaf < potential_leaves.size() - added);
                for (const auto &as : potential_leaves[pleaf].action_schemes){
                    potential_leaves[s].add_leaf_only_schema(as);
                }
            }

            ++added;
        }
        if (factoring_timer.is_expired()){
            exit_if_timeout();
            return;
        }
    }

    cout << "Added " << added << " causal-graph SCC potential leaves." << endl;

    if (added == 0){
        return;
    }

    if (min_mobility > 1) {
        // erase effect schemes that does not satisfy requirements
        auto lambda_expr = [&](const PotentialLeaf &pleaf){return pleaf.num_actions < min_mobility;};
        potential_leaves.erase(remove_if(potential_leaves.end() - added, potential_leaves.end(), lambda_expr), potential_leaves.end());
        // adjust IDs
        size_t id = 0;
        for (PotentialLeaf &p_leaf : potential_leaves) {
            p_leaf.id = id;
            id++;
        }
        for (size_t var = 0; var < g_variable_domain.size(); ++var){
            var_to_p_leaves[var].clear();
        }
        for (const PotentialLeaf &pleaf : potential_leaves) {
            for (int var : pleaf.vars) {
                var_to_p_leaves[var].insert(pleaf.id);
            }
        }
    }
}


void LPGeneralFactoring::merge_potential_leaves(vector<PotentialLeaf> &potential_leaves) const {

    queue<size_t> open;
    for (const PotentialLeaf &p_leaf : potential_leaves) {
        open.push(p_leaf.id);
    }

    const causal_graph::CausalGraph &cg = causal_graph::get_causal_graph();

    size_t added = 0;

    size_t id = potential_leaves.size();
    vector<PotentialLeaf> merged_leaves;
    for (int iteration = 0; iteration < max_merge_steps; ++iteration) {
        while (!open.empty()) {
            const PotentialLeaf &p_leaf = potential_leaves[open.front()];
            open.pop();

            set<size_t> merge_candidates;

            if (merge_overlapping){
                for (int var : p_leaf.vars) {
                    for (size_t index : var_to_p_leaves[var]) {
                        merge_candidates.insert(index);
                    }
                }

                set<size_t> erase_cands;
                for (size_t cand : merge_candidates){
                    const vector<int> &vars = potential_leaves[cand].vars;
                    if (vars.size() == 1 ||
                            includes(p_leaf.vars.begin(), p_leaf.vars.end(), vars.begin(), vars.end()) ||
                            includes(vars.begin(), vars.end(), p_leaf.vars.begin(), p_leaf.vars.end())) {
                        erase_cands.insert(cand);
                    }
                }
                for (size_t c : erase_cands){
                    merge_candidates.erase(c);
                }
            }

            if (merge_dependent){
                for (int var : p_leaf.vars) {
                    for (int c_var : cg.get_predecessors(var)){
                        for (size_t index : var_to_p_leaves[c_var]) {
                            merge_candidates.insert(index);
                        }
                    }
                    for (int c_var : cg.get_successors(var)){
                        for (size_t index : var_to_p_leaves[c_var]) {
                            merge_candidates.insert(index);
                        }
                    }
                }

                set<size_t> erase_cands;
                for (size_t cand : merge_candidates){
                    const vector<int> &vars = potential_leaves[cand].vars;
                    if (includes(p_leaf.vars.begin(), p_leaf.vars.end(), vars.begin(), vars.end()) ||
                            includes(vars.begin(), vars.end(), p_leaf.vars.begin(), p_leaf.vars.end())) {
                        erase_cands.insert(cand);
                    }
                }
                for (size_t c : erase_cands){
                    merge_candidates.erase(c);
                }
            }
            for (size_t cand : merge_candidates) {
                vector<int> merged_vars;
                merged_vars.reserve(p_leaf.vars.size() + potential_leaves[cand].vars.size());
                set_union(p_leaf.vars.begin(),
                        p_leaf.vars.end(),
                        potential_leaves[cand].vars.begin(),
                        potential_leaves[cand].vars.end(),
                        back_inserter(merged_vars));

                //duplicate check
                bool duplicate = false;
                for (size_t j : var_to_p_leaves[merged_vars[0]]) {
                    if (potential_leaves[j].vars == merged_vars) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    for (const PotentialLeaf &merged_leaf : merged_leaves) {
                        if (merged_leaf.vars == merged_vars) {
                            duplicate = true;
                            break;
                        }
                    }
                }
                if (duplicate) {
                    continue;
                }

                PotentialLeaf merged_leaf(id, merged_vars);

                //calculate number of leaf actions and leaf-only actions
                set<size_t> overlapping_with_merged_leaf;
                for (int var : merged_vars) {
                    for (size_t j : var_to_p_leaves[var]) {
                        overlapping_with_merged_leaf.insert(j);
                    }
                }
                for (size_t ol_leaf : overlapping_with_merged_leaf) {
                    if (includes(merged_vars.begin(),
                            merged_vars.end(),
                            potential_leaves[ol_leaf].vars.begin(),
                            potential_leaves[ol_leaf].vars.end())) {
                        for (size_t as : potential_leaves[ol_leaf].action_schemes){
                            merged_leaf.add_leaf_only_schema(as);
                        }
                    }
                }

                merged_leaves.push_back(merged_leaf);
                ++id;
            }
        }
        added += merged_leaves.size();
        for (const PotentialLeaf &merged_leaf : merged_leaves) {
            assert(potential_leaves.size() == merged_leaf.id);
            open.push(potential_leaves.size());
            potential_leaves.push_back(merged_leaf);
            for (int var : merged_leaf.vars) {
                var_to_p_leaves[var].insert(merged_leaf.id);
            }
        }
        merged_leaves.clear();
    }
    cout << "Added " << added << " merged leaves." << endl;
}


void LPGeneralFactoring::add_options_to_parser(OptionParser &parser) {

    vector<string> strategies;
    strategies.push_back("mml");
    strategies.push_back("mm_opt");
    strategies.push_back("mm_approx");
//    strategies.push_back("mf");
    parser.add_enum_option(
            "strategy",
            strategies,
            "TODO",
            "mml",
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

    parser.add_option<bool>(
            "add_cg_sccs",
            "TODO",
            "false"
    );

    parser.add_option<int>(
            "max_merge_steps",
            "TODO",
            "0"
    );

    parser.add_option<bool>(
            "merge_dependent",
            "TODO",
            "false"
    );

    parser.add_option<bool>(
            "merge_overlapping",
            "TODO",
            "false"
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
    LPGeneralFactoring::add_options_to_parser(parser);
    lp::add_lp_solver_option_to_parser(parser);

    Options opts = parser.parse();

    LPGeneralFactoring *factoring = 0;
    if (!parser.dry_run()) {
        factoring = new LPGeneralFactoring(opts);
    }

    return factoring;
}

static Plugin<Factoring> _plugin_lp("lp_general", _parse_lp);
}
