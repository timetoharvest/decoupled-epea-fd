#include "state.h"

#include "globals.h"
#include "utilities.h"
#include "state_registry.h"

#include <algorithm>
#include <iostream>
#include <cassert>
using namespace std;


State::State(const PackedStateBin *buffer_, const StateRegistry &registry_,
             StateID id_)
    : buffer(buffer_),
      registry(&registry_),
      id(id_) {
    assert(buffer);
    assert(id != StateID::no_state);
}

State::~State() {
}

int State::operator[](size_t index) const {
    size_t i = index;
    if (g_use_decoupled_search){
        assert((size_t) g_new_index[index] < g_center_vars.size());
        i = g_new_index[index];
    }
    return g_state_packer->get(buffer, i);
}

void State::dump_pddl() const {
    if (g_use_decoupled_search){
        for (size_t var = 0; var < g_center_vars.size(); ++var) {
            const string &fact_name = g_fact_names[g_center_vars[var]][(*this)[g_center_vars[var]]];
            if (fact_name != "<none of those>")
                cout << fact_name << endl;
        }
    } else {
	for (size_t var = 0; var < g_variable_domain.size(); ++var) {
	    const string &fact_name = g_fact_names[var][(*this)[var]];
	    if (fact_name != "<none of those>")
		cout << fact_name << endl;
	}
    }
}

void State::dump_fdr() const {
    if (g_use_decoupled_search){
        for (size_t i = 0; i < g_center_vars.size(); ++i){
            cout << "  #" << i << " [" << g_variable_name[g_center_vars[i]] << "] -> " << (*this)[g_center_vars[i]] << endl;
        }
    } else {
	for (size_t i = 0; i < g_variable_domain.size(); ++i){
	    cout << "  #" << i << " [" << g_variable_name[i] << "] -> " << (*this)[i] << endl;
	}
    }
}
