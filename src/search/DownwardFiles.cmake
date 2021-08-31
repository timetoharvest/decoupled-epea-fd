# See http://www.fast-downward.org/ForDevelopers/AddingSourceFiles
# for general information on adding source files and CMake plugins.
#
# All plugins are enabled by default and users can disable them by specifying
#    -DPLUGIN_FOO_ENABLED=FALSE
# The default behavior can be changed so all non-essential plugins are
# disabled by default by specifying
#    -DDISABLE_PLUGINS_BY_DEFAULT=TRUE
# In that case, individual plugins can be enabled with
#    -DPLUGIN_FOO_ENABLED=TRUE
#
# Defining a new plugin:
#    fast_downward_plugin(
#        NAME <NAME>
#        [ DISPLAY_NAME <DISPLAY_NAME> ]
#        [ HELP <HELP> ]
#        SOURCES
#            <FILE_1> [ <FILE_2> ... ]
#        [ DEPENDS <PLUGIN_NAME_1> [ <PLUGIN_NAME_2> ... ] ]
#        [ DEPENDENCY_ONLY ]
#        [ CORE_PLUGIN ]
#    )
#
# <DISPLAY_NAME> defaults to lower case <NAME> and is used to group files
#   in IDEs and for messages.
# <HELP> defaults to <DISPLAY_NAME> and is used to describe the cmake option.
# SOURCES lists the source files that are part of the plugin. Entries are
#   listed without extension. For an entry <file>, both <file>.h and <file>.cc
#   are added if the files exist.
# DEPENDS lists plugins that will be automatically enabled if this plugin is
#   enabled. If the dependency was not enabled before, this will be logged.
# DEPENDENCY_ONLY disables the plugin unless it is needed as a dependency and
#   hides the option to enable the plugin in cmake GUIs like ccmake.
# CORE_PLUGIN always enables the plugin (even if DISABLE_PLUGINS_BY_DEFAULT
#   is used) and hides the option to disable it in CMake GUIs like ccmake.

option(
    DISABLE_PLUGINS_BY_DEFAULT
    "If set to YES only plugins that are specifically enabled will be compiled"
    NO)
# This option should not show up in CMake GUIs like ccmake where all
# plugins are enabled or disabled manually.
mark_as_advanced(DISABLE_PLUGINS_BY_DEFAULT)

fast_downward_plugin(
    NAME CORE_SOURCES
    HELP "Core source files"
    SOURCES
        planner

        axioms
        combining_evaluator
        eager_search
        enforced_hill_climbing_search
        evaluator
        factoring
        g_evaluator
        globals
        heuristic
        ipc_max_heuristic
        iterated_search
        lazy_search
        max_evaluator
        operator
        operator_cost
        operator_factor_info
        operator_id
        option_parser
        option_parser_util
        per_state_information
        pref_evaluator
        pruning_method
        relaxation_heuristic
        search_engine
        search_node_info
        search_progress
        search_space
        state
        state_id
        state_registry
        sum_evaluator

        weighted_evaluator
          
        open_lists/open_list_buckets

        additive_heuristic
        blind_search_heuristic
        cea_heuristic
        cg_heuristic
        cg_cache
        ff_heuristic
        goal_count_heuristic
        hm_heuristic
        lm_cut_heuristic
        max_heuristic

    DEPENDS CAUSAL_GRAPH INT_PACKER SEGMENTED_VECTOR SUCCESSOR_GENERATOR
    CORE_PLUGIN
)

fast_downward_plugin(
    NAME UTILS
    HELP "System utilities"
    SOURCES
    	utils/array_pool
        utils/collections
        utils/countdown_timer
        utils/hash
        utils/language
        utils/logging
        utils/math
        utils/memory
        utils/rng
        utils/rng_options
        utils/system
        utils/system_unix
        utils/system_windows
        utils/timer
    CORE_PLUGIN
)
 
fast_downward_plugin(
    NAME ALTERNATION_OPEN_LIST
    HELP "Open list that alternates between underlying open lists in a round-robin manner"
    SOURCES
        open_lists/alternation_open_list
)
 
fast_downward_plugin(
    NAME PARETO_OPEN_LIST
    HELP "Pareto open list"
    SOURCES
        open_lists/pareto_open_list
)
 
fast_downward_plugin(
    NAME STANDARD_SCALAR_OPEN_LIST
    HELP "Standard scalar open list"
    SOURCES
        open_lists/standard_scalar_open_list
)
 
fast_downward_plugin(
    NAME TIEBREAKING_OPEN_LIST
    HELP "Tiebreaking open list"
    SOURCES
        open_lists/tiebreaking_open_list
)

