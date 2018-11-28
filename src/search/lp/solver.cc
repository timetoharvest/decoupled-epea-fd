#include "solver.h"
#include "../utils/system.h"

#ifdef USE_LP
#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
#endif

using namespace std;
using utils::ExitCode;

namespace lp {

LPConstraint::LPConstraint(double lower_bound, double upper_bound)
    : lower_bound(lower_bound),
      upper_bound(upper_bound) {
}

void LPConstraint::clear() {
    variables.clear();
    coefficients.clear();
}

bool LPConstraint::empty() const {
    return variables.empty();
}

void LPConstraint::insert(int index, double coefficient) {
    variables.push_back(index);
    coefficients.push_back(coefficient);
}

LPVariable::LPVariable(double lower_bound, double upper_bound,
                       double objective_coefficient)
    : lower_bound(lower_bound),
      upper_bound(upper_bound),
      objective_coefficient(objective_coefficient) {
}

MIPVariable::MIPVariable(double lower_bound, double upper_bound,
                       double objective_coefficient, MIPVariableType type)
    : lower_bound(lower_bound),
      upper_bound(upper_bound),
      objective_coefficient(objective_coefficient),
      type(type) {
}

void Solver::print_statistics() const {
    cout << "LP variables: " << get_num_variables() << endl;
    cout << "LP constraints: " << get_num_constraints() << endl;
}

}
