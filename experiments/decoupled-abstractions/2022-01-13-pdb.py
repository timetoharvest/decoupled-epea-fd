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

import common_setup
from common_setup import IssueConfig, IssueExperiment

DIR = os.path.dirname(os.path.abspath(__file__))
SCRIPT_NAME = os.path.splitext(os.path.basename(__file__))[0]
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"]
REVISION = "bc40844ff952e620cc4f671d5e01553009029422"
REVISIONS = [REVISION]
CONFIGS = []
decoupling = {
    'fork' : 'fork()',
    'ifork' : 'ifork()',
    'mmstrict' : 'lp_factoring(factoring_type=strict, strategy=mm)',
    'mmgeneral' : 'lp_factoring(factoring_type=general, strategy=mm)',
}
# seeds = [i for i in range(2022, 2027)]
seeds = [2022]
for name, dec in decoupling.items():
    for seed in seeds:
        CONFIGS.append(IssueConfig(f'{name}-pdb-cegar-rnd{seed}', ['--decoupling', dec, '--search',  f'astar(pdb(cegar_pattern(random_seed={seed})))'], build_options=['release64'], driver_options=['--build', 'release64'])),
    CONFIGS.append(IssueConfig(f'{name}-pdb-greedy', ['--decoupling', dec, '--search',  f'astar(pdb(greedy()))'], build_options=['release64'], driver_options=['--build', 'release64'])),

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
exp.add_parser('pattern-collection-filter-parser.py')
exp.add_parser('decoupled-parser.py')

exp.add_step('build', exp.build)
exp.add_step('start', exp.start_runs)
exp.add_fetcher(name='fetch')

# pattern filter attributes
average_num_affected_leaf_factors = Attribute('average_num_affected_leaf_factors', absolute=False, min_wins=True, function=arithmetic_mean)
maximum_num_affected_leaf_factors = Attribute('maximum_num_affected_leaf_factors', absolute=False, min_wins=True, function=arithmetic_mean)

# decoupled attributes
average_member_states = Attribute('average_member_states', absolute=False, min_wins=True, function=arithmetic_mean)

attributes = [
    average_num_affected_leaf_factors,
    maximum_num_affected_leaf_factors,
    average_member_states,
]

attributes.extend(exp.DEFAULT_TABLE_ATTRIBUTES)

for name, _ in decoupling.items():
    algorithms=[
        f'{REVISION}-{name}-pdb-cegar-rnd{seed}' for seed in seeds
    ]
    algorithms.append(f'{REVISION}-{name}-pdb-greedy')
    report_name=f'{exp.name}-absolute-{name}'
    report_file=Path(exp.eval_dir) / f'{report_name}.html'
    exp.add_report(
        AbsoluteReport(
            attributes=attributes,
            filter_algorithm=algorithms,
        ),
        name=report_name,
        outfile=report_file,
    )
    exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

BASELINE_EXPLICIT_REV="bc40844ff952e620cc4f671d5e01553009029422"
exp.add_fetcher('data/2022-01-13-baseline-pdb-eval', merge=True)
BASELINE_DECOUPLED_REVISION = "f07d81fd0b063119d86add881dc1ac8ab6b9f171"
decoupled_algos=[
    f'{BASELINE_DECOUPLED_REVISION}-{name}-{heur}' for name in decoupling.keys() for heur in ['blind', 'lmcut']
]
exp.add_fetcher('data/2022-01-11-baseline-decoupled-eval', merge=True,filter_algorithm=decoupled_algos)

class Filter:
    def __init__(self, dec):
        self.decoupling = dec
        self.excluded_tasks = set()

    def collect_non_factorable_tasks(self, run):
        # Only consider algorithms of the right factoring.
        if f'-{self.decoupling}-' not in run['algorithm']:
            return True
        error = run.get('error')
        if error is not None and error == 'search-abstained':
            self.excluded_tasks.add(f"{run['domain']}:{run['problem']}")
        return True

    def filter_interesting_tasks(self, run):
        return f"{run['domain']}:{run['problem']}" not in self.excluded_tasks

for name, _ in decoupling.items():
    f = Filter(name)
    algorithms=[
        f'{BASELINE_DECOUPLED_REVISION}-{name}-blind',
        f'{BASELINE_DECOUPLED_REVISION}-{name}-lmcut',
        f'{BASELINE_EXPLICIT_REV}-explicit-pdb-greedy',
        f'{REVISION}-{name}-pdb-greedy',
        f'{BASELINE_EXPLICIT_REV}-explicit-pdb-cegar-rnd2022',
        f'{REVISION}-{name}-pdb-cegar-rnd2022',
    ]
    report_name=f'{exp.name}-absolute-with-baselines-{name}'
    report_file=Path(exp.eval_dir) / f'{report_name}.html'
    exp.add_report(
        AbsoluteReport(
            attributes=attributes,
            filter_algorithm=algorithms,
            filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
        ),
        name=report_name,
        outfile=report_file,
    )
    exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

exp.run_steps()
