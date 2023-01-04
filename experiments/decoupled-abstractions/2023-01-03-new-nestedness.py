#! /usr/bin/env python3

import itertools
import math
import os
from pathlib import Path
import subprocess

from lab.environments import LocalEnvironment, TetralithEnvironment
from lab.reports import Attribute, geometric_mean, arithmetic_mean

from downward.reports.absolute import AbsoluteReport
from downward.reports.compare import ComparativeReport
from downward.reports.scatter import ScatterPlotReport

import common_setup
from common_setup import IssueConfig, IssueExperiment

DIR = os.path.dirname(os.path.abspath(__file__))
SCRIPT_NAME = os.path.splitext(os.path.basename(__file__))[0]
BENCHMARKS_DIR = os.environ["AUTOSCALE_BENCHMARKS_OPT"]
REVISION = "9cb4f5bde63a161fe9e1200ce74332b6e81dc5e8"
BASE_REVISION = "17a0e29662a85a06d24a4a73b370ab973c6ed3a4"
REVISIONS = [REVISION]
TIME_LIMIT=900

CONFIGS = []
decoupling = {
    #'fork' : 'fork()',
    #'ifork' : 'ifork()',
    'lpmm' : 'lp_general(factoring_time_limit=30, memory_limit=4000, add_cg_sccs=true, strategy=mm_approx, min_flexibility=0.8)',
}
merge_strategies = {
    'dfp' : 'merge_stateless(merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',

    'sccdfp' : 'merge_clustering(clustering_factory=clustering_cg_sccs(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',

    'sbmiasm': 'merge_stateless(merge_selector=score_based_filtering(scoring_functions=[sf_miasm(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,threshold_before_merge=1),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',

    'sccsbmiasm': 'merge_clustering(clustering_factory=clustering_cg_sccs(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[sf_miasm(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,threshold_before_merge=1),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',


    'compliantdfp' : 'merge_clustering(clustering_factory=clustering_compliant_factoring(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',

    'compliantsbmiasm': 'merge_clustering(clustering_factory=clustering_compliant_factoring(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[sf_miasm(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,threshold_before_merge=1),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',

    'linearcggl' : 'merge_strategy=merge_precomputed(merge_tree=linear(variable_order=cg_goal_level))',
}

for strat_name, merge_strategy in merge_strategies.items():
    for dec_name, dec in decoupling.items():
        CONFIGS.append(IssueConfig(f'{dec_name}-b50k-{strat_name}-exact', ['--decoupling', dec, '--search',  f'astar(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy={merge_strategy},label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,main_loop_max_time={TIME_LIMIT},decoupled_lookup=exact),check_consistency=true)'], build_options=['release64'], driver_options=['--build', 'release64']))


SUITE = sorted(["agricola", "blocksworld", "depots", "floortile", "grid", "logistics", "nomystery", "parcprinter", "pegsol", "rovers", "snake", "termes", "tidybot", "visitall",
         "airport", "childsnack", "driverlog", "freecell", "gripper", "miconic", "openstacks", "parking", "pipesworld-notankage", "satellite", "sokoban", "tetris", "tpp", "woodworking",
         "barman", "data-network", "elevators", "ged", "hiking", "mprime", "organic-synthesis-split", "pathways", "pipesworld-tankage", "scanalyzer", "storage", "thoughtful", "transport", "zenotravel"])

ENVIRONMENT = TetralithEnvironment(
    email="daniel.gnad@liu.se",
#    time_limit_per_task="24:00:00",
#    memory_per_cpu=None,
    extra_options="#SBATCH -A snic2022-22-1038"
)

exp = IssueExperiment(
    revisions=REVISIONS,
    configs=CONFIGS,
    environment=ENVIRONMENT,
)

exp.add_suite(BENCHMARKS_DIR, SUITE)

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(exp.PLANNER_PARSER)
exp.add_parser('decoupled-parser.py')
exp.add_parser('ms-parser.py')
exp.add_parser('consistency-parser.py')

exp.add_step('build', exp.build)
exp.add_step('start', exp.start_runs)
exp.add_parse_again_step()
exp.add_fetcher(name='fetch')

attributes=[
    # m&s attributes
    Attribute('search_out_of_memory', absolute=True, min_wins=True),
    Attribute('search_out_of_time', absolute=True, min_wins=True),
    Attribute('ms_construction_time', absolute=False, min_wins=True, function=geometric_mean),
    Attribute('score_ms_construction_time', min_wins=False, digits=4),
    Attribute('ms_atomic_construction_time', absolute=False, min_wins=True, function=geometric_mean),
    Attribute('ms_abstraction_constructed', absolute=True, min_wins=False),
    Attribute('ms_atomic_fts_constructed', absolute=True, min_wins=False),
    Attribute('ms_out_of_memory', absolute=True, min_wins=True),
    Attribute('ms_out_of_time', absolute=True, min_wins=True),
    Attribute('ms_memory_delta', absolute=False, min_wins=True),
    Attribute('ms_reached_time_limit', absolute=False, min_wins=True),
    Attribute('ms_max_num_nested_leaves', absolute=True, function=sum),
    Attribute('ms_strongly_compliant', absolute=True, function=sum),

    # decoupled attributes
    Attribute('average_member_states', absolute=False, min_wins=True, function=arithmetic_mean),
    Attribute("number_leaf_factors", absolute=False),

    # general search attributes
    Attribute('reopened_until_last_jump', absolute=False, min_wins=True, function=arithmetic_mean),
    Attribute('inconsistent_heuristic', absolute=True, min_wins=True, function=sum),
]

