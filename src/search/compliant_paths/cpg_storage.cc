#include "cpg_storage.h"


CPGStorage* CPGStorage::storage;

void CPGStorage::initialize() {
    if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
        if (CompliantPathGraph::get_pruning_options().do_advanced_pruning()){
            storage = new PruningReachableStorage();
        } else {
            storage = new ReachableStorage();
        }
    } else {
        if (CompliantPathGraph::get_pruning_options().do_advanced_pruning()){
            if (CompliantPathGraph::get_pruning_options().use_frontier_pruning()){
                storage = new FrontierPriceStorage();
            } else {
                storage = new EffectivePriceStorage();
            }
        } else {
            storage = new PriceStorage();
        }
    }
    storage->register_at_compliant_path_graph();
}

void PriceStorage::register_at_compliant_path_graph() {
    Prices::register_cpg_storage(this);
}

uint64_t PriceStorage::get_decoupled_state_hash(const State &state) {
    if (hashes[state] == 0){
        hashes[state] = prices[state].get_hash();
    }
    return hashes[state];
}

void EffectivePriceStorage::register_at_compliant_path_graph() {
    EffectivePrices::register_cpg_storage(this);
}

uint64_t EffectivePriceStorage::get_decoupled_state_hash(const State &state) {
    if (hashes[state] == 0){
        hashes[state] = effective_prices[state].get_hash();
    }
    return hashes[state];
}

void FrontierPriceStorage::register_at_compliant_path_graph() {
    FrontierPrices::register_cpg_storage(this);
}

uint64_t FrontierPriceStorage::get_decoupled_state_hash(const State &state) {
    if (hashes[state] == 0){
        hashes[state] = frontier_prices[state].get_hash();
    }
    return hashes[state];
}

void ReachableStorage::register_at_compliant_path_graph() {
    Reachable::register_cpg_storage(this);
}

uint64_t ReachableStorage::get_decoupled_state_hash(const State &state) {
    if (hashes[state] == 0){
        hashes[state] = reachable[state].get_hash();
    }
    return hashes[state];
}

void PruningReachableStorage::register_at_compliant_path_graph() {
    PruningReachable::register_cpg_storage(this);
}

uint64_t PruningReachableStorage::get_decoupled_state_hash(const State &state) {
    if (hashes[state] == 0){
        hashes[state] = pruning_reachable[state].get_hash();
    }
    return hashes[state];
}