fast_downward_plugin(
    NAME DYNAMIC_BITSET
    HELP "Poor man's version of boost::dynamic_bitset"
    SOURCES
        algorithms/dynamic_bitset
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME MAXIMUM_INDEPENDENT_SET
    HELP "Class that computes maximum independent sets"
    SOURCES
        algorithms/mis
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME EQUIVALENCE_RELATION
    HELP "Equivalence relation over [1, ..., n] that can be iteratively refined"
    SOURCES
        algorithms/equivalence_relation
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME INT_HASH_SET
    HELP "Hash set storing non-negative integers"
    SOURCES
        algorithms/int_hash_set
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME INT_PACKER
    HELP "Greedy bin packing algorithm to pack integer variables with small domains tightly into memory"
    SOURCES
        algorithms/int_packer
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME MAX_CLIQUES
    HELP "Implementation of the Max Cliques algorithm by Tomita et al."
    SOURCES
        algorithms/max_cliques
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME PRIORITY_QUEUES
    HELP "Three implementations of priority queue: HeapQueue, BucketQueue and AdaptiveQueue"
    SOURCES
        algorithms/priority_queues
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME ORDERED_SET
    HELP "Set of elements ordered by insertion time"
    SOURCES
        algorithms/ordered_set
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME SEGMENTED_VECTOR
    HELP "Memory-friendly and vector-like data structure"
    SOURCES
        algorithms/segmented_vector
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME DECOUPLED_SEARCH
    HELP "Classes relevant to run decoupled search"
    SOURCES
        leaf_state
        leaf_state_id

        compliant_paths/compliant_path_graph
        compliant_paths/cpg_storage
        compliant_paths/cudd_cpg
        compliant_paths/effective_prices
        compliant_paths/explicit_state_cpg
        compliant_paths/frontier_prices
        compliant_paths/path_price_tag
        compliant_paths/pricing_function
        compliant_paths/pruning_options
        compliant_paths/pruning_reachable
        compliant_paths/reachability_function
        compliant_paths/simulation_relation
        compliant_paths/symbolic_state_cpg
    DEPENDS SCCS CAUSAL_GRAPH SYMMETRIES SYMBOLIC_SEARCH
)

fast_downward_plugin(
    NAME SYMBOLIC_SEARCH
    HELP "Classes relevant to run symbolic search"
    SOURCES
        symbolic/breadth_first_search
        symbolic/closed_list
        symbolic/cudd_method
        symbolic/debug_macros
        symbolic/frontier
        symbolic/leaf_state_space
        symbolic/open_list
        symbolic/opt_order
        symbolic/solution_bound
        symbolic/sym_bucket
        symbolic/sym_controller
        symbolic/sym_decoupled_manager
        symbolic/sym_enums
        symbolic/sym_estimate
        symbolic/sym_params_search
        symbolic/sym_pricing_function_debug
        symbolic/sym_pricing_function
        symbolic/sym_pricing_function_sat
        symbolic/sym_search
        symbolic/sym_solution
        symbolic/sym_state_space_manager
        symbolic/sym_util
        symbolic/sym_variables
        symbolic/transition_relation
        symbolic/unidirectional_search
        symbolic/uniform_cost_search
    DEPENDS DECOUPLED_SEARCH
)

fast_downward_plugin(
    NAME SYMBOLIC_DECOUPLED_LMCUT
    HELP "A variant of the LM-cut heuristic for decoupled search with symbolic leaf representation"
    SOURCES
        lm_cut_heuristic_sym_dec
    DEPENDS DECOUPLED_SEARCH SYMBOLIC_SEARCH
)

fast_downward_plugin(
    NAME MIS_FACTORING
    HELP "Class relevant to use factoring strategies using an maximum indepedent sets"
    SOURCES
        factorings/factoring_ranking
        factorings/mis_factoring
    DEPENDS DECOUPLED_SEARCH MAXIMUM_INDEPENDENT_SET VARIABLE_INT
)

fast_downward_plugin(
    NAME LP_FACTORING
    HELP "Class relevant to use factoring strategies using an LP solver"
    SOURCES
        factorings/lp_factoring
        factorings/lp_general_factoring
        factorings/lp_no_center_factoring
    DEPENDS DECOUPLED_SEARCH LP_SOLVER
)

fast_downward_plugin(
    NAME SCHEMA_IA_FACTORING
    HELP "Class relevant to use factoring strategies without center factor"
    SOURCES
        factorings/schema_ia_factoring
    DEPENDS DECOUPLED_SEARCH
)

fast_downward_plugin(
    NAME VARIABLE_INT
    HELP "Class representing an integer with variable memory footprint"
    SOURCES
        utils/variable_int
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME SYMMETRIES
    HELP "Classes relevant to use symmetry breaking"
    SOURCES
        symmetries/decoupled_group
        symmetries/decoupled_permutation
        symmetries/graph_creator
        symmetries/group
        symmetries/lexicographic_ordering
        symmetries/permutation
        symmetries/symmetry_cpg
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME STUBBORN_SETS
    HELP "Base class for all stubborn set partial order reduction methods"
    SOURCES
        pruning/stubborn_sets
    DEPENDENCY_ONLY
)
 
