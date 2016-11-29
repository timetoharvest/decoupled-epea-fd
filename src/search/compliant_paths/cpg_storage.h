#ifndef CPG_STORAGE
#define CPG_STORAGE

#include "../per_state_information.h"
#include "pricing_function.h"
#include "pruning_prices.h"
#include "pruning_reachable.h"
#include "reachability_function.h"


class CPGStorage {
    friend class StateRegistry; // to get non-const cpg
    friend class Prunable; // to get non-const cpg
    
protected:
    
    virtual void register_at_compliant_path_graph() = 0;
    
    virtual CompliantPathGraph* get_mutable_cpg(const State &state) = 0;

public:
    
    static CPGStorage *storage;
    
    static void initialize();

    virtual const CompliantPathGraph* get_cpg(const State &state) const = 0;
    
    virtual size_t size(const StateRegistry *registry) const = 0;
    
};

class PriceStorage : public CPGStorage {
    friend class PathPriceTag;
    friend class Prices;

    PerStateInformation<Prices> prices;
    
    void store_cpg(const State &state, const Prices &cpg) {
        prices[state] = cpg;
    }
    
protected:
    
    virtual void register_at_compliant_path_graph() override;
    
    virtual CompliantPathGraph* get_mutable_cpg(const State &state) override {
        return &prices[state];
    }
    
public:
    
    virtual const CompliantPathGraph* get_cpg(const State &state) const override {
        return &prices[state];
    }
    
    virtual size_t size(const StateRegistry *registry) const override {
        return prices.size(registry);
    }
    
};

class PruningPriceStorage : public CPGStorage {
    friend class PruningPrices;

    PerStateInformation<PruningPrices> pruning_prices;
    
    void store_cpg(const State &state, const PruningPrices &cpg) {
        pruning_prices[state] = cpg;
    }
    
protected:
    
    virtual void register_at_compliant_path_graph() override;
    
    virtual CompliantPathGraph* get_mutable_cpg(const State &state) override {
        return &pruning_prices[state];
    }
    
public:
    
    virtual const CompliantPathGraph* get_cpg(const State &state) const override {
        return &pruning_prices[state];
    }
    
    virtual size_t size(const StateRegistry *registry) const override {
        return pruning_prices.size(registry);
    }
    
};

class ReachableStorage : public CPGStorage {
    friend class Reachable;

    PerStateInformation<Reachable> reachable;
    
    void store_cpg(const State &state, const Reachable &cpg) {
        reachable[state] = cpg;
    }
        
protected:
    
    virtual void register_at_compliant_path_graph() override;
    
    virtual CompliantPathGraph* get_mutable_cpg(const State &state) override {
        return &reachable[state];
    }
    
public:
    
    virtual const CompliantPathGraph* get_cpg(const State &state) const override {
        return &reachable[state];
    }
    
    virtual size_t size(const StateRegistry *registry) const override {
        return reachable.size(registry);
    }
    
};

class PruningReachableStorage : public CPGStorage {
    friend class PruningReachable;

    PerStateInformation<PruningReachable> pruning_reachable;
    
    void store_cpg(const State &state, const PruningReachable &cpg) {
        pruning_reachable[state] = cpg;
    }
        
protected:
    
    virtual void register_at_compliant_path_graph() override;
    
    virtual CompliantPathGraph* get_mutable_cpg(const State &state) override {
        return &pruning_reachable[state];
    }
    
public:
    
    virtual const CompliantPathGraph* get_cpg(const State &state) const override {
        return &pruning_reachable[state];
    }
    
    virtual size_t size(const StateRegistry *registry) const override {
        return pruning_reachable.size(registry);
    }
    
};

#endif
