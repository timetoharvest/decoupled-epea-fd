#ifndef CPG_STORAGE
#define CPG_STORAGE

#include "../per_state_information.h"
#include "pricing_function.h"
#include "pruning_prices.h"
#include "pruning_reachable.h"
#include "reachability_function.h"


class CPGStorage {
    
protected:
    
    virtual void register_at_compliant_path_graph() = 0;

public:
    
    static CPGStorage *storage;
    
    static void initialize();

    virtual const CompliantPathGraph* get_cpg(const State &state) const = 0;
    
    virtual Prunable* get_prunable(const State &state) = 0;

};

class PriceStorage : public CPGStorage {
    friend class PathPriceTag;
    friend class Prices;

    PerStateInformation<Prices> prices;
    
    void store_cpg(const State &state, const Prices &cpg) {
        prices[state] = cpg;
    }
    
protected:
    
    virtual void register_at_compliant_path_graph();
    
public:
    
    virtual const CompliantPathGraph* get_cpg(const State &state) const {
        return &prices[state];
    }
    
    virtual Prunable* get_prunable(const State &state) {
        return &prices[state];
    }
    
};

class PruningPriceStorage : public CPGStorage {
    friend class PruningPrices;

    PerStateInformation<PruningPrices> pruning_prices;
    
    void store_cpg(const State &state, const PruningPrices &cpg) {
        pruning_prices[state] = cpg;
    }
    
protected:
    
    virtual void register_at_compliant_path_graph();
    
public:
    
    virtual const CompliantPathGraph* get_cpg(const State &state) const {
        return &pruning_prices[state];
    }
    
    virtual Prunable* get_prunable(const State &state) {
        return &pruning_prices[state];
    }
    
};

class ReachableStorage : public CPGStorage {
    friend class Reachable;

    PerStateInformation<Reachable> reachable;
    
    void store_cpg(const State &state, const Reachable &cpg) {
        reachable[state] = cpg;
    }
        
protected:
    
    virtual void register_at_compliant_path_graph();
    
public:
    
    virtual const CompliantPathGraph* get_cpg(const State &state) const {
        return &reachable[state];
    }
    
    virtual Prunable* get_prunable(const State &state) {
        return &reachable[state];
    }
    
};

class PruningReachableStorage : public CPGStorage {
    friend class PruningReachable;

    PerStateInformation<PruningReachable> pruning_reachable;
    
    void store_cpg(const State &state, const PruningReachable &cpg) {
        pruning_reachable[state] = cpg;
    }
        
protected:
    
    virtual void register_at_compliant_path_graph();
    
public:
    
    virtual const CompliantPathGraph* get_cpg(const State &state) const {
        return &pruning_reachable[state];
    }
    
    virtual Prunable* get_prunable(const State &state) {
        return &pruning_reachable[state];
    }
    
};

#endif