attributes.extend(exp.DEFAULT_TABLE_ATTRIBUTES)

exp.add_absolute_report_step(attributes=attributes)

exp.add_fetcher("data/2022-12-13-test-exact-caching-lookup-eval/", filter_algorithm=[
          f"{BASE_REVISION}-lpmm-b50k-sbmiasm-exactnocache",
          f"{BASE_REVISION}-lpmm-b50k-compliantsbmiasm-exactnocache",
          f"{BASE_REVISION}-lpmm-b50k-linearcggl-exactnocache",
])

 
class Filter:
    def __init__(self, dec, algos):
        self.decoupling = dec
        self.algos = algos
        self.excluded_tasks = set()

    def collect_non_factorable_tasks(self, run):
        # Only consider algorithms relevant to the table.
        if run['algorithm'] not in self.algos:
            return False
        error = run.get('error')
        if error is not None and error == 'decoupled-search-abstained':
            self.excluded_tasks.add(f"{run['domain']}:{run['problem']}")
        return True

    def filter_interesting_tasks(self, run):
        return f"{run['domain']}:{run['problem']}" not in self.excluded_tasks


for dec_name in decoupling.keys():
    algos = [
          f"{BASE_REVISION}-lpmm-b50k-linearcggl-exacticaps23",
          f"{BASE_REVISION}-lpmm-b50k-compliantsbmiasm-explistate",
          f"{BASE_REVISION}-lpmm-b50k-linearcggl-explistate",
          f"{BASE_REVISION}-lpmm-b50k-compliantsbmiasm-exactcompliant",
        # f'{REVISION}-blind-explicit',
        # f'{REVISION}-lmcut-explicit',
        # f'{REVISION}-{dec_name}-blind',
        # f'{REVISION}-{dec_name}-lmcut',
    ]
    algos.extend([f"{REVISION}-{config.nick}" for config in CONFIGS if config.nick.startswith(dec_name)])
    algos = sorted(algos, key=lambda x : x[41:])
    f = Filter(dec_name, algos)
    report_name=f'{exp.name}-abs-{dec_name}'
    report_file=Path(exp.eval_dir) / f'{report_name}.html'
    exp.add_report(
        AbsoluteReport(
            attributes=attributes,
            filter_algorithm=algos,
            filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
        ),
        name=report_name,
        outfile=report_file,
    )
    #exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

RUNTIME_PLOT_CONFIG_PAIRS = [
    [f"{BASE_REVISION}-lpmm-b50k-compliantsbmiasm-exactnocache", f"{REVISION}-lpmm-b50k-compliantsbmiasm-exact"],
    [f"{BASE_REVISION}-lpmm-b50k-linearcggl-exactnocache",       f"{REVISION}-lpmm-b50k-linearcggl-exact"],
    [f"{BASE_REVISION}-lpmm-b50k-sbmiasm-exactnocache",          f"{REVISION}-lpmm-b50k-sbmiasm-exact"],
#    [f"{REVISION}-lpmm-b50k-compliantsbmiasm-exact",        f"{BASE_REVISION}-lpmm-b50k-compliantsbmiasm-exactcompliant"],
#    [f"{REVISION}-lpmm-b50k-compliantsbmiasm-exactnocache", f"{BASE_REVISION}-lpmm-b50k-compliantsbmiasm-exactcompliant"],
]

def filter_zero_nesting(run):
    if ("ms_max_num_nested_leaves" in run and run["ms_max_num_nested_leaves"] == 0 and "search_time" in run):
        del run["search_time"]
    return run

PNG_OR_TEX='png'
for c1, c2 in RUNTIME_PLOT_CONFIG_PAIRS:
    report_name=f'{exp.name}-plot-searchtime-{c1}-vs-{c2}.{PNG_OR_TEX}'
    report_file=Path(exp.eval_dir) / f'{report_name}'
    exp.add_report(
        ScatterPlotReport(
            attributes='search_time',
            filter_algorithm=[c1, c2],
            #filter=filter_zero_nesting,
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            get_category=lambda run1, run2: f"{str(run2['ms_max_num_nested_leaves']) + run2['domain'] + run2['problem'] if 'ms_max_num_nested_leaves' in run2 and run2['ms_max_num_nested_leaves'] == 1 else ''}", #lambda run1, run2: run1['domain'],
            format=PNG_OR_TEX,
            show_missing=False,
        ),
        name=report_name,
        outfile=report_file,
    )


exp.run_steps()
