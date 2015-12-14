#ifndef DECOUPLED_DATA_STORE_H
#define DECOUPLED_DATA_STORE_H

class State;

class DecoupledDataStore {
    friend class StateRegistry;
        
    virtual void store_cached_results(const State &state) = 0;
    
};

#endif
