#ifndef SYMMETRIES_GRAPH_CREATOR_H
#define SYMMETRIES_GRAPH_CREATOR_H

#ifdef USE_BLISS
#include <graph.hh>
#endif
#include "decoupled_group.h"
#include "group.h"
#include "../plugin.h"
#include "../state.h"

//TODO: Add vertex for axioms.
enum color_t {PREDICATE_VERTEX, VALUE_VERTEX, PRECOND_VERTEX, EFFECT_VERTEX,
              GOAL_VERTEX, INIT_VERTEX, CONDITIONAL_EFFECT_VERTEX,
	      CONDITIONAL_DELETE_EFFECT_VERTEX, CENTER_FACTOR_VERTEX, MAX_VALUE};

/**
 * This class will create a bliss graph which will be used to find the
 * automorphism groups
 */

class GraphCreator  {
    
    bool initialized;
    int time_bound;
    int generators_bound;
public:

    GraphCreator(const Options &opts);
    virtual ~GraphCreator();

    void initialize();

    Permutation create_permutation_from_trace(Trace& auth) const {
        return group->compose_permutation(auth);
    }

    void get_trace(const State& state, Trace& full_trace) {
        group->get_trace(state, full_trace);
    }

    const vector<int> & get_canonical_state(const State & state)  {
        return group->get_canonical_state(state);
    }
    
    const std::vector<int> & get_canonical_decoupled_state(const State & center,
                                                           CompliantPathGraph &leaves)  const {
	assert(decoupled_group);
        return decoupled_group->get_canonical_decoupled_state (center, leaves);
    }
    
//     const Operator* get_real_action(const State &state, const CompliantPathGraph &cpg, const Operator *op,
//     TODO implement this             const State &succ_state, const CompliantPathGraph &succ_cpg) const;
    
    Permutation create_permutation_from_state_to_state(const State &from_state, 
                                                       const State &to_state);

    static void add_options_to_parser(OptionParser &parser);
    
    void free_memory() { group->free_memory(); }

private:
    unique_ptr<Group> group;
    unique_ptr<DecoupledGroup> decoupled_group;

//#ifdef USE_BLISS
    bliss::Digraph* create_bliss_directed_graph() const;
    void add_operator_directed_graph(bliss::Digraph* g, const Operator& op) const;
    /* bool effect_can_be_overwritten(int ind, const std::vector<PrePost>& prepost) const; */
//#endif
};

#endif
