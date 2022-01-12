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
REVISION = "14515ac7c622a1ea0e37069146eebb568a446472"
REVISIONS = [REVISION]
TIME_LIMIT=2
if common_setup.is_running_on_cluster():
    TIME_LIMIT=900
CONFIGS = []
decoupling = {
    'fork' : 'fork(abstain_type=COMPUTE_FACTORING_RUN_STANDARD)',
    'ifork' : 'ifork(abstain_type=COMPUTE_FACTORING_RUN_STANDARD)',
    'mmstrict' : 'lp_factoring(factoring_type=strict, strategy=mm, abstain_type=COMPUTE_FACTORING_RUN_STANDARD)',
    'mmgeneral' : 'lp_factoring(factoring_type=general, strategy=mm,abstain_type=COMPUTE_FACTORING_RUN_STANDARD)',
}
for name, dec in decoupling.items():
    CONFIGS.append(IssueConfig(f'{name}-explicit-cpdbs-hc', ['--decoupling', dec, '--search',  f'astar(cpdbs(hillclimbing(max_time={TIME_LIMIT})))'], build_options=['release64'], driver_options=['--build', 'release64'])),
    CONFIGS.append(IssueConfig(f'{name}-explicit-cpdbs-hcleafdisjoint', ['--decoupling', dec, '--search',  f'astar(cpdbs(hillclimbing(max_time={TIME_LIMIT},disjoint_leaf_additivity=true)))'], build_options=['release64'], driver_options=['--build', 'release64'])),
    CONFIGS.append(IssueConfig(f'{name}-explicit-cpdbs-hcsingleleaf', ['--decoupling', dec, '--search',  f'astar(cpdbs(hillclimbing(max_time={TIME_LIMIT},pattern_filter=n_leaves_patterns(1))))'], build_options=['release64'], driver_options=['--build', 'release64'])),
CONFIGS.append(IssueConfig('explicit-cpdbs-hc', ['--search',  f'astar(cpdbs(hillclimbing(max_time={TIME_LIMIT})))'], build_options=['release64'], driver_options=['--build', 'release64'])),

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

exp.add_step('build', exp.build)
exp.add_step('start', exp.start_runs)
exp.add_fetcher(name='fetch')

# pattern generator attributes
pattern_collection_computation_time = Attribute('pattern_collection_computation_time', absolute=False, min_wins=True, function=geometric_mean)
pattern_collection_num_patterns = Attribute('pattern_collection_num_patterns', absolute=True, min_wins=False, function=sum)

attributes = [
    pattern_collection_computation_time,
    pattern_collection_num_patterns,
]

attributes.extend(exp.DEFAULT_TABLE_ATTRIBUTES)

exp.add_absolute_report_step(attributes=attributes)

report_name=f'{exp.name}-compare-explicit-cpdbs-without-vs-with-factoring'
report_file=Path(exp.eval_dir) / f'{report_name}.html'
algo_pairs=[
    (f'{REVISION}-explicit-cpdbs-hc', f'{REVISION}-{name}-explicit-cpdbs-hc') for name in decoupling.keys()
]
exp.add_report(
    ComparativeReport(
        attributes=attributes,
        algorithm_pairs=algo_pairs,
    ),
    name=report_name,
    outfile=report_file,
)
exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

report_name=f'{exp.name}-compare-explicit-cpdbs-vs-disjointleaf-and-singleleaf'
report_file=Path(exp.eval_dir) / f'{report_name}.html'
algo_pairs=[
    (f'{REVISION}-explicit-cpdbs-hc', f'{REVISION}-{name}-explicit-cpdbs-hcleafdisjoint') for name in decoupling.keys()
]
algo_pairs.extend([
    (f'{REVISION}-explicit-cpdbs-hc', f'{REVISION}-{name}-explicit-cpdbs-hcsingleleaf') for name in decoupling.keys()
])
exp.add_report(
    ComparativeReport(
        attributes=attributes,
        algorithm_pairs=algo_pairs,
    ),
    name=report_name,
    outfile=report_file,
)
exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

exp.run_steps()
