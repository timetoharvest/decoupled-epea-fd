#ifndef DECOUPLED_DATA_STORE_H
#define DECOUPLED_DATA_STORE_H

#include "state.h"

class DecoupledDataStore {
        
    virtual void store_cached_results(const State &state) = 0;
    
};

#endif
