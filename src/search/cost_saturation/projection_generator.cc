#include "projection_generator.h"

#include "projection.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../pdbs/dominance_pruning.h"
#include "../pdbs/pattern_database.h"
#include "../pdbs/pattern_generator.h"
#include "../task_utils/task_properties.h"
#include "../utils/logging.h"

#include <memory>

using namespace pdbs;
using namespace std;

namespace cost_saturation {
ProjectionGenerator::ProjectionGenerator(const options::Options &opts)
    : pattern_generator(
          opts.get<shared_ptr<pdbs::PatternCollectionGenerator>>("patterns")),
      dominance_pruning(opts.get<bool>("dominance_pruning")),
      combine_labels(opts.get<bool>("combine_labels")),
      debug(opts.get<bool>("debug")) {
}

Abstractions ProjectionGenerator::generate_abstractions(
    const shared_ptr<AbstractTask> &task) {
    utils::Timer patterns_timer;
    utils::Log log;
    TaskProxy task_proxy(*task);

    task_properties::verify_no_axioms(task_proxy);

    log << "Compute patterns" << endl;
    PatternCollectionInformation pattern_collection_info =
        pattern_generator->generate(task);
    shared_ptr<pdbs::PatternCollection> patterns =
        pattern_collection_info.get_patterns();

    int max_pattern_size = 0;
    for (const pdbs::Pattern &pattern : *patterns) {
        max_pattern_size = max(max_pattern_size, static_cast<int>(pattern.size()));
    }

    log << "Number of patterns: " << patterns->size() << endl;
    log << "Maximum pattern size: " << max_pattern_size << endl;
    log << "Time for computing patterns: " << patterns_timer << endl;

    if (dominance_pruning) {
        shared_ptr<PDBCollection> pdbs = pattern_collection_info.get_pdbs();
        shared_ptr<vector<PatternClique>> pattern_cliques =
            pattern_collection_info.get_pattern_cliques();
        prune_dominated_cliques(
            *patterns,
            *pdbs, *pattern_cliques, task_proxy.get_variables().size(),
            numeric_limits<double>::infinity());
    }

    log << "Build projections" << endl;
    utils::Timer pdbs_timer;
    shared_ptr<TaskInfo> task_info = make_shared<TaskInfo>(task_proxy);
    Abstractions abstractions;
    for (const pdbs::Pattern &pattern : *patterns) {
        unique_ptr<Abstraction> projection = utils::make_unique_ptr<Projection>(
            task_proxy, task_info, pattern, combine_labels);

        if (debug) {
            log << "Pattern " << abstractions.size() + 1 << ": "
                << pattern << endl;
            projection->dump();
        }
        abstractions.push_back(move(projection));
    }

    int collection_size = 0;
    for (auto &abstraction : abstractions) {
        collection_size += abstraction->get_num_states();
    }

    log << "Time for building projections: " << pdbs_timer << endl;
    log << "Number of projections: " << abstractions.size() << endl;
    log << "Number of states in projections: " << collection_size << endl;
    return abstractions;
}

void add_projection_options_to_parser(OptionParser &parser) {
    parser.add_option<shared_ptr<pdbs::PatternCollectionGenerator>>(
        "patterns",
        "pattern generation method",
        OptionParser::NONE);
    parser.add_option<bool>(
        "dominance_pruning",
        "prune dominated patterns",
        "false");
    parser.add_option<bool>(
        "combine_labels",
        "group labels that only induce parallel transitions",
        "true");
    parser.add_option<bool>(
        "debug",
        "print debugging info",
        "false");
}

static shared_ptr<AbstractionGenerator> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Projection generator",
        "");

    add_projection_options_to_parser(parser);

    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;

    return make_shared<ProjectionGenerator>(opts);
}

static Plugin<AbstractionGenerator> _plugin("projections", _parse);
}
