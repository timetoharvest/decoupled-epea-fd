#include "canonical_pdbs_heuristic.h"

#include "decoupled_canonical_pdbs_approximate.h"
#include "decoupled_canonical_pdbs_exponential.h"
#include "decoupled_canonical_pdbs_single_leaf.h"
#include "dominance_pruning.h"
#include "pattern_filter.h"
#include "pattern_generator.h"
#include "utils.h"

#include "../globals.h"
#include "../leaf_state_id.h"
#include "../option_parser.h"
#include "../plugin.h"

#include "../tasks/root_task.h"

#include "../utils/logging.h"
#include "../utils/timer.h"

#include <iostream>
#include <limits>
#include <memory>

using namespace std;

namespace pdbs {

enum class DecoupledPatternCollectionLookUp {APPROX, SINGLE_LEAF, EXP};

unique_ptr<CanonicalPDBs> get_canonical_pdbs_from_options(
    const shared_ptr<AbstractTask> &task, const Options &opts) {
    shared_ptr<PatternCollectionGenerator> pattern_generator =
        opts.get<shared_ptr<PatternCollectionGenerator>>("patterns");
    utils::Timer timer;
    utils::g_log << "Initializing canonical PDB heuristic..." << endl;
    DecoupledPatternCollectionLookUp dec_lookup_type(opts.get<DecoupledPatternCollectionLookUp>("dec_lookup_type"));
    PatternCollectionInformation pattern_collection_info =
        pattern_generator->generate(task);
    shared_ptr<PatternCollection> patterns =
        pattern_collection_info.get_patterns();

    if (g_factoring && dec_lookup_type == DecoupledPatternCollectionLookUp::SINGLE_LEAF){
        // verify that every pattern affects at most one leaf factor
        // we want to check this here and not in the constructor of DecoupledCanonicalPDBsSingleLeaf
        // because non-complying patterns might be filtered by dominance pruning beforehand
        for (const Pattern &pattern : *patterns){
            unordered_set<LeafFactorID> affected_leaves = get_leaf_factors_of_pattern(pattern);
            if (affected_leaves.size() > 1){
                cerr << "Every pattern may affect at most one leaf for SINGLE_LEAF decoupled lookup." << endl;
                utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
            }
        }
    }

    /*
      We compute PDBs and pattern cliques here (if they have not been
      computed before) so that their computation is not taken into account
      for dominance pruning time.
    */
    shared_ptr<PDBCollection> pdbs = pattern_collection_info.get_pdbs();

    shared_ptr<vector<PatternClique>> pattern_cliques;
    if (g_factoring && dec_lookup_type == DecoupledPatternCollectionLookUp::APPROX){
        // adapt compatibility graph so that no two patterns can be in the same clique
        // if they affect the same leaf factor
        pattern_cliques = pattern_collection_info.get_pattern_cliques(true);
    } else {
        pattern_cliques = pattern_collection_info.get_pattern_cliques();
    }

    double max_time_dominance_pruning = opts.get<double>("max_time_dominance_pruning");
    if (max_time_dominance_pruning > 0.0) {
        int num_variables = TaskProxy(*task).get_variables().size();
        /*
          NOTE: Dominance pruning could also be computed without having access
          to the PDBs, but since we want to delete patterns, we also want to
          update the list of corresponding PDBs so they are synchronized.

          In the long term, we plan to have patterns and their PDBs live
          together, in which case we would only need to pass their container
          and the pattern cliques.
        */
        prune_dominated_cliques(
            *patterns,
            *pdbs,
            *pattern_cliques,
            num_variables,
            max_time_dominance_pruning);
    }

    // Do not dump pattern collections for size reasons.
    dump_pattern_collection_generation_statistics(
        "Canonical PDB heuristic", timer(), pattern_collection_info);

    if (g_factoring){
        switch (dec_lookup_type){
            case DecoupledPatternCollectionLookUp::APPROX: return utils::make_unique_ptr<DecoupledCanonicalPDBsApproximate>(pdbs, pattern_cliques);
            case DecoupledPatternCollectionLookUp::EXP: return utils::make_unique_ptr<DecoupledCanonicalPDBsExponential>(pdbs, pattern_cliques);
        case DecoupledPatternCollectionLookUp::SINGLE_LEAF: return utils::make_unique_ptr<DecoupledCanonicalPDBsSingleLeaf>(pdbs, pattern_cliques);
        default: cerr << "Unknown dec_lookup_type, options are APPROX, EXP, or SINGLE_LEAF." << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
        }
    }

    return utils::make_unique_ptr<CanonicalPDBs>(pdbs, pattern_cliques);
}

CanonicalPDBsHeuristic::CanonicalPDBsHeuristic(const Options &opts)
    : Heuristic(opts),
      canonical_pdbs(get_canonical_pdbs_from_options(tasks::g_root_task, opts)) {
}

int CanonicalPDBsHeuristic::compute_heuristic(const GlobalState &ancestor_state) {
    int h = canonical_pdbs->get_value(ancestor_state);
    if (h == numeric_limits<int>::max()) {
        return DEAD_END;
    } else {
        return h;
    }
}

void add_decoupled_lookup_types_option_to_parser(options::OptionParser &parser) {
    vector<string> lookup_types;
    vector<string> lookup_descriptions;
    lookup_types.push_back("APPROX");
    lookup_descriptions.push_back("Sum over the minimum leaf state per leaf."
                                  "Ensures that no two patterns in a clique affect the same leaf"
                                  "by adapting the compatibility graph.");
    lookup_types.push_back("SINGLE_LEAF");
    lookup_descriptions.push_back("An efficient lookup that only supports patterns"
                                  "that affect a single leaf.");
    lookup_types.push_back("EXP");
    lookup_descriptions.push_back("Minimizes the heuristic value over all"
                                  "member states of a decoupled state.");
    parser.add_enum_option<DecoupledPatternCollectionLookUp>(
        "dec_lookup_type",
        lookup_types,
        "Specifies the way the heuristic is computed for decoupled states.",
        "APPROX",
        lookup_descriptions);
}

void add_canonical_pdbs_options_to_parser(options::OptionParser &parser) {
    parser.add_option<double>(
        "max_time_dominance_pruning",
        "The maximum time in seconds spent on dominance pruning. Using 0.0 "
        "turns off dominance pruning. Dominance pruning excludes patterns "
        "and additive subsets that will never contribute to the heuristic "
        "value because there are dominating subsets in the collection.",
        "infinity",
        Bounds("0.0", "infinity"));
}

static shared_ptr<Heuristic> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Canonical PDB",
        "The canonical pattern database heuristic is calculated as follows. "
        "For a given pattern collection C, the value of the "
        "canonical heuristic function is the maximum over all "
        "maximal additive subsets A in C, where the value for one subset "
        "S in A is the sum of the heuristic values for all patterns in S "
        "for a given state.");
    parser.document_language_support("action costs", "supported");
    parser.document_language_support("conditional effects", "not supported");
    parser.document_language_support("axioms", "not supported");
    parser.document_property("admissible", "yes");
    parser.document_property("consistent", "yes");
    parser.document_property("safe", "yes");
    parser.document_property("preferred operators", "no");

    parser.add_option<shared_ptr<PatternCollectionGenerator>>(
        "patterns",
        "pattern generation method",
        "systematic(1)");

    add_canonical_pdbs_options_to_parser(parser);
    add_decoupled_lookup_types_option_to_parser(parser);

    Heuristic::add_options_to_parser(parser);

    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;

    return make_shared<CanonicalPDBsHeuristic>(opts);
}

static Plugin<Evaluator> _plugin("cpdbs", _parse, "heuristics_pdb");
}
