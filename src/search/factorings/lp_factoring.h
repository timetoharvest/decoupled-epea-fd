#ifndef FACTORINGS_LP_FACTORING_H
#define FACTORINGS_LP_FACTORING_H

#include "../factoring.h"


namespace lp {
class LinearProgram;
struct LPVariable;
class LPConstraint;
enum class LPSolverType;
}


namespace lp_factoring {
class LPFactoring : public Factoring {

    int cplex_time_limit;

    int memory_limit;

    lp::LPSolverType lp_solver_type;

protected:

    double infty;

    mutable utils::Timer timer;

    LPFactoring(const options::Options &opts);

    FactoredVars get_factoring() override = 0;

    bool check_timeout() const;

    // TODO num_variables is currently unused
    bool check_lp_size(size_t num_constraints, size_t num_variables) const;

    std::vector<double> solve_lp(lp::LinearProgram &lp,
            bool fail_on_out_of_memory = true,
            bool solve_lp_relaxation = false) const;

    void dump_lp(const lp::LinearProgram &lp) const;

public:

    static void add_options_to_parser(options::OptionParser &parser);

};
}

#endif
