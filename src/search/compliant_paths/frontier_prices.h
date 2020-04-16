#ifndef FRONTIER_PRICES_H
#define FRONTIER_PRICES_H

#include "../ext/boost/dynamic_bitset.hpp"
#include "compliant_path_graph.h"
#include "effective_prices.h"

#include <vector>

class FrontierPriceStorage;

class FrontierPrices : public EffectivePrices {
    friend class EffectivePrices; // to get init state cpg
    
    static FrontierPriceStorage *cpg_storage;
    

    void resize();
    
    void compute_cost_frontier();

    void compute_cost_frontier(LeafFactorID factor);
    
    void goal_price_propagation();

    void propagate_dominance_prices();
    
    void update(const State &new_center_state);


    static std::unique_ptr<CompliantPathGraph> get_init_state_cpg();

protected:  
    
    std::vector<boost::dynamic_bitset<> > frontier;
    
    virtual std::unique_ptr<CompliantPathGraph> get_successor_via_center_action(const State &new_center_state,
                                                                                const Operator &op) const override;
                                                                          
    virtual DOMINANCE check_dominance(const State &other, uint64_t hash, int g_advantage, DOMINANCE needed) override;
    
    virtual void store_new_cpg(const State &state) override;
    
public:
    
    FrontierPrices(){
        assert(g_factoring->get_profile() == FACTORING_PROFILE::FORK);
    };

    FrontierPrices(const FrontierPrices &cpg);

    FrontierPrices(FrontierPrices &&other) = delete;

    FrontierPrices& operator=(const FrontierPrices &other) = delete;

    FrontierPrices& operator=(FrontierPrices &&other);
    
    virtual void dump() const override;

    virtual std::unique_ptr<CompliantPathGraph> clone() const override;

    virtual uint64_t get_hash() const override;

    
    static void register_cpg_storage(FrontierPriceStorage *storage);
    
};



#endif
