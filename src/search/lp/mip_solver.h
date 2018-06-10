#ifndef MIP_MIP_SOLVER_H
#define MIP_MIP_SOLVER_H

#include "../utils/language.h"
#include "../utils/system.h"

#include <functional>
#include <memory>
#include <vector>

/*
  All methods that use COIN specific classes only do something useful
  if the planner is compiled with USE_LP. Otherwise, they just print
  an error message and abort.
*/
#ifdef USE_LP
#define MIP_METHOD(X) X;
#else
#define MIP_METHOD(X) NO_RETURN X { \
        ABORT("MIP method called but the planner was compiled without MIP support.\n"); \
}
#endif

class CoinPackedVectorBase;
class OsiSolverInterface;

namespace options {
class OptionParser;
}

namespace mip {
enum class MIPSolverType {
    CLP, CPLEX, GUROBI
};

enum class MIPObjectiveSense {
    MAXIMIZE, MINIMIZE
};

enum class MIPVariableType {
    INT, CON
};

void add_mip_solver_option_to_parser(options::OptionParser &parser);

class MIPConstraint {
    std::vector<int> variables;
    std::vector<double> coefficients;
    double lower_bound;
    double upper_bound;
public:
    MIPConstraint(double lower_bound, double upper_bound);

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
class MIPSolver {
    bool is_initialized;
    bool is_solved;
    int num_permanent_constraints;
    bool has_temporary_constraints_;
#ifdef USE_LP
    std::unique_ptr<OsiSolverInterface> mip_solver;
#endif

    /*
      Temporary data for assigning a new problem. We keep the vectors
      around to avoid recreating them in every assignment.
    */
    std::vector<double> elements;
    std::vector<int> indices;
    std::vector<int> starts;
    std::vector<double> col_lb;
    std::vector<double> col_ub;
    std::vector<double> objective;
    std::vector<double> row_lb;
    std::vector<double> row_ub;
    std::vector<CoinPackedVectorBase *> rows;
    std::vector<int> int_vars;
    void clear_temporary_data();
public:
    MIP_METHOD(explicit MIPSolver(MIPSolverType solver_type))
    /*
      Note that the destructor does not use MIP_METHOD because it should not
      have the attribute NO_RETURN. It also cannot be set to the default
      destructor here (~LPSolver() = default;) because OsiSolverInterface
      is a forward declaration and the incomplete type cannot be destroyed.
    */
    ~MIPSolver();

    MIP_METHOD(void load_problem(
                  MIPObjectiveSense sense,
                  const std::vector<MIPVariable> &variables,
                  const std::vector<MIPConstraint> &constraints))
    MIP_METHOD(void add_temporary_constraints(const std::vector<MIPConstraint> &constraints))
    MIP_METHOD(void clear_temporary_constraints())
    MIP_METHOD(double get_infinity() const)

    MIP_METHOD(void set_objective_coefficients(const std::vector<double> &coefficients))
    MIP_METHOD(void set_objective_coefficient(int index, double coefficient))
    MIP_METHOD(void set_constraint_lower_bound(int index, double bound))
    MIP_METHOD(void set_constraint_upper_bound(int index, double bound))
    MIP_METHOD(void set_variable_lower_bound(int index, double bound))
    MIP_METHOD(void set_variable_upper_bound(int index, double bound))

    MIP_METHOD(void solve())
    MIP_METHOD(void solveLPRelaxation())

    /*
      Return true if solving the MIP or the MIPs LP relaxation showed that it is bounded feasible and
      the discovered solution is guaranteed to be optimal. We test for
      optimality explicitly because solving the MIP or the MIPs LP relaxation sometimes finds suboptimal
      solutions due to numerical difficulties.
      The MIP has to be solved with a call to solve() or solveLPRelaxation() before calling this method.
    */
    MIP_METHOD(bool has_optimal_solution() const)

    /*
      Return the objective value found after solving the MIP or MIPs LP relaxation.
      The MIP has to be solved with a call to solve() or solveLPRelaxation() and has to have an optimal
      solution before calling this method.
    */
    MIP_METHOD(double get_objective_value() const)

    /*
      Return the solution found after solving an MIP as a vector with one entry
      per variable.
      The MIP has to be solved with a call to solve() or solveLPRelaxation() and has to have an optimal
      solution before calling this method.
    */
    MIP_METHOD(std::vector<double> extract_solution() const)

    MIP_METHOD(int get_num_variables() const)
    MIP_METHOD(int get_num_constraints() const)
    MIP_METHOD(int has_temporary_constraints() const)
    MIP_METHOD(void print_statistics() const)
};
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
}

#endif
