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
BENCHMARKS_DIR = Path.home() / 'repos/downward/new-benchmarks/benchmarks/2022-03-03-opt'
REVISION = "93cca10dd5b128806feaebd88a71bd9594e75de3"
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
CONFIGS.append(IssueConfig('explicit-blind', ['--search',  f'astar(blind())'], build_options=['release64'], driver_options=['--build', 'release64']))
CONFIGS.append(IssueConfig('explicit-lmcut', ['--search',  f'astar(lmcut())'], build_options=['release64'], driver_options=['--build', 'release64']))


SUITE = [
    "agricola", "airport", "barman", "blocksworld", "childsnack",
    "data-network", "depots", "driverlog", "elevators", "floortile",
    "freecell", "ged", "grid", "gripper", "hiking", "logistics",
    "miconic", "mprime", "nomystery", "openstacks",
    "organic-synthesis-split", "parcprinter", "parking", "pathways",
    "pegsol", "pipesworld-notankage", "pipesworld-tankage", "rovers",
    "satellite", "scanalyzer", "snake", "sokoban", "storage", "termes",
    "tetris", "thoughtful", "tidybot", "tpp", "transport", "visitall",
    "woodworking", "zenotravel",
]
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
    SUITE = ['logistics:p01.pddl', 'zenotravel:p01.pddl']
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

exp.run_steps()
