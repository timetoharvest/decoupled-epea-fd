#ifndef PRUNING_REACHABLE_H
#define PRUNING_REACHABLE_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "../globals.h"
#include "../leaf_state.h"
#include "../leaf_state_id.h"
#include "../pruning/prunable.h"
#include "reachability_function.h"

#include <vector>

class PruningReachable : public Reachable {
    
    static PruningReachable tmp_p_reach;
    
protected:
    
    std::vector<boost::dynamic_bitset<> > frontier;
    
    
    PruningReachable(const PruningReachable *tag);
    
    // returns the duplicate counter for this base state and price tag 
    // i.e. the counter of the subsuming that or the current max + 1
    int check_duplicate(const State &base_state, int curr_number_duplicates, 
                        int g_value, const State &predecessor);

    // returns the new duplicate counter
    virtual int update_price_tags(int curr_number_duplicates,
                                  const State &base_state,
                                  int g_value,
                                  const State &predecessor,
                                  const Operator *op) const;
    
    void compute_frontier();
    
public:
    
    PruningReachable(){};


    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier();
    
    virtual void dump() const;
    
    
    static void store_new_graph(const State &state);
    
    static void create_init_state_reachable();
 
};



#endif
