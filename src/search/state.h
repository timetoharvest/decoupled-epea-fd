#ifndef STATE_H
#define STATE_H

#include "int_packer.h"
#include "state_id.h"

#include <iostream>
#include <vector>

class StateRegistry;

typedef IntPacker::Bin PackedStateBin;

// For documentation on classes relevant to storing and working with registered
// states see the file state_registry.h.
class State {
    friend class StateRegistry;
    friend class PriceTag;
    template <class Entry>
    friend class PerStateInformation;
    
    // registry isn't a reference because we want to support operator=
    const StateRegistry *registry;
    
    StateID id;
    
    // Values for vars are maintained in a packed state and accessed on demand.
    const PackedStateBin *buffer;

    const PackedStateBin *get_packed_buffer() const {
        return buffer;
    }

    const StateRegistry &get_registry() const {
        return *registry;
    }

    // No implementation to prevent default construction
    State();
    
    // Only used by the state registry.
    State(const PackedStateBin *buffer_, const StateRegistry &registry_,
          StateID id_);
    
public:
    virtual ~State();

    StateID get_id() const {
        return id;
    }

    virtual int operator[](int index) const;

    virtual void dump_pddl() const;
    virtual void dump_fdr() const;
};

#endif
