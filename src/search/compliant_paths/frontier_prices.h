#ifndef FRONTIER_PRICES_H
#define FRONTIER_PRICES_H

#include "compliant_path_graph.h"
#include "effective_prices.h"

#include <vector>

class FrontierPriceStorage;

class FrontierPrices : public EffectivePrices {
    friend class EffectivePrices; // to get init state cpg
    
    static FrontierPriceStorage *cpg_storage;
    
    static std::unique_ptr<CompliantPathGraph> get_init_state_cpg();
    
    
    FrontierPrices(const FrontierPrices *tag);
    
    
    void compute_cost_frontier();
    void compute_cost_frontier(LeafFactorID factor);
    
    void goal_price_propagation();

    void propagate_dominance_prices();
    // TODO get rid of this function
    const std::vector<boost::dynamic_bitset<> > & get_frontier();
    
protected:  
    
    std::vector<boost::dynamic_bitset<> > frontier;
    
    // returns the new duplicate counter
    virtual std::unique_ptr<CompliantPathGraph> get_successor_via_center_action(const State &predecessor,
                                                                                const Operator *op) const override;
                                                                                
    virtual void update(const State &base_state) override;
                                                                          
    virtual DOMINATION check_dominance(const State &other, DOMINATION needed) override;
    
    virtual void store_new_graph(const State &state) const override;
    
public:
    
    FrontierPrices(){};
    
    FrontierPrices(const FrontierPrices &) : EffectivePrices() {}; // HACK
    
    virtual void dump() const override;

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;

    
    static void register_cpg_storage(FrontierPriceStorage *storage);
    
};



#endif
