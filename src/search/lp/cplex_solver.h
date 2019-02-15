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

    virtual ~CPLEXSolver();

    // TODO probably refactor these two, to not have the potentially huge
    // vector of constraints in memory twice; instead, build the LP incrementally.
    virtual void load_problem(
            LPObjectiveSense sense,
            const std::vector<LPVariable> &variables,
            const std::vector<LPConstraint> &constraints) override;
    virtual void load_problem(
            LPObjectiveSense sense,
            const std::vector<MIPVariable> &variables,
            const std::vector<LPConstraint> &constraints) override;

    virtual double get_infinity() const override;
    virtual void set_objective_coefficients(const std::vector<double> &coefficients) override;
    virtual void set_objective_coefficient(int index, double coefficient) override;
    virtual void set_constraint_lower_bound(int index, double bound) override;
    virtual void set_constraint_upper_bound(int index, double bound) override;
    virtual void set_variable_lower_bound(int index, double bound) override;
    virtual void set_variable_upper_bound(int index, double bound) override;

    virtual void solve() override;
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
    virtual bool has_optimal_solution() const override;

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
    virtual double get_objective_value() const override;

    /*
      Return the solution found after solving an LP as a vector with one entry
      per variable.
      The LP has to be solved with a call to solve() and has to have an optimal
      solution before calling this method.
    */
    virtual std::vector<double> extract_solution() const override;

    std::vector<double> extract_best_solution() const;

    std::vector<int> extract_int_solution() const;

    std::vector<int> extract_best_int_solution() const;

    virtual int get_num_variables() const override;
    virtual int get_num_constraints() const override;
};
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
}

#endif
