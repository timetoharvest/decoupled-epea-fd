#include "leaf_state_id.h"

#include <ostream>

using namespace std;

const LeafStateID LeafStateID::no_state = LeafStateID(-1, -1);

ostream &operator<<(ostream &os, LeafStateID id) {
    os << "#"  << id.hash() << " factor " << id.get_factor();
    return os;
}
