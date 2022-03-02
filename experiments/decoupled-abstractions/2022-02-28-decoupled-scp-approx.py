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
REVISION = "6771467f81f8d1e6fb3ebc19114d833b9932950f"
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
    CONFIGS.append(IssueConfig(f'{name}-scpapproxpoly-all-cegar-sys2-hc', ['--decoupling', dec, '--search',  f'astar(max_scp_approx(abstractions=[projections(patterns=multiple_cegar(total_max_time={CEGAR_MAX_TIME})),projections(patterns=systematic(pattern_max_size=2,max_time={SYS_MAX_TIME})),projections(patterns=hillclimbing(max_time={HC_MAX_TIME}))],max_time={ORDERS_MAX_TIME},saturator=all))'], build_options=['release64'], driver_options=['--build', 'release64']))
    CONFIGS.append(IssueConfig(f'{name}-scpapproxpoly-allleafdisjoint-cegar-sys2-hc', ['--decoupling', dec, '--search',  f'astar(max_scp_approx(abstractions=[projections(patterns=multiple_cegar(total_max_time={CEGAR_MAX_TIME})),projections(patterns=systematic(pattern_max_size=2,max_time={SYS_MAX_TIME})),projections(patterns=hillclimbing(max_time={HC_MAX_TIME}))],max_time={ORDERS_MAX_TIME},saturator=all_leaf_disjoint))'], build_options=['release64'], driver_options=['--build', 'release64']))

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

exp.add_absolute_report_step(attributes=attributes)

OLD_REV='14515ac7c622a1ea0e37069146eebb568a446472'
algorithms = []
for name in decoupling.keys():
    algorithms.append(f'{OLD_REV}-{name}-scpapprox-all-cegar-sys2-hc')
    algorithms.append(f'{OLD_REV}-{name}-scpapprox-allleafdisjoint-cegar-sys2-hc')
exp.add_fetcher('data/2022-01-11-scp-approx-eval', filter_algorithm=algorithms,merge=True)

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

for name in decoupling.keys():
    algo_pairs=[
        (f'{OLD_REV}-{name}-scpapprox-all-cegar-sys2-hc', f'{REVISION}-{name}-scpapproxpoly-hc'),
        (f'{OLD_REV}-{name}-scpapprox-allleafdisjoint-cegar-sys2-hc', f'{REVISION}-{name}-scpapproxpoly-hcdisjointleaf'),
    ]
    all_algos = [algo for algo_pair in algo_pairs for algo in algo_pair]
    all_algos = list(set(all_algos))
    f = Filter(name, all_algos)
    report_name=f'{exp.name}-compare-old-{name}'
    report_file=Path(exp.eval_dir) / f'{report_name}.html'
    exp.add_report(
        ComparativeReport(
            attributes=attributes,
            algorithm_pairs=algo_pairs,
            filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
        ),
        name=report_name,
        outfile=report_file,
    )
    exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

exp.run_steps()
