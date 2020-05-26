#ifndef LP_CPLEX_SOLVER_H
#define LP_CPLEX_SOLVER_H

#include "lp_solver.h"

#include <functional>
#include <memory>
#include <vector>

#ifdef COIN_HAS_CPX1
#include <ilocplex.h>
#endif


namespace lp {

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
class CPLEXSolver : public LPSolver {
#ifdef COIN_HAS_CPX1
    IloEnv env;
    IloNumVarArray ilo_variables;
    IloObjective ilo_objective;
    IloRangeArray ilo_constraints;
    IloModel model;
    IloCplex cplex;
#endif
public:
    LP_METHOD(explicit CPLEXSolver(int time_limit = -1))

    virtual ~CPLEXSolver();

    LP_METHOD(virtual void load_problem(
            LPObjectiveSense sense,
            const std::vector<LPVariable> &variables,
            const std::vector<LPConstraint> &constraints) override)

    LP_METHOD(virtual double get_infinity() const override)
    LP_METHOD(virtual void set_objective_coefficients(const std::vector<double> &coefficients) override)
    LP_METHOD(virtual void set_objective_coefficient(int index, double coefficient) override)
    LP_METHOD(virtual void set_constraint_lower_bound(int index, double bound) override)
    LP_METHOD(virtual void set_constraint_upper_bound(int index, double bound) override)
    LP_METHOD(virtual void set_variable_lower_bound(int index, double bound) override)
    LP_METHOD(virtual void set_variable_upper_bound(int index, double bound) override)

    LP_METHOD(virtual void solve() override)
    // solves the LP, populating the solution pool
    LP_METHOD(void populate())
    LP_METHOD(void solveLPRelaxation())

    /*
      Return true if the solving the LP showed that it is bounded feasible and
      the discovered solution is guaranteed to be optimal. We test for
      optimality explicitly because solving the LP sometimes finds suboptimal
      solutions due to numerical difficulties.
      The LP has to be solved with a call to solve() before calling this method.
    */
    LP_METHOD(virtual bool has_optimal_solution() const override)

    /*
      Return true if the solving the LP showed that it is bounded feasible.
      The LP has to be solved with a call to solve() or populate() before calling this method.
     */
    LP_METHOD(bool has_solution() const)

    /*
      Return the objective value found after solving an LP.
      The LP has to be solved with a call to solve() and has to have an optimal
      solution before calling this method.
    */
    LP_METHOD(virtual double get_objective_value() const override)

    /*
      Return the solution found after solving an LP as a vector with one entry
      per variable.
      The LP has to be solved with a call to solve() and has to have an optimal
      solution before calling this method.
    */
    LP_METHOD(virtual std::vector<double> extract_solution() const override)

    LP_METHOD(std::vector<double> extract_best_solution() const)

    LP_METHOD(virtual int get_num_variables() const override)
    LP_METHOD(virtual int get_num_constraints() const override)
};
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
}

#endif
