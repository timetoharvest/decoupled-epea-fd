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
    : registry(&registry_),
      id(id_),
      buffer(buffer_){
    assert(buffer);
    assert(id != StateID::no_state);
}

State::~State() {
}

int State::operator[](int index) const {
    int i = index;
    if (g_use_decoupled_search){
        assert(g_new_index[index] < g_number_center_vars);
        i = g_new_index[index];
    }
    return g_state_packer->get(buffer, i);
}

void State::dump_pddl() const {
    if (g_use_decoupled_search){
        for (int var = 0; var < g_number_center_vars; var++) {
            const string &fact_name = g_fact_names[g_center_var[var]][(*this)[g_center_var[var]]];
            if (fact_name != "<none of those>")
                cout << fact_name << endl;
        }
    } else {
	for (int var = 0; var < g_variable_domain.size(); var++) {
	    const string &fact_name = g_fact_names[var][(*this)[var]];
	    if (fact_name != "<none of those>")
		cout << fact_name << endl;
	}
    }
}

void State::dump_fdr() const {
    if (g_use_decoupled_search){
        for (size_t i = 0; i < g_number_center_vars; ++i){
            cout << "  #" << i << " [" << g_variable_name[g_center_var[i]] << "] -> " << (*this)[g_center_var[i]] << endl;
        }
    } else {
	for (size_t i = 0; i < g_variable_domain.size(); ++i){
	    cout << "  #" << i << " [" << g_variable_name[i] << "] -> " << (*this)[i] << endl;
	}
    }
}
