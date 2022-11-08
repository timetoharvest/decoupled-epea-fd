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
REVISION = "fd640d3059ccc79b5e93553d46cc0b0c5ed50e00"
REVISIONS = [REVISION]
TIME_LIMIT=900

CONFIGS = []
decoupling = {
    'fork' : 'fork()',
    'ifork' : 'ifork()',
    'lpmm' : 'lp_general(factoring_time_limit=30, memory_limit=4000, add_cg_sccs=true, strategy=mm_approx, min_flexibility=0.8)',
}
merge_strategies = {
    'dfp' : 'merge_stateless(merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',

    'sccdfp' : 'merge_clustering(clustering_factory=clustering_cg_sccs(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',

    'sbmiasm': 'merge_stateless(merge_selector=score_based_filtering(scoring_functions=[sf_miasm(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,threshold_before_merge=1),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',

    'sccsbmiasm': 'merge_clustering(clustering_factory=clustering_cg_sccs(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[sf_miasm(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,threshold_before_merge=1),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',


    'compliantdfp' : 'merge_clustering(clustering_factory=clustering_compliant_factoring(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',

    'compliantsbmiasm': 'merge_clustering(clustering_factory=clustering_compliant_factoring(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[sf_miasm(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,threshold_before_merge=1),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',
}

for strat_name, merge_strategy in merge_strategies.items():
    if (strat_name not in ["compliantdfp", "compliantsbmiasm"]):
        CONFIGS.append(IssueConfig(f'b50k-{strat_name}-explicit', ['--search',  f'astar(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy={merge_strategy},label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,main_loop_max_time={TIME_LIMIT}),check_consistency=true)'], build_options=['release64'], driver_options=['--build', 'release64']))
    for dec_name, dec in decoupling.items():
        CONFIGS.append(IssueConfig(f'{dec_name}-b50k-{strat_name}-exact', ['--decoupling', dec, '--search',  f'astar(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy={merge_strategy},label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,main_loop_max_time={TIME_LIMIT},decoupled_lookup=exact),check_consistency=true)'], build_options=['release64'], driver_options=['--build', 'release64']))
        if (strat_name in ["compliantdfp", "compliantsbmiasm"]):
            CONFIGS.append(IssueConfig(f'{dec_name}-b50k-{strat_name}-exactcompliant', ['--decoupling', dec, '--search',  f'astar(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy={merge_strategy},label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,main_loop_max_time={TIME_LIMIT},decoupled_lookup=exact_compliant_merging),check_consistency=true)'], build_options=['release64'], driver_options=['--build', 'release64']))
            expl_dec = dec.replace(")", ", abstain_type=COMPUTE_FACTORING_RUN_STANDARD)")
            CONFIGS.append(IssueConfig(f'b50k-{strat_name}-explicit-{dec_name}', ['--decoupling', expl_dec, '--search',  f'astar(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy={merge_strategy},label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,main_loop_max_time={TIME_LIMIT}),check_consistency=true)'], build_options=['release64'], driver_options=['--build', 'release64']))

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

    # decoupled attributes
    Attribute('average_member_states', absolute=False, min_wins=True, function=arithmetic_mean),

    # general search attributes
    Attribute('reopened_until_last_jump', absolute=False, min_wins=True, function=arithmetic_mean),
    Attribute('inconsistent_heuristic', absolute=True, min_wins=True, function=sum),
]

attributes.extend(exp.DEFAULT_TABLE_ATTRIBUTES)

exp.add_absolute_report_step(attributes=attributes)

exp.add_fetcher("data/2022-11-08-blind-lmcut-baselines-autoscale-eval")

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
        # f"{BASELINE_REV}-explicit-b50k-sccdfp",
        # f"{BASELINE_REV}-explicit-b50k-sbmiasm",
        # f"{BASELINE_REV}-explicit-blind",
        # f"{BASELINE_REV}-{dec_name}-scpexp_prune_copy_leafid-all-cegarsys2hc_infinityleaves",
        # f'{BASELINE_REV}-explicit-{dec_name}-b50k-compliantdfp',
         f'{REVISION}-blind-explicit',
         f'{REVISION}-lmcut-explicit',
         f'{REVISION}-{dec_name}-blind',
         f'{REVISION}-{dec_name}-lmcut',
    ]
    algos.extend([f"{REVISION}-{config.nick}" for config in CONFIGS if config.nick.startswith(dec_name) or config.nick.endswith(f"-{dec_name}") or config.nick.endswith("-explicit")])
    algos = sorted(algos)
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

exp.run_steps()
