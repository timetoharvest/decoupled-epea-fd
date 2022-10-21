#! /usr/bin/env python3

import itertools
import math
import os
from pathlib import Path
import subprocess

from lab.environments import LocalEnvironment, BaselSlurmEnvironment
from lab.reports import Attribute, geometric_mean, arithmetic_mean

from downward.reports.absolute import AbsoluteReport
from downward.reports.compare import ComparativeReport
from downward.reports.scatter import ScatterPlotReport

import common_setup
from common_setup import IssueConfig, IssueExperiment

DIR = os.path.dirname(os.path.abspath(__file__))
SCRIPT_NAME = os.path.splitext(os.path.basename(__file__))[0]
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"]
REVISION = "5133da136956824c227f6734ff7af242d7f64492"
REVISIONS = [REVISION]
TIME_LIMIT=2
if common_setup.is_running_on_cluster():
    TIME_LIMIT=900
CONFIGS = []
decoupling = {
    'fork' : 'fork()',
    'ifork' : 'ifork()',
    'lpmm' : 'lp_general(factoring_time_limit=30, memory_limit=4000, add_cg_sccs=true, strategy=mm_approx, min_flexibility=0.8)',
}
merge_strategies = {
    'sccdfp' : 'merge_clustering(clustering_factory=clustering_cg_sccs(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',
    'sbmiasm': 'merge_stateless(merge_selector=score_based_filtering(scoring_functions=[sf_miasm(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,threshold_before_merge=1),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))',
    'compliantdfp' : 'merge_clustering(clustering_factory=clustering_compliant_factoring(),order_of_clusters=given,merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)]))'
}

for dec_name, dec in decoupling.items():
    for strat_name, merge_strategy in merge_strategies.items():
        CONFIGS.append(IssueConfig(f'{dec_name}-b50k-{strat_name}-exact', ['--decoupling', dec, '--search',  f'astar(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy={merge_strategy},label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,main_loop_max_time={TIME_LIMIT},decoupled_lookup=exact),check_consistency=true)'], build_options=['release64'], driver_options=['--build', 'release64']))
        if strat_name == 'compliantdfp':
            CONFIGS.append(IssueConfig(f'{dec_name}-b50k-{strat_name}-exactcompliant', ['--decoupling', dec, '--search',  f'astar(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy={merge_strategy},label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,main_loop_max_time={TIME_LIMIT},decoupled_lookup=exact_compliant_merging),check_consistency=true)'], build_options=['release64'], driver_options=['--build', 'release64']))

SUITE = common_setup.DEFAULT_OPTIMAL_SUITE
ENVIRONMENT = BaselSlurmEnvironment(
    email="silvan.sievers@unibas.ch",
    partition="infai_2",
    export=[],
    # paths obtained via:
    # module purge
    # module -q load CMake/3.15.3-GCCcore-8.3.0
    # module -q load GCC/8.3.0
    # echo $PATH
    # echo $LD_LIBRARY_PATH
    setup='export PATH=/scicore/soft/apps/binutils/2.32-GCCcore-8.3.0/bin:/scicore/soft/apps/CMake/3.15.3-GCCcore-8.3.0/bin:/scicore/soft/apps/cURL/7.66.0-GCCcore-8.3.0/bin:/scicore/soft/apps/bzip2/1.0.8-GCCcore-8.3.0/bin:/scicore/soft/apps/ncurses/6.1-GCCcore-8.3.0/bin:/scicore/soft/apps/GCCcore/8.3.0/bin:/infai/sieverss/repos/bin:/infai/sieverss/local:/export/soft/lua_lmod/centos7/lmod/lmod/libexec:/usr/local/bin:/usr/bin:/usr/local/sbin:/usr/sbin:$PATH\nexport LD_LIBRARY_PATH=/scicore/soft/apps/binutils/2.32-GCCcore-8.3.0/lib:/scicore/soft/apps/cURL/7.66.0-GCCcore-8.3.0/lib:/scicore/soft/apps/bzip2/1.0.8-GCCcore-8.3.0/lib:/scicore/soft/apps/zlib/1.2.11-GCCcore-8.3.0/lib:/scicore/soft/apps/ncurses/6.1-GCCcore-8.3.0/lib:/scicore/soft/apps/GCCcore/8.3.0/lib64:/scicore/soft/apps/GCCcore/8.3.0/lib')

if common_setup.is_test_run():
    SUITE = IssueExperiment.DEFAULT_TEST_SUITE
    ENVIRONMENT = LocalEnvironment(processes=4)

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
        if error is not None and error == 'search-abstained':
            self.excluded_tasks.add(f"{run['domain']}:{run['problem']}")
        return True

    def filter_interesting_tasks(self, run):
        return f"{run['domain']}:{run['problem']}" not in self.excluded_tasks

BASELINE_REV = "da21e08a7f035ec8f3ef2f29f898a5fd7b647947"
# exp.add_fetcher('data/2022-10-18-explicit-baseline-blind-eval', filter_algorithm=[
    # f"{BASELINE_REV}-explicit-blind"
# ],merge=True)
# exp.add_fetcher('data/2022-10-18-explicit-baseline-mas-eval', filter_algorithm=[
    # f"{BASELINE_REV}-explicit-b50k-{mas_name}" for mas_name in ['sccdfp', 'sbmiasm']
# ],merge=True)
# exp.add_fetcher('data/2022-10-18-decoupled-scppdbs-exp-eval',filter_algorithm=[
    # f"{BASELINE_REV}-{name}-scpexp_prune_copy_leafid-all-cegarsys2hc_infinityleaves" for name in decoupling.keys()
# ],merge=True)
exp.add_fetcher('data/2022-10-21-explicit-decoupled-baseline-mas-eval',filter_algorithm=[
    f"{REVISION}-explicit-{dec_name}-b50k-{strat_name}" for dec_name in decoupling.keys() for strat_name in merge_strategies.keys()
],merge=True)

for dec_name in decoupling.keys():
    algos = [
        # f"{BASELINE_REV}-explicit-b50k-sccdfp",
        # f"{BASELINE_REV}-explicit-b50k-sbmiasm",
        # f"{BASELINE_REV}-explicit-blind",
        # f"{BASELINE_REV}-{dec_name}-scpexp_prune_copy_leafid-all-cegarsys2hc_infinityleaves",
        f'{REVISION}-explicit-{dec_name}-b50k-compliantdfp',
        f'{REVISION}-explicit-{dec_name}-b50k-sbmiasm',
        f'{REVISION}-explicit-{dec_name}-b50k-sccdfp',
    ]
    algos.extend([f"{REVISION}-{config.nick}" for config in CONFIGS if config.nick.startswith(dec_name)])
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
    exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

exp.run_steps()