fast_downward_plugin(
    NAME STUBBORN_SETS_SIMPLE
    HELP "Stubborn sets simple"
    SOURCES
        pruning/stubborn_sets_simple
    DEPENDS STUBBORN_SETS
)

fast_downward_plugin(
    NAME STUBBORN_SETS_EC
    HELP "Stubborn set method that dominates expansion core"
    SOURCES
        pruning/stubborn_sets_ec
    DEPENDS STUBBORN_SETS
)

 fast_downward_plugin(
    NAME STUBBORN_SETS_DECOUPLED
    HELP "Stubborn set implementation for decoupled search"
    SOURCES
        pruning/stubborn_sets_decoupled
    DEPENDS STUBBORN_SETS_SIMPLE DECOUPLED_SEARCH
)

fast_downward_plugin(
    NAME LP_SOLVER
    HELP "Interface to an LP solver"
    SOURCES
        lp/cplex_solver
        lp/lp_internals
        lp/lp_solver
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME DOMAIN_TRANSITION_GRAPH
    HELP "DTGs"
    SOURCES
        task_utils/domain_transition_graph
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME CAUSAL_GRAPH
    HELP "Causal Graph"
    SOURCES
        task_utils/causal_graph
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME SUCCESSOR_GENERATOR
    HELP "Successor generator"
    SOURCES
        task_utils/successor_generator
        task_utils/successor_generator_factory
        task_utils/successor_generator_internals
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME VARIABLE_ORDER_FINDER
    HELP "Variable order finder"
    SOURCES
        task_utils/variable_order_finder
    DEPENDENCY_ONLY
)

fast_downward_plugin(
    NAME MAS_HEURISTIC
    HELP "The Merge-and-Shrink heuristic"
    SOURCES
        merge_and_shrink/label
        merge_and_shrink/labels
        merge_and_shrink/label_reducer
        merge_and_shrink/merge_and_shrink_heuristic
        merge_and_shrink/merge_dfp
        merge_and_shrink/merge_linear
        merge_and_shrink/merge_strategy
        merge_and_shrink/shrink_bisimulation
        merge_and_shrink/shrink_bucket_based
        merge_and_shrink/shrink_fh
        merge_and_shrink/shrink_random
        merge_and_shrink/shrink_strategy
        merge_and_shrink/transition_system
    DEPENDS PRIORITY_QUEUES EQUIVALENCE_RELATION SCCS VARIABLE_ORDER_FINDER
)
 
fast_downward_plugin(
    NAME LANDMARKS
    HELP "Plugin containing the code to reason with landmarks"
    SOURCES
        landmarks/exploration
        landmarks/h_m_landmarks
        landmarks/lama_ff_synergy
        landmarks/landmark_cost_assignment
        landmarks/landmark_count_heuristic
        landmarks/landmark_status_manager
        landmarks/landmark_graph_merged
        landmarks/landmark_graph
        landmarks/landmark_factory
        landmarks/landmark_factory_rpg_exhaust
        landmarks/landmark_factory_rpg_sasp
        landmarks/landmark_factory_zhu_givan
        landmarks/util
    DEPENDS PRIORITY_QUEUES SUCCESSOR_GENERATOR DOMAIN_TRANSITION_GRAPH LP_SOLVER
)
 
fast_downward_plugin(
    NAME PDBS
    HELP "Plugin containing the code for PDBs"
    SOURCES
        pdbs/canonical_pdbs_heuristic
        pdbs/dominance_pruner
        pdbs/match_tree
        pdbs/max_cliques
        pdbs/pattern_generation_edelkamp
        pdbs/pattern_generation_haslum
        pdbs/pdb_heuristic
        pdbs/util
        pdbs/zero_one_pdbs_heuristic
    DEPENDS CAUSAL_GRAPH PRIORITY_QUEUES SUCCESSOR_GENERATOR VARIABLE_ORDER_FINDER
)

fast_downward_plugin(
    NAME SCCS
    HELP "Algorithm to compute the strongly connected components (SCCs) of a "
         "directed graph."
    SOURCES
        algorithms/sccs
    DEPENDENCY_ONLY
)

fast_downward_add_plugin_sources(PLANNER_SOURCES)

# The order in PLANNER_SOURCES influences the order in which object
# files are given to the linker, which can have a significant influence
# on performance (see issue67). The general recommendation seems to be
# to list files that define functions after files that use them.
# We approximate this by reversing the list, which will put the plugins
# first, followed by the core files, followed by the main file.
# This is certainly not optimal, but works well enough in practice.
list(REVERSE PLANNER_SOURCES)
