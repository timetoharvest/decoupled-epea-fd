#include "cpg_storage.h"


CPGStorage* CPGStorage::storage;

void CPGStorage::initialize() {
    if (g_factoring->get_search_type() == SAT || g_factoring->get_search_type() == UNSAT){
        if (CompliantPathGraph::get_pruning().do_advanced_pruning()){
            storage = new PruningReachableStorage();
        } else {
            storage = new ReachableStorage();
        }
    } else {
        if (CompliantPathGraph::get_pruning().do_advanced_pruning()){
            storage = new PruningPriceStorage();
        } else {
            storage = new PriceStorage();
        }
    }
    storage->register_at_compliant_path_graph();
}

void PriceStorage::register_at_compliant_path_graph() {
    Prices::register_cpg_storage(this);
}

void PruningPriceStorage::register_at_compliant_path_graph() {
    PruningPrices::register_cpg_storage(this);
}

void ReachableStorage::register_at_compliant_path_graph() {
    Reachable::register_cpg_storage(this);
}

void PruningReachableStorage::register_at_compliant_path_graph() {
    PruningReachable::register_cpg_storage(this);
}
