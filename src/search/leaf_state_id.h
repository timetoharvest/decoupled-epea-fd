#ifndef LEAF_STATE_ID_H
#define LEAF_STATE_ID_H

#include <ext/hash_set>

// For documentation on classes relevant to storing and working with registered
// states see the file state_registry.h.

class LeafStateID {
    friend class StateRegistry;
    friend std::ostream &operator<<(std::ostream &os, LeafStateID id);
    template<typename>
    friend class PerStateInformation;
   
    int value;
    
    int factor;
    
    // No implementation to prevent default construction
    LeafStateID();
    
public:
    
    explicit LeafStateID(int id, int factor)
        : value(id), factor(factor) {
    }
    
    ~LeafStateID() {
    }

    static const LeafStateID no_state;

    bool operator==(const LeafStateID &other) const {
        return value == other.value;
    }

    bool operator!=(const LeafStateID &other) const {
        return !(*this == other);
    }
    
    bool operator<(const LeafStateID &other) const {
        return value < other.value;
    }

    size_t hash() const {
        return value;
    }
    
    int get_factor() const {
        return factor;
    }
};

std::ostream &operator<<(std::ostream &os, LeafStateID id);

namespace __gnu_cxx {
template<>
struct hash<LeafStateID> {
    size_t operator()(LeafStateID id) const {
        return id.hash();
    }
};
}

#endif
