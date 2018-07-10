#ifndef LP_SOLVER_H
#define LP_SOLVER_H

#include "../utils/language.h"

#include <functional>
#include <memory>
#include <vector>

namespace lp {

enum class LPObjectiveSense {
    MAXIMIZE, MINIMIZE
};

class LPConstraint {
    std::vector<int> variables;
    std::vector<double> coefficients;
    double lower_bound;
    double upper_bound;
public:
    LPConstraint(double lower_bound, double upper_bound);

    const std::vector<int> &get_variables() const {return variables; }
    const std::vector<double> &get_coefficients() const {return coefficients; }

    double get_lower_bound() const {return lower_bound; }
    void set_lower_bound(double lb) {lower_bound = lb; }
    double get_upper_bound() const {return upper_bound; }
    void set_upper_bound(double ub) {upper_bound = ub; }

    void clear();
    bool empty() const;
    // Coefficients must be added without duplicate indices.
    void insert(int index, double coefficient);
};

struct LPVariable {
    double lower_bound;
    double upper_bound;
    double objective_coefficient;

    LPVariable(double lower_bound,
               double upper_bound,
               double objective_coefficient);
};

enum class MIPVariableType {
    INT, CON
};

struct MIPVariable {
    double lower_bound;
    double upper_bound;
    double objective_coefficient;
    MIPVariableType type;

    MIPVariable(double lower_bound,
               double upper_bound,
               double objective_coefficient,
               MIPVariableType type);
};

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

class Solver {
protected:
    bool is_initialized;
    bool is_solved;
public:
    virtual void load_problem(
            LPObjectiveSense sense,
            const std::vector<LPVariable> &variables,
            const std::vector<LPConstraint> &constraints) = 0;
    virtual void load_problem(
            LPObjectiveSense sense,
            const std::vector<MIPVariable> &variables,
            const std::vector<LPConstraint> &constraints) = 0;
    virtual double get_infinity() const = 0;
    virtual void set_objective_coefficients(const std::vector<double> &coefficients) = 0;
    virtual void set_objective_coefficient(int index, double coefficient) = 0;
    virtual void set_constraint_lower_bound(int index, double bound) = 0;
    virtual void set_constraint_upper_bound(int index, double bound) = 0;
    virtual void set_variable_lower_bound(int index, double bound) = 0;
    virtual void set_variable_upper_bound(int index, double bound) = 0;

    virtual void solve() = 0;

    /*
      Return true if the solving the LP showed that it is bounded feasible and
      the discovered solution is guaranteed to be optimal. We test for
      optimality explicitly because solving the LP sometimes finds suboptimal
      solutions due to numerical difficulties.
      The LP has to be solved with a call to solve() before calling this method.
    */
    virtual bool has_optimal_solution() const = 0;

    /*
      Return the objective value found after solving an LP.
      The LP has to be solved with a call to solve() and has to have an optimal
      solution before calling this method.
    */
    virtual double get_objective_value() const = 0;

    /*
      Return the solution found after solving an LP as a vector with one entry
      per variable.
      The LP has to be solved with a call to solve() and has to have an optimal
      solution before calling this method.
    */
    virtual std::vector<double> extract_solution() const = 0;

    virtual int get_num_variables() const = 0;
    virtual int get_num_constraints() const = 0;
    void print_statistics() const;
};
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
}

#endif
