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
REVISION = "eaa07a7dcbee6c6f54252254cfd32aaf32b409fc"
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
    # CONFIGS.append(IssueConfig(f'explicit-{name}-cpdbs-hc1leaf', ['--decoupling', dec, '--search',  f'astar(cpdbs(hillclimbing(max_time={TIME_LIMIT},pattern_filter=n_leaves_patterns(1))))'], build_options=['release64'], driver_options=['--build', 'release64'])),
    CONFIGS.append(IssueConfig(f'explicit-{name}-cpdbs-hc2leaves', ['--decoupling', dec, '--search',  f'astar(cpdbs(hillclimbing(max_time={TIME_LIMIT},pattern_filter=n_leaves_patterns(2))))'], build_options=['release64'], driver_options=['--build', 'release64'])),
    CONFIGS.append(IssueConfig(f'explicit-{name}-cpdbs-hc3leaves', ['--decoupling', dec, '--search',  f'astar(cpdbs(hillclimbing(max_time={TIME_LIMIT},pattern_filter=n_leaves_patterns(3))))'], build_options=['release64'], driver_options=['--build', 'release64'])),
    # CONFIGS.append(IssueConfig(f'explicit-{name}-cpdbs-hc', ['--decoupling', dec, '--search',  f'astar(cpdbs(hillclimbing(max_time={TIME_LIMIT})))'], build_options=['release64'], driver_options=['--build', 'release64'])),

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

# decoupled attributes
average_member_states = Attribute('average_member_states', absolute=False, min_wins=True, function=arithmetic_mean)

attributes = [
    average_member_states,
]

attributes.extend(exp.DEFAULT_TABLE_ATTRIBUTES)

exp.add_absolute_report_step(attributes=attributes)

exp.run_steps()
