#! /usr/bin/env python3

import itertools
import math
import os
from pathlib import Path
import subprocess

from lab.environments import LocalEnvironment, BaselSlurmEnvironment
from lab.reports import Attribute, geometric_mean, arithmetic_mean

from downward.reports.compare import ComparativeReport

import common_setup
from common_setup import IssueConfig, IssueExperiment

DIR = os.path.dirname(os.path.abspath(__file__))
SCRIPT_NAME = os.path.splitext(os.path.basename(__file__))[0]
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"]
REVISION = "e99b8c6ced5b5d195f2210d3de9f21cfa4dd5784"
REVISIONS = [REVISION]
TIME_LIMIT=2
if common_setup.is_running_on_cluster():
    TIME_LIMIT=100
CONFIGS = [
    IssueConfig('incarcs-oneleafpatterns-cpdbscegar', ['--decoupling', 'incarcs()', '--search',  f'astar(cpdbs(multiple_cegar(pattern_filter=one_leaf_patterns,total_max_time={TIME_LIMIT})))'], build_options=['release64'], driver_options=['--build', 'release64']),
    IssueConfig('incarcs-oneleafpatterns-cpdbshc', ['--decoupling', 'incarcs()', '--search',  f'astar(cpdbs(hillclimbing(pattern_filter=one_leaf_patterns,max_time={TIME_LIMIT})))'], build_options=['release64'], driver_options=['--build', 'release64']),
    IssueConfig('incarcs-oneleafpatterns-cpdbssys2', ['--decoupling', 'incarcs()', '--search',  'astar(cpdbs(systematic(2,pattern_filter=one_leaf_patterns)))'], build_options=['release64'], driver_options=['--build', 'release64']),
]

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
exp.add_parser('pattern-collection-generator-parser.py')
exp.add_parser('pattern-collection-filter-parser.py')

exp.add_step('build', exp.build)
exp.add_step('start', exp.start_runs)
exp.add_fetcher(name='fetch')

# pattern generator attributes
pattern_collection_computation_time = Attribute('pattern_collection_computation_time', absolute=False, min_wins=True, function=geometric_mean)
pattern_collection_num_patterns = Attribute('pattern_collection_num_patterns', absolute=True, min_wins=False, function=sum)

# pattern filter attributes
average_num_affected_leaf_factors = Attribute('average_num_affected_leaf_factors', absolute=False, min_wins=True, function=arithmetic_mean)
maximum_num_affected_leaf_factors = Attribute('maximum_num_affected_leaf_factors', absolute=False, min_wins=True, function=arithmetic_mean)

attributes = [
    pattern_collection_computation_time,
    pattern_collection_num_patterns,

    average_num_affected_leaf_factors,
    maximum_num_affected_leaf_factors,
]

attributes.extend(exp.DEFAULT_TABLE_ATTRIBUTES)

exp.add_absolute_report_step(
    attributes=attributes,
    filter_algorithm=[
        f'{REVISION}-cpdbscegar',
        f'{REVISION}-cpdbsreedy',
        f'{REVISION}-cpdbsrandom',
    ]
)

BASELINE_REV=REVISION
exp.add_fetcher('data/2022-01-05-baseline-cpdbs-eval', merge=True)

class Filter:
    def __init__(self):
        self.excluded_tasks = set()

    def collect_non_factorable_tasks(self, run):
        error = run.get('error')
        if error is not None and error == 'exitcode-19':
            self.excluded_tasks.add(f"{run['domain']}:{run['problem']}")
        return True

    def filter_interesting_tasks(self, run):
        return f"{run['domain']}:{run['problem']}" not in self.excluded_tasks

f = Filter()
report_name=f'{exp.name}-compare-baseline-pdb'
report_file=Path(exp.eval_dir) / f'{report_name}.html'
exp.add_report(
    ComparativeReport(
        attributes=attributes,
        algorithm_pairs=[
            (f'{BASELINE_REV}-cpdbscegar', f'{REVISION}-incarcs-oneleafpatterns-cpdbscegar'),
            (f'{BASELINE_REV}-cpdbshc', f'{REVISION}-incarcs-oneleafpatterns-cpdbshc'),
            (f'{BASELINE_REV}-cpdbssys2', f'{REVISION}-incarcs-oneleafpatterns-cpdbssys2'),
        ],
        filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
    ),
    name=report_name,
    outfile=report_file,
)
exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

exp.run_steps()
