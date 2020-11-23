#include "symbolic_state_cpg.h"

#include "cpg_storage.h"
#include "cudd_cpg.h"


using namespace std;



unique_ptr<CompliantPathGraph> SymbolicStateCPG::get_init_state_cpg() {
    return CuddCPG::get_init_state_cpg();
}

void SymbolicStateCPG::fill_in_leaf_actions(vector<OperatorID> &ops, vector<StateID> &states) {
    CuddCPG::fill_in_leaf_actions(ops, states);
}

void SymbolicStateCPG::print_statistics() {
    CuddCPG::print_statistics();
}
