#include "cudd_cpg.h"

#include "cpg_storage.h"
#include "../factoring.h"
#include "../leaf_state_id.h"
#include "../operator.h"
#include "../state.h"

#include <functional>

using namespace std;


bool CuddCPG::initialized = false;


CuddCPG::CuddCPG(const CuddCPG &/*other*/) {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

CuddCPG& CuddCPG::operator=(CuddCPG &&/*other*/) {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return *this;
}

unique_ptr<CompliantPathGraph> CuddCPG::get_init_state_cpg() {
    if (!initialized){
        initialize();
        initialized = true;
    }
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return unique_ptr<CompliantPathGraph>(nullptr);
}

void CuddCPG::initialize() {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

unique_ptr<CompliantPathGraph> CuddCPG::get_successor_via_center_action(const State &, const Operator &) const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return unique_ptr<CompliantPathGraph>(nullptr);
}

DOMINANCE CuddCPG::check_dominance(const State &, uint64_t , int  , DOMINANCE ) {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return DOMINANCE::NONE;
}

void CuddCPG::store_new_cpg(const State &) {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

void CuddCPG::discard_new_cpg() const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

bool CuddCPG::is_hypercube_covered(const State &, int ) const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return false;
}

void CuddCPG::populate_reached_leaf_facts(function<void(int, int)> ) const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

void CuddCPG::populate_cost_of_leaf_facts(function<void(int, int, int)> ) const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

vector<pair<int, int> > CuddCPG::get_leaf_fact_set(LeafStateHash , LeafFactorID ) const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return vector<pair<int, int>>();
}

bool CuddCPG::goal_reachable(LeafFactorID ) const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return false;
}

int CuddCPG::get_goal_cost(LeafFactorID ) const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return 0;
}

bool CuddCPG::is_applicable(const Operator &) const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return false;
}

void CuddCPG::remove_inapplicable_ops(vector<OperatorID> &) const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

uint64_t CuddCPG::get_hash() const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return 0;
}

unique_ptr<CompliantPathGraph> CuddCPG::clone() const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return unique_ptr<CompliantPathGraph>(nullptr);
}

void CuddCPG::dump() const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

size_t CuddCPG::get_memory_footprint() const {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
    return 0;
}

void CuddCPG::fill_in_leaf_actions(vector<OperatorID> &, vector<StateID> &) {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

void CuddCPG::print_statistics() {
    // TODO
    cerr << "symbolic leaf representation not yet implemented" << endl;
    exit_with(utils::ExitCode::SEARCH_UNSUPPORTED);
}

