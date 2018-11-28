#include "cplex_solver.h"
#include "../utils/system.h"

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

#include <cassert>
#include <numeric>

using namespace std;

namespace lp {

static const string CPLEX_ERROR_MEMORY_1 = "CPLEX Error  1001: Out of memory.\n";
static const string CPLEX_ERROR_MEMORY_2 = "not enough memory";

CPLEXSolver::~CPLEXSolver() {
    cplex.end();
    model.end();
    ilo_objective.end();
    ilo_constraints.endElements();
    ilo_constraints.end();
    ilo_variables.endElements();
    ilo_variables.end();
    env.end();
}

CPLEXSolver::CPLEXSolver(int max_num_threads, double integrality_tolerance, int time_limit)
    : ilo_variables(IloNumVarArray(env,0)),
      ilo_objective(IloObjective(env)),
      ilo_constraints(IloRangeArray(env,0)),
      model(IloModel(env)),
      cplex(IloCplex(env)) {
    is_initialized = false;
    is_solved = false;
    cplex.setParam(IloCplex::Param::Threads, max_num_threads);
    cplex.setParam(IloCplex::Param::MIP::Tolerances::Integrality, integrality_tolerance);
    if (time_limit > 0){
        cplex.setParam(IloCplex::Param::TimeLimit, time_limit);
        cplex.setParam(IloCplex::Param::MIP::Limits::Populate, 5);
        cplex.setParam(IloCplex::Param::MIP::Pool::Replace, CPX_SOLNPOOL_OBJ);
    }
}

void CPLEXSolver::load_problem(LPObjectiveSense sense,
                            const vector<LPVariable> &variables,
                            const vector<LPConstraint> &constraints) {

    IloNumExpr ilo_obj_expr;
    for (const LPVariable &var : variables) {
        IloNumVar ilo_var(env, var.lower_bound, var.upper_bound, ILOFLOAT);
        ilo_variables.add(ilo_var);
        ilo_obj_expr += var.objective_coefficient * ilo_var;
    }

    if (sense == LPObjectiveSense::MINIMIZE) {
        ilo_objective.setExpr(ilo_obj_expr);
        ilo_objective.setSense(IloObjective::Minimize);
        model.add(ilo_objective);
    } else {
        ilo_objective.setExpr(ilo_obj_expr);
        ilo_objective.setSense(IloObjective::Maximize);
        model.add(ilo_objective);
    }
    ilo_obj_expr.end();

    for (const LPConstraint &constraint : constraints) {
        const vector<int> &vars = constraint.get_variables();
        const vector<double> &coeffs = constraint.get_coefficients();
        assert(vars.size() == coeffs.size());
        IloExpr ilo_constr_expr(env);
        for (size_t i = 0; i < vars.size(); i++) {
            ilo_constr_expr += coeffs[i] * ilo_variables[vars[i]];
        }
        IloRange ilo_constraint (env, constraint.get_lower_bound(), ilo_constr_expr, constraint.get_upper_bound());
        ilo_constraints.add(ilo_constraint);
        ilo_constr_expr.end();
    }
    model.add(ilo_constraints);
}

void CPLEXSolver::load_problem(LPObjectiveSense sense,
                            const vector<MIPVariable> &variables,
                            const vector<LPConstraint> &constraints) {
    IloNumExpr ilo_obj_expr(env);
    for (const MIPVariable &var : variables) {
        if (var.type == MIPVariableType::INT) {
          IloNumVar ilo_var (env, var.lower_bound, var.upper_bound, ILOINT);
          ilo_variables.add(ilo_var);
          ilo_obj_expr += var.objective_coefficient * ilo_var;
        } else {
          IloNumVar ilo_var (env, var.lower_bound, var.upper_bound, ILOFLOAT);
          ilo_variables.add(ilo_var);
          ilo_obj_expr += var.objective_coefficient * ilo_var;
        }
    }
    if (sense == LPObjectiveSense::MINIMIZE) {
        ilo_objective.setExpr(ilo_obj_expr);
        ilo_objective.setSense(IloObjective::Minimize);
        model.add(ilo_objective);
    } else {
        ilo_objective.setExpr(ilo_obj_expr);
        ilo_objective.setSense(IloObjective::Maximize);
        model.add(ilo_objective);
    }

    for (const LPConstraint &constraint : constraints) {
        const vector<int> &vars = constraint.get_variables();
        const vector<double> &coeffs = constraint.get_coefficients();
        assert(vars.size() == coeffs.size());
        IloExpr ilo_constr_expr(env);
        for (size_t i = 0; i < vars.size(); i++) {
            ilo_constr_expr += coeffs[i] * ilo_variables[vars[i]];
        }
        IloRange ilo_constraint (env, constraint.get_lower_bound(), ilo_constr_expr, constraint.get_upper_bound());
        ilo_constraints.add(ilo_constraint);
    }
    model.add(ilo_constraints);
}


double CPLEXSolver::get_infinity() const {
    return IloInfinity;
}

void CPLEXSolver::set_objective_coefficients(const vector<double> &coefficients) {
    assert(static_cast<int>(coefficients.size()) == get_num_variables());
    for (size_t i = 0; i < coefficients.size(); i++) {
        set_objective_coefficient(i,coefficients[i]);
    }
    is_solved = false;
}

void CPLEXSolver::set_objective_coefficient(int index, double coefficient) {
    assert(index < get_num_variables());
    ilo_objective.setLinearCoef(ilo_variables[index], coefficient);
    is_solved = false;
}

void CPLEXSolver::set_constraint_lower_bound(int index, double bound) {
    assert(index < get_num_constraints());
    ilo_constraints[index].setLB(bound);
    is_solved = false;
}

void CPLEXSolver::set_constraint_upper_bound(int index, double bound) {
    assert(index < get_num_constraints());
    ilo_constraints[index].setUB(bound);
    is_solved = false;
}

void CPLEXSolver::set_variable_lower_bound(int index, double bound) {
    assert(index < get_num_variables());
    ilo_variables[index].setLB(bound);
    is_solved = false;
}

void CPLEXSolver::set_variable_upper_bound(int index, double bound) {
    assert(index < get_num_variables());
    ilo_variables[index].setUB(bound);
    is_solved = false;
}

void CPLEXSolver::solve() {
    try {
        cplex.extract(model);
        cplex.solve();
        is_solved = true;
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

void CPLEXSolver::populate() {
    try {
        cplex.extract(model);
        cplex.populate();
        is_solved = true;
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

void CPLEXSolver::solveLPRelaxation() {
    try {
        IloModel relaxed_model(env);
        relaxed_model.add(model);
        relaxed_model.add(IloConversion (env, ilo_variables, ILOFLOAT));
        cplex.extract(relaxed_model);
        cplex.solve();
        is_solved = true;
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

bool CPLEXSolver::has_optimal_solution() const {
    try {
        assert(is_solved);
        return cplex.getStatus() == IloAlgorithm::Optimal;
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

bool CPLEXSolver::has_solution() const {
    try {
        assert(is_solved);
        if (cplex.getStatus() == IloAlgorithm::Optimal ||
                cplex.getStatus() == IloAlgorithm::Feasible){
            return true;
        }
        return false;
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

double CPLEXSolver::get_objective_value() const {
    try {
        assert(has_optimal_solution());
        return cplex.getObjValue();
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

vector<int> CPLEXSolver::extract_int_solution() const {
    try {
        assert(has_optimal_solution());
        cout << "CPLEX status: " << cplex.getStatus() << endl;
        vector<int> solution;
        for (int var = 0; var < get_num_variables(); var++) {
            solution.push_back(IloRound(cplex.getValue(ilo_variables[var])));
        }
        return solution;
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

vector<int> CPLEXSolver::extract_best_int_solution() const {
    try {
        assert(has_solution());
        cout << "CPLEX status: " << cplex.getStatus() << endl;
        int best = 0;
        for (int i = 0; i < cplex.getSolnPoolNsolns(); ++i){
            if (cplex.getObjValue(i) > cplex.getObjValue(best)){
                best = i;
            }
        }
        vector<int> solution;
        for (int var = 0; var < get_num_variables(); var++) {
            solution.push_back(IloRound(cplex.getValue(ilo_variables[var], best)));
        }
        return solution;
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

vector<double> CPLEXSolver::extract_solution() const {
    try {
        assert(has_optimal_solution());
        cout << "CPLEX status: " << cplex.getStatus() << endl;
        vector<double> solution;
        for (int var = 0; var < get_num_variables(); var++) {
            solution.push_back(cplex.getValue(ilo_variables[var]));
        }
        return solution;
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

vector<double> CPLEXSolver::extract_best_solution() const {
    try {
        assert(has_solution());
        cout << "CPLEX status: " << cplex.getStatus() << endl;
        int best = 0;
        for (int i = 0; i < cplex.getSolnPoolNsolns(); ++i){
            if (cplex.getObjValue(i) > cplex.getObjValue(best)){
                best = i;
            }
        }
        vector<double> solution;
        for (int var = 0; var < get_num_variables(); var++) {
            solution.push_back(cplex.getValue(ilo_variables[var], best));
        }
        return solution;
    } catch (IloException &exception) {
        handle_cplex_error(exception);
    }
}

int CPLEXSolver::get_num_variables() const {
    return ilo_variables.getSize();
}

int CPLEXSolver::get_num_constraints() const {
    return ilo_constraints.getSize();
}

NO_RETURN
void CPLEXSolver::handle_cplex_error(const IloException &exception) const{
  string error_message = exception.getMessage();
  if (error_message == CPLEX_ERROR_MEMORY_1 ||
      error_message == CPLEX_ERROR_MEMORY_2) {
      cout << "Error while factoring planning task!" << endl;
      cout << "CPLEX Out of Memory" << endl;
      exit_with(utils::ExitCode::OUT_OF_MEMORY);
  } else {
      cout << "Error : " << error_message << "\n";
      exit_with(utils::ExitCode::CRITICAL_ERROR);
  }
}


}
