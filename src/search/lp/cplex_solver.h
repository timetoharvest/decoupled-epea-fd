#ifndef LP_CPLEX_SOLVER_H
#define LP_CPLEX_SOLVER_H

#include "../utils/language.h"
#include "../utils/system.h"
#include "solver.h"

#include <ilcplex/ilocplex.h>
#include <functional>
#include <memory>
#include <vector>


namespace lp {

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

class CPLEXSolver : public Solver {
    IloEnv env;
    IloNumVarArray ilo_variables;
    IloObjective ilo_objective;
    IloRangeArray ilo_constraints;
    IloModel model;
    IloCplex cplex;
public:
    CPLEXSolver(int max_num_threads = 1, double integrality_tolerance = 0.0, int time_limit = -1);

    ~CPLEXSolver();

    void load_problem(
            LPObjectiveSense sense,
            const std::vector<LPVariable> &variables,
            const std::vector<LPConstraint> &constraints);
    void load_problem(
            LPObjectiveSense sense,
            const std::vector<MIPVariable> &variables,
            const std::vector<LPConstraint> &constraints);

    double get_infinity() const;
    void set_objective_coefficients(const std::vector<double> &coefficients);
    void set_objective_coefficient(int index, double coefficient);
    void set_constraint_lower_bound(int index, double bound);
    void set_constraint_upper_bound(int index, double bound);
    void set_variable_lower_bound(int index, double bound);
    void set_variable_upper_bound(int index, double bound);

    void solve();
    // solves the MIP, populating the solution pool
    void populate();
    void solveLPRelaxation();

    /*
      Return true if the solving the LP showed that it is bounded feasible and
      the discovered solution is guaranteed to be optimal. We test for
      optimality explicitly because solving the LP sometimes finds suboptimal
      solutions due to numerical difficulties.
      The LP has to be solved with a call to solve() before calling this method.
    */
    bool has_optimal_solution() const;

    /*
      Return true if the solving the LP showed that it is bounded feasible.
      The LP has to be solved with a call to solve() or populate() before calling this method.
     */
    bool has_solution() const;

    /*
      Return the objective value found after solving an LP.
      The LP has to be solved with a call to solve() and has to have an optimal
      solution before calling this method.
    */
    double get_objective_value() const;

    /*
      Return the solution found after solving an LP as a vector with one entry
      per variable.
      The LP has to be solved with a call to solve() and has to have an optimal
      solution before calling this method.
    */
    std::vector<double> extract_solution() const;

    std::vector<double> extract_best_solution() const;

    std::vector<int> extract_int_solution() const;

    std::vector<int> extract_best_int_solution() const;

    int get_num_variables() const;
    int get_num_constraints() const;

    NO_RETURN
    void handle_cplex_error(const IloException &exception) const;
};
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
}

#endif
