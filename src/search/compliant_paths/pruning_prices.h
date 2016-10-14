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
    friend class Prices; // to get init state cpg
    
    static PruningPrices tmp_p_price;
    
    static PruningPriceStorage *cpg_storage;
    
    static CompliantPathGraph& get_init_state_prices();
    
    
    void compute_cost_frontier();
    void compute_cost_frontier(size_t factor);

    void goal_price_propagation();

    void propagate_dominance_prices(); 
    
protected:  
    
    std::vector<boost::dynamic_bitset<> > frontier;
    
    std::vector<std::vector<int> > effective_prices;
    
    std::vector<size_t> number_effective_states;
        
    
    PruningPrices(const PruningPrices *tag);

    
    bool add_effective_state(const LeafStateID &fact, int cost);
    
    // returns the new duplicate counter
    virtual CompliantPathGraph& update_price_tags(const State &base_state,
                                        const Operator *op,
                                        const State &predecessor) const override;
    
public:
    
    PruningPrices(){};
    
    virtual void store_new_graph(const State &state) const;
    
    virtual bool has_effective_leaf_state(const LeafStateID &id) const override;

    virtual int get_effective_cost_of_state(const LeafStateID &id) const override;

    virtual size_t get_number_effective_states(size_t factor) const override;
    
    virtual const std::vector<boost::dynamic_bitset<> > & get_frontier() override;
    
    virtual void dump() const override;

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;

    
    static void register_cpg_storage(PruningPriceStorage *storage);
    
};



#endif
