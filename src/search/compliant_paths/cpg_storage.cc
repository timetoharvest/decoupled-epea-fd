#include "cpg_storage.h"

#include "effective_prices.h"
#include "frontier_prices.h"
#include "pricing_function.h"
#include "pruning_options.h"
#include "pruning_reachable.h"
#include "reachability_function.h"

CPGStorage* CPGStorage::storage;

void CPGStorage::initialize(const PruningOptions &pruning_options) {
    if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
        if (pruning_options.do_advanced_pruning()){
            storage = new Storage<PruningReachable>();
        } else {
            storage = new Storage<Reachable>();
        }
    } else {
        if (pruning_options.do_advanced_pruning()){
            if (pruning_options.use_frontier_pruning()){
                storage = new Storage<FrontierPrices>();
            } else {
                storage = new Storage<EffectivePrices>();
            }
        } else {
            storage = new Storage<Prices>();
        }
    }
    storage->register_at_compliant_path_graph();
}

template<class T>
void Storage<T>::register_at_compliant_path_graph() {
    T::register_cpg_storage(this);
}

template<class T>
uint64_t Storage<T>::get_decoupled_state_hash(const State &state) {
    if (hashes[state] == 0){
        hashes[state] = cpgs[state].get_hash();
    }
    return hashes[state];
}
