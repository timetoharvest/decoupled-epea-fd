#include "mip_solver.h"

#include "mip_internals.h"

#include "../option_parser.h"

#include "../utils/system.h"

#ifdef USE_LP
#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <OsiSolverInterface.hpp>
#include <CoinPackedMatrix.hpp>
#include <CoinPackedVector.hpp>
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
#endif

#include <cassert>
#include <numeric>

using namespace std;
using utils::ExitCode;

namespace mip {
void add_mip_solver_option_to_parser(OptionParser &parser) {
    parser.document_note(
        "Note",
        "to use an MIP solver, you must build the planner with MIP support. ");
    vector<string> mip_solvers;
    vector<string> mip_solvers_doc;
    mip_solvers.push_back("CLP");
    mip_solvers_doc.push_back("default MIP solver shipped with the COIN library");
    mip_solvers.push_back("CPLEX");
    mip_solvers_doc.push_back("commercial solver by IBM");
    mip_solvers.push_back("GUROBI");
    mip_solvers_doc.push_back("commercial solver");
    parser.add_enum_option(
        "mipsolver",
        mip_solvers,
        "external solver that should be used to solve mixed integer linear programs",
        "CPLEX",
        mip_solvers_doc);
}

MIPConstraint::MIPConstraint(double lower_bound, double upper_bound)
    : lower_bound(lower_bound),
      upper_bound(upper_bound) {
}

void MIPConstraint::clear() {
    variables.clear();
    coefficients.clear();
}

bool MIPConstraint::empty() const {
    return variables.empty();
}

void MIPConstraint::insert(int index, double coefficient) {
    variables.push_back(index);
    coefficients.push_back(coefficient);
}

MIPVariable::MIPVariable(double lower_bound, double upper_bound,
                       double objective_coefficient, MIPVariableType type)
    : lower_bound(lower_bound),
      upper_bound(upper_bound),
      objective_coefficient(objective_coefficient),
      type(type) {
}

MIPSolver::~MIPSolver() {
}

#ifdef USE_LP

MIPSolver::MIPSolver(MIPSolverType solver_type)
    : is_initialized(false),
      is_solved(false),
      num_permanent_constraints(0),
      has_temporary_constraints_(false) {
    mip_solver = create_mip_solver(solver_type);
}

void MIPSolver::clear_temporary_data() {
    elements.clear();
    indices.clear();
    starts.clear();
    col_lb.clear();
    col_ub.clear();
    objective.clear();
    row_lb.clear();
    row_ub.clear();
    rows.clear();
    int_vars.clear();
}

void MIPSolver::load_problem(MIPObjectiveSense sense,
                            const vector<MIPVariable> &variables,
                            const vector<MIPConstraint> &constraints) {
    clear_temporary_data();
    is_initialized = false;
    num_permanent_constraints = constraints.size();

    for (vector<MIPVariable>::const_iterator it = variables.begin(); it != variables.end(); it++) {
        col_lb.push_back(it->lower_bound);
        col_ub.push_back(it->upper_bound);
        objective.push_back(it->objective_coefficient);
        if (it->type == MIPVariableType::INT) {
            int_vars.push_back(it - variables.begin());
        }
    }

    for (const MIPConstraint &constraint : constraints) {
        row_lb.push_back(constraint.get_lower_bound());
        row_ub.push_back(constraint.get_upper_bound());
    }

    if (sense == MIPObjectiveSense::MINIMIZE) {
        mip_solver->setObjSense(1);
    } else {
        mip_solver->setObjSense(-1);
    }

    for (const MIPConstraint &constraint : constraints) {
        const vector<int> &vars = constraint.get_variables();
        const vector<double> &coeffs = constraint.get_coefficients();
        assert(vars.size() == coeffs.size());
        starts.push_back(elements.size());
        indices.insert(indices.end(), vars.begin(), vars.end());
        elements.insert(elements.end(), coeffs.begin(), coeffs.end());
    }
    /*
      There are two ways to pass the lengths of vectors to a CoinMatrix:
      1) 'starts' contains one entry per vector and we pass a separate array
         of vector 'lengths' to the constructor.
      2) If there are no gaps in the elements, we can also add elements.size()
         as a last entry in the vector 'starts' and leave the parameter for
         'lengths' at its default (0).
      OSI recreates the 'lengths' array in any case and uses optimized code
      for the second case, so we use it here.
     */
    starts.push_back(elements.size());

    try {
        CoinPackedMatrix matrix(false,
                                variables.size(),
                                constraints.size(),
                                elements.size(),
                                elements.data(),
                                indices.data(),
                                starts.data(),
                                0);
        mip_solver->loadProblem(matrix,
                               col_lb.data(),
                               col_ub.data(),
                               objective.data(),
                               row_lb.data(),
                               row_ub.data());
        mip_solver->setInteger(&int_vars.front(), int_vars.size());
    } catch (CoinError &error) {
        handle_coin_error(error);
    }

    clear_temporary_data();
}

void MIPSolver::add_temporary_constraints(const vector<MIPConstraint> &constraints) {
    if (!constraints.empty()) {
        clear_temporary_data();
        int num_rows = constraints.size();
        for (const MIPConstraint &constraint : constraints) {
            row_lb.push_back(constraint.get_lower_bound());
            row_ub.push_back(constraint.get_upper_bound());
            rows.push_back(new CoinShallowPackedVector(
                               constraint.get_variables().size(),
                               constraint.get_variables().data(),
                               constraint.get_coefficients().data(),
                               false));
        }

        try {
            mip_solver->addRows(num_rows,
                               rows.data(), row_lb.data(), row_ub.data());
        } catch (CoinError &error) {
            handle_coin_error(error);
        }
        for (CoinPackedVectorBase *row : rows) {
            delete row;
        }
        clear_temporary_data();
        has_temporary_constraints_ = true;
        is_solved = false;
    }
}

void MIPSolver::clear_temporary_constraints() {
    if (has_temporary_constraints_) {
        try {
            mip_solver->restoreBaseModel(num_permanent_constraints);
        } catch (CoinError &error) {
            handle_coin_error(error);
        }
        has_temporary_constraints_ = false;
        is_solved = false;
    }
}

double MIPSolver::get_infinity() const {
    try {
        return mip_solver->getInfinity();
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
}

void MIPSolver::set_objective_coefficients(const vector<double> &coefficients) {
    assert(static_cast<int>(coefficients.size()) == get_num_variables());
    vector<int> indices(coefficients.size());
    iota(indices.begin(), indices.end(), 0);
    try {
        mip_solver->setObjCoeffSet(indices.data(),
                                  indices.data() + indices.size(),
                                  coefficients.data());
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
    is_solved = false;
}

void MIPSolver::set_objective_coefficient(int index, double coefficient) {
    assert(index < get_num_variables());
    try {
        mip_solver->setObjCoeff(index, coefficient);
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
    is_solved = false;
}

void MIPSolver::set_constraint_lower_bound(int index, double bound) {
    assert(index < get_num_constraints());
    try {
        mip_solver->setRowLower(index, bound);
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
    is_solved = false;
}

void MIPSolver::set_constraint_upper_bound(int index, double bound) {
    assert(index < get_num_constraints());
    try {
        mip_solver->setRowUpper(index, bound);
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
    is_solved = false;
}

void MIPSolver::set_variable_lower_bound(int index, double bound) {
    assert(index < get_num_variables());
    try {
        mip_solver->setColLower(index, bound);
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
    is_solved = false;
}

void MIPSolver::set_variable_upper_bound(int index, double bound) {
    assert(index < get_num_variables());
    try {
        mip_solver->setColUpper(index, bound);
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
    is_solved = false;
}

void MIPSolver::solve() {
    try {
        mip_solver->branchAndBound();
        if (mip_solver->isAbandoned()) {
            // The documentation of OSI is not very clear here but memory seems
            // to be the most common cause for this in our case.
            cerr << "Abandoned LP during resolve. "
                 << "Reasons include \"numerical difficulties\" and running out of memory." << endl;
            utils::exit_with(ExitCode::CRITICAL_ERROR);
        }
        is_solved = true;
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
}

void MIPSolver::solveLPRelaxation() {
    try {
        if (is_initialized) {
            mip_solver->resolve();
        } else {
            mip_solver->initialSolve();
            is_initialized = true;
        }
        if (mip_solver->isAbandoned()) {
            // The documentation of OSI is not very clear here but memory seems
            // to be the most common cause for this in our case.
            cerr << "Abandoned LP during resolve. "
                 << "Reasons include \"numerical difficulties\" and running out of memory." << endl;
            utils::exit_with(ExitCode::CRITICAL_ERROR);
        }
        is_solved = true;
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
}

bool MIPSolver::has_optimal_solution() const {
    assert(is_solved);
    try {
        return !mip_solver->isProvenPrimalInfeasible() &&
               !mip_solver->isProvenDualInfeasible() &&
               mip_solver->isProvenOptimal();
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
}

double MIPSolver::get_objective_value() const {
    assert(has_optimal_solution());
    try {
        return mip_solver->getObjValue();
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
}

vector<double> MIPSolver::extract_solution() const {
    assert(has_optimal_solution());
    try {
        const double *sol = mip_solver->getColSolution();
        return vector<double>(sol, sol + get_num_variables());
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
}

int MIPSolver::get_num_variables() const {
    try {
        return mip_solver->getNumCols();
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
}

int MIPSolver::get_num_constraints() const {
    try {
        return mip_solver->getNumRows();
    } catch (CoinError &error) {
        handle_coin_error(error);
    }
}

int MIPSolver::has_temporary_constraints() const {
    return has_temporary_constraints_;
}

void MIPSolver::print_statistics() const {
    cout << "MIP variables: " << get_num_variables() << endl;
    cout << "MIP constraints: " << get_num_constraints() << endl;
}

#endif
}
