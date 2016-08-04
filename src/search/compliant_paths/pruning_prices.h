#ifndef PRUNING_PRICES_H
#define PRUNING_PRICES_H

#include "compliant_path_graph.h"
#include "../globals.h"
#include "../leaf_state.h"
#include "../leaf_state_id.h"
#include "pricing_function.h"
#include "../pruning/prunable.h"

#include <vector>

class PruningPriceStorage;

class PruningPrices : public Prices {
    
    static PruningPrices tmp_p_price;
    
    static PruningPriceStorage *cpg_storage;
    
    
    void compute_cost_frontier();

    void goal_price_propagation();

    void propagate_dominance_prices(); 
    
protected:  
    
    std::vector<boost::dynamic_bitset<> > frontier;
    
    std::vector<std::vector<int> > effective_prices;
    
    std::vector<size_t> number_effective_states;
        
    
    PruningPrices(const PruningPrices *tag);

    
    bool add_effective_state(const LeafStateID &fact, int cost);
    
    // returns the new duplicate counter
    virtual Prunable& update_price_tags(const State &base_state,
                                        const Operator *op,
                                        const State &predecessor) const;
    
public:
    
    PruningPrices(){};
    
    
    virtual void store_new_graph(const State &state) const;
    
    virtual bool has_effective_leaf_state(int id, int factor) const;

    virtual int get_effective_cost_of_state(int id, int factor) const;

    virtual size_t get_number_effective_states(int factor) const;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier();
    
    virtual void dump() const;

    
    static void register_cpg_storage(PruningPriceStorage *storage);

    static void create_init_state_prices(const State &state);
    
};



#endif
