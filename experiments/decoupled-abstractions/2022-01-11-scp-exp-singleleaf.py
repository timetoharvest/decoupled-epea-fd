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
REVISION = "f07d81fd0b063119d86add881dc1ac8ab6b9f171"
REVISIONS = [REVISION]
CEGAR_MAX_TIME=2
SYS_MAX_TIME=2
HC_MAX_TIME=2
ORDERS_MAX_TIME=2
if common_setup.is_running_on_cluster():
    CEGAR_MAX_TIME=100
    SYS_MAX_TIME=100
    HC_MAX_TIME=500
    ORDERS_MAX_TIME=200
CONFIGS = []
decoupling = {
    'fork' : 'fork()',
    'ifork' : 'ifork()',
    'mmstrict' : 'lp_factoring(factoring_type=strict, strategy=mm)',
    'mmgeneral' : 'lp_factoring(factoring_type=general, strategy=mm)',
}
for name, dec in decoupling.items():
    CONFIGS.append(IssueConfig(f'{name}-scpexp-all-cegar-sys2-hc', ['--decoupling', dec, '--search',  f'astar(max_scp_exp(abstractions=[projections(patterns=multiple_cegar(total_max_time={CEGAR_MAX_TIME})),projections(patterns=systematic(pattern_max_size=2,max_time={SYS_MAX_TIME})),projections(patterns=hillclimbing(max_time={HC_MAX_TIME}))],max_time={ORDERS_MAX_TIME},saturator=all))'], build_options=['release64'], driver_options=['--build', 'release64']))

    CONFIGS.append(IssueConfig(f'{name}-scpsingleleaf-all-cegarsingleleaf-sys2singleleaf-hcsingleleaf', ['--decoupling', dec, '--search',  f'astar(max_scp_single_leaf(abstractions=[projections(patterns=multiple_cegar(total_max_time={CEGAR_MAX_TIME},pattern_filter=n_leaves_patterns(1))),projections(patterns=systematic(pattern_max_size=2,max_time={SYS_MAX_TIME},pattern_filter=n_leaves_patterns(1))),projections(patterns=hillclimbing(max_time={HC_MAX_TIME},pattern_filter=n_leaves_patterns(1)))],max_time={ORDERS_MAX_TIME},saturator=all))'], build_options=['release64'], driver_options=['--build', 'release64'])),

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
exp.add_parser('decoupled-parser.py')

exp.add_step('build', exp.build)
exp.add_step('start', exp.start_runs)
exp.add_fetcher(name='fetch')

# pattern generator attributes
pattern_collection_computation_time = Attribute('pattern_collection_computation_time', absolute=False, min_wins=True, function=geometric_mean)
pattern_collection_num_patterns = Attribute('pattern_collection_num_patterns', absolute=True, min_wins=False, function=sum)

# pattern filter attributes
average_num_affected_leaf_factors = Attribute('average_num_affected_leaf_factors', absolute=False, min_wins=True, function=arithmetic_mean)
maximum_num_affected_leaf_factors = Attribute('maximum_num_affected_leaf_factors', absolute=False, min_wins=True, function=arithmetic_mean)

# decoupled attributes
average_member_states = Attribute('average_member_states', absolute=False, min_wins=True, function=arithmetic_mean)

attributes = [
    pattern_collection_computation_time,
    pattern_collection_num_patterns,

    average_num_affected_leaf_factors,
    maximum_num_affected_leaf_factors,

    average_member_states,
]

attributes.extend(exp.DEFAULT_TABLE_ATTRIBUTES)

for name in decoupling.keys():
    algorithms=[
        f'{REVISION}-{name}-scpexp-all-cegar-sys2-hc',
        f'{REVISION}-{name}-scpsingleleaf-all-cegarsingleleaf-sys2singleleaf-hcsingleleaf',
    ]
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

BASELINE_EXPLICIT_REV="f07d81fd0b063119d86add881dc1ac8ab6b9f171"
explicit_algos=[
    f'{BASELINE_EXPLICIT_REV}-{name}-explicit-scp-all-cegar-sys2-hc' for name in decoupling.keys()
]
explicit_algos.extend([
    f'{BASELINE_EXPLICIT_REV}-explicit-scp-all-cegar-sys2-hc',
    f'{BASELINE_EXPLICIT_REV}-explicit-blind',
    f'{BASELINE_EXPLICIT_REV}-explicit-lmcut',
])
exp.add_fetcher('data/2022-01-11-baseline-explicit-eval', merge=True,filter_algorithm=explicit_algos)
# BASELINE_DECOUPLED_REVISION = "f07d81fd0b063119d86add881dc1ac8ab6b9f171"
# decoupled_algos=[
    # f'{BASELINE_DECOUPLED_REVISION}-{name}-{heur}' for name in decoupling.keys() for heur in ['blind', 'lmcut']
# ]
# exp.add_fetcher('data/2022-01-11-baseline-decoupled-eval', merge=True,filter_algorithm=decoupled_algos)
BASELINE_DECOUPLED_REVISION = "c266104fac299b327cfaaf434a8ab36070233178"
decoupled_algos=[
    f'{BASELINE_DECOUPLED_REVISION}-{name}-{heur}' for name in decoupling.keys() for heur in ['blind', 'lmcut']
]
exp.add_fetcher('data/2022-01-10-baseline-decoupled-eval', merge=True,filter_algorithm=decoupled_algos)

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
        f'{BASELINE_EXPLICIT_REV}-{name}-explicit-scp-all-cegar-sys2-hc',
        f'{BASELINE_EXPLICIT_REV}-explicit-scp-all-cegar-sys2-hc',
        f'{REVISION}-{name}-scpexp-all-cegar-sys2-hc',
        f'{REVISION}-{name}-scpsingleleaf-all-cegarsingleleaf-sys2singleleaf-hcsingleleaf',
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

# for name, _ in decoupling.items():
    # f = Filter(name)
    # report_name=f'{exp.name}-compare-{name}'
    # report_file=Path(exp.eval_dir) / f'{report_name}.png'
    # exp.add_report(
        # ComparativeReport(
            # attributes=attributes,
            # algorithm_pairs=[
                # (f'{BASELINE_SCP_REV}-scp-cegar', f'{REVISION}-{name}-scpapprox-cegar-allorders',),
                # (f'{BASELINE_SCP_REV}-scp-cegar', f'{REVISION}-{name}-scpapprox-cegar-leafdisjointorders'),
                # (f'{BASELINE_SCP_REV}-scp-cegar', f'{REVISION}-{name}-scpexp-cegar-allorders'),
                # (f'{BASELINE_SCP_REV}-scp-cegar', f'{REVISION}-{name}-scpsingleleaf-cegaroneleaf-allorders'),
            # ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
        # ),
        # name=report_name,
        # outfile=report_file,
    # )
    # exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

for name in decoupling.keys():
    f = Filter(name)
    report_name=f'{exp.name}-plot-expansions-approx-vs-exp-{name}'
    report_file=Path(exp.eval_dir) / f'{report_name}.png'
    exp.add_report(
        ScatterPlotReport(
            attributes='expansions_until_last_jump',
            filter_algorithm=[
                f'{REVISION}-{name}-scpapprox-cegar-sys2-hc',
                f'{REVISION}-{name}-scpexp-cegar-sys2-hc',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            get_category=lambda run1, run2: run1['domain']
        ),
        name=report_name,
        outfile=report_file,
    )

exp.run_steps()
