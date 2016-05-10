#include "cpg_storage.h"


using namespace std;


PerStateInformation<Prices> CPGStorage::price_tags;
PerStateInformation<PruningPrices> CPGStorage::pruning_prices;
PerStateInformation<PruningReachable> CPGStorage::pruning_reachable;
PerStateInformation<Reachable> CPGStorage::reach_tags;
