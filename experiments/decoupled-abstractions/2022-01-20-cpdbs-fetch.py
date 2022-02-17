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
REVISION = "1d424e48c3ed22f69e30fd1e843a02e3c44535ed"
REVISIONS = [REVISION]
TIME_LIMIT=2
if common_setup.is_running_on_cluster():
    TIME_LIMIT=900
CONFIGS = []
decoupling = {
    'fork' : 'fork()',
    'ifork' : 'ifork()',
    'mmstrict' : 'lp_factoring(factoring_type=strict, strategy=mm)',
    'mmgeneral' : 'lp_factoring(factoring_type=general, strategy=mm)',
}

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
    environment=ENVIRONMENT,
)
exp.add_suite(BENCHMARKS_DIR, SUITE)

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(exp.PLANNER_PARSER)

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

algos=[x for sublist in [[f'{REVISION}-{name}-cpdbsexp-hcsingleleaf', f'{REVISION}-{name}-cpdbsexp-hcdisjointleaf'] for name in decoupling.keys()] for x in sublist]
exp.add_fetcher('data/2022-01-20-cpdbs-more-exp-eval', filter_algorithm=algos)

algos=[f'{REVISION}-{name}-cpdbsexpbroken-hc' for name in decoupling.keys()]
exp.add_fetcher('data/2022-01-20-cpdbs-expbroken-eval', merge=True, filter_algorithm=algos)

EXP_REVISION='14515ac7c622a1ea0e37069146eebb568a446472'
exp_algos=[
    f'{EXP_REVISION}-{name}-cpdbsexp-hc' for name in decoupling.keys()
]
exp.add_fetcher('data/2022-01-11-cpdbs-eval', merge=True, filter_algorithm=exp_algos)

for name, _ in decoupling.items():
    algorithms=[
        f'{EXP_REVISION}-{name}-cpdbsexp-hc',
        f'{REVISION}-{name}-cpdbsexp-hcsingleleaf',
        f'{REVISION}-{name}-cpdbsexp-hcdisjointleaf',
        f'{REVISION}-{name}-cpdbsexpbroken-hc',
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

for name in decoupling.keys():
    report_name=f'{exp.name}-plot-expansions-exp-regular-vs-singleleaf-{name}'
    report_file=Path(exp.eval_dir) / f'{report_name}.png'
    exp.add_report(
        ScatterPlotReport(
            attributes='expansions',
            filter_algorithm=[
                f'{EXP_REVISION}-{name}-cpdbsexp-hc',
                f'{REVISION}-{name}-cpdbsexp-hcsingleleaf',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            get_category=lambda run1, run2: run1['domain']
        ),
        name=report_name,
        outfile=report_file,
    )

for name in decoupling.keys():
    report_name=f'{exp.name}-plot-expansions-exp-regular-vs-leafdisjoint-{name}'
    report_file=Path(exp.eval_dir) / f'{report_name}.png'
    exp.add_report(
        ScatterPlotReport(
            attributes='expansions',
            filter_algorithm=[
                f'{EXP_REVISION}-{name}-cpdbsexp-hc',
                f'{REVISION}-{name}-cpdbsexp-hcdisjointleaf',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            get_category=lambda run1, run2: run1['domain']
        ),
        name=report_name,
        outfile=report_file,
    )

for name in decoupling.keys():
    report_name=f'{exp.name}-plot-expansions-exp-vs-expbroken-{name}'
    report_file=Path(exp.eval_dir) / f'{report_name}.png'
    exp.add_report(
        ScatterPlotReport(
            attributes='expansions',
            filter_algorithm=[
                f'{EXP_REVISION}-{name}-cpdbsexp-hc',
                f'{REVISION}-{name}-cpdbsexpbroken-hc',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            get_category=lambda run1, run2: run1['domain']
        ),
        name=report_name,
        outfile=report_file,
    )
exp.run_steps()
