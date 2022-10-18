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
REVISION = "b3e66c5a28d3bd3a68192b118bbb0e0f7be2d52e"
REVISIONS = [REVISION]
TIME_LIMIT=2
if common_setup.is_running_on_cluster():
    TIME_LIMIT=900
CONFIGS = []
decoupling = {
    'fork' : 'fork()',
    'ifork' : 'ifork()',
    'mmgeneral' : 'lp_general(factoring_time_limit=30, memory_limit=4000, add_cg_sccs=true, strategy=mm_approx, min_flexibility=0.8)',
}

for dec_name, dec in decoupling.items():
    CONFIGS.append(IssueConfig(f'{dec_name}-b50k-sccdfp-exact', ['--decoupling', dec, '--search', f'astar(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_sccs(order_of_sccs=topological,merge_selector=score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,main_loop_max_time={TIME_LIMIT},decoupled_lookup=exact))'], build_options=['release64'], driver_options=['--build', 'release64']))
    CONFIGS.append(IssueConfig(f'{dec_name}-b50k-sbmiasm-exact', ['--decoupling', dec, '--search', f'astar(merge_and_shrink(shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[sf_miasm(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,threshold_before_merge=1),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false,random_seed=2016)])),label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50000,threshold_before_merge=1,main_loop_max_time={TIME_LIMIT},decoupled_lookup=exact))'], build_options=['release64'], driver_options=['--build', 'release64']))

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

exp.add_step('build', exp.build)
exp.add_step('start', exp.start_runs)
exp.add_fetcher(name='fetch')

# general search attributes
reopened_until_last_jump = Attribute('reopened_until_last_jump', absolute=False, min_wins=True, function=arithmetic_mean)

# decoupled attributes
average_member_states = Attribute('average_member_states', absolute=False, min_wins=True, function=arithmetic_mean)

attributes = [
    average_member_states,
    reopened_until_last_jump,
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

BASELINE_REV = "3e4019961fde2c2e20516af14669100f50ba69dc"
exp.add_fetcher('data/2022-10-10-explicit-baseline-mas-eval', merge=True)
exp.add_fetcher('data/2022-10-10-decoupled-mas-eval', merge=True)
OLD_REV='e843f20ac7e877edb548acd4f0110ccbd0137525'
exp.add_fetcher('data/2022-04-01-decoupled-baseline-blind-lmcut-ipc-eval', filter_algorithm=[
    f"{OLD_REV}-{name}-blind" for name in decoupling.keys()
],merge=True)
exp.add_fetcher('data/2022-04-01-decoupled-scp-exp-ipc-eval',filter_algorithm=[
    f"{OLD_REV}-{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves" for name in decoupling.keys()
],merge=True)

for dec_name in decoupling.keys():
    algos = [f"{BASELINE_REV}-explicit-b50k-sccdfp", f"{BASELINE_REV}-explicit-b50k-sbmiasm", f"{OLD_REV}-{dec_name}-blind", f"{OLD_REV}-{dec_name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves"]
    algos.extend([f"{BASELINE_REV}-{dec_name}-b50k-sccdfp-noprecompute", f"{BASELINE_REV}-{dec_name}-b50k-sbmiasm-noprecompute"])
    algos.extend([f"{REVISION}-{config.nick}" for config in CONFIGS if config.nick.startswith(dec_name)])
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
