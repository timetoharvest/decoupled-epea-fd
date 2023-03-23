#! /usr/bin/env python3

import itertools
import math
import os
from pathlib import Path
import subprocess

from lab.environments import LocalEnvironment, TetralithEnvironment
from lab.reports import Attribute, geometric_mean, arithmetic_mean

from downward.reports.absolute import AbsoluteReport
from downward.reports.compare import ComparativeReport
from downward.reports.scatter import ScatterPlotReport

import common_setup
from common_setup import IssueConfig, IssueExperiment

DIR = os.path.dirname(os.path.abspath(__file__))
SCRIPT_NAME = os.path.splitext(os.path.basename(__file__))[0]
BENCHMARKS_DIR = os.environ["AUTOSCALE_BENCHMARKS_OPT"]
REVISION = "a657826557ddeee8ab70162265e932c0949f6b8b"
REVISIONS = [REVISION]

CONFIGS = []

CONFIGS.append(IssueConfig("ePDBs-SCP-sys2hc-explicit", ["--search",
           "astar(scp(abstractions=[projections(patterns=multiple_cegar(total_max_time=100)),projections(patterns=systematic(pattern_max_size=2,max_time=100)),projections(patterns=hillclimbing(max_time=500))],max_time=200,saturator=all))"], build_options=['release64'], driver_options=['--build', 'release64']))

CONFIGS.append(IssueConfig("ePDBs-SCP-sys2hc-decoupled", ["--decoupling",
           'lp_general(factoring_time_limit=30, memory_limit=4000, add_cg_sccs=true, strategy=mm_approx, min_flexibility=0.8)',
           "--search",
           "astar(max_scp_single_leaf(abstractions=[projections(patterns=multiple_cegar(total_max_time=100,pattern_filter=n_leaves_patterns(1))),projections(patterns=systematic(pattern_max_size=2,max_time=100,pattern_filter=n_leaves_patterns(1))),projections(patterns=hillclimbing(max_time=500,pattern_filter=n_leaves_patterns(1)))],max_time=200,saturator=all))"], build_options=['release64'], driver_options=['--build', 'release64']))


SUITE = sorted(["agricola", "blocksworld", "depots", "floortile", "grid", "logistics", "nomystery", "parcprinter", "pegsol", "rovers", "snake", "termes", "tidybot", "visitall",
         "airport", "childsnack", "driverlog", "freecell", "gripper", "miconic", "openstacks", "parking", "pipesworld-notankage", "satellite", "sokoban", "tetris", "tpp", "woodworking",
         "barman", "data-network", "elevators", "ged", "hiking", "mprime", "organic-synthesis-split", "pathways", "pipesworld-tankage", "scanalyzer", "storage", "thoughtful", "transport", "zenotravel"])

ENVIRONMENT = TetralithEnvironment(
    email="daniel.gnad@liu.se",
#    time_limit_per_task="24:00:00",
#    memory_per_cpu=None,
    extra_options="#SBATCH -A snic2022-22-1038"
)

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
exp.add_parse_again_step()
exp.add_fetcher(name='fetch')

attributes=[
    # decoupled attributes
    Attribute('average_member_states', absolute=False, min_wins=True, function=arithmetic_mean),
    Attribute("number_leaf_factors", absolute=False),

    # general search attributes
    Attribute('reopened_until_last_jump', absolute=False, min_wins=True, function=arithmetic_mean),
    Attribute('inconsistent_heuristic', absolute=True, min_wins=True, function=sum),
]

attributes.extend(exp.DEFAULT_TABLE_ATTRIBUTES)

#exp.add_absolute_report_step(attributes=attributes)

 
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
        if error is not None and error == 'decoupled-search-abstained':
            self.excluded_tasks.add(f"{run['domain']}:{run['problem']}")
        return True

    def filter_interesting_tasks(self, run):
        return f"{run['domain']}:{run['problem']}" not in self.excluded_tasks


for dec_name in decoupling.keys():
        algos = [
        # f"{BASE_REVISION}-lpmm-b50k-linearcggl-exacticaps23",
        # f"{BASE_REVISION}-lpmm-b50k-compliantsbmiasm-explistate",
        # f"{BASE_REVISION}-lpmm-b50k-linearcggl-explistate",
        # f"{BASE_REVISION}-lpmm-b50k-compliantsbmiasm-exactcompliant",
        # f'{REVISION}-blind-explicit',
        # f'{REVISION}-lmcut-explicit',
        # f'{REVISION}-{dec_name}-blind',
        # f'{REVISION}-{dec_name}-lmcut',
        ]
        algos.extend([f"{REVISION}-{config.nick}" for config in CONFIGS])# if config.nick.startswith(dec_name)])
        algos = sorted(algos, key=lambda x : x[41:])
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
        #exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

RUNTIME_PLOT_CONFIG_PAIRS = [
#    [f"{REVISION}-b50k-linearcg-explicit",          f"{REVISION}-lpmm-b50k-compliantlinearcg-explicit"],
#    [f"{REVISION}-b50k-linearrand-explicit",         f"{REVISION}-lpmm-b50k-compliantlinearrand-explicit"],
#    [f"{REVISION}-lpmm-b50k-linearcg-exact",        f"{REVISION}-lpmm-b50k-compliantlinearcg-exact"],
    [f"{REVISION}-lpmm-b50k-linearrand-exact",       f"{REVISION}-lpmm-b50k-compliantlinearrand-exact"],
]

def filter_zero_nesting(run):
    if ("ms_max_num_nested_leaves" in run and run["ms_max_num_nested_leaves"] == 0 and "search_time" in run):
        del run["search_time"]
    return run

PNG_OR_TEX='tex'
for c1, c2 in RUNTIME_PLOT_CONFIG_PAIRS:
    for attr in ["search_time", #"expansions_until_last_jump", 
                 "evaluations"]:
        report_name=f'{exp.name}-plot-{attr}-{c1}-vs-{c2}.{PNG_OR_TEX}'
        report_file=Path(exp.eval_dir) / f'{report_name}'
        exp.add_report(
            ScatterPlotReport(
                attributes=attr,
                filter_algorithm=[c1, c2],
                #filter=filter_zero_nesting,
                #filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
                get_category=lambda run1, run2: str(run1['ms_strongly_compliant']) if "exact" in run1["algorithm"] else "",
                format=PNG_OR_TEX,
                show_missing=False,
            ),
            name=report_name,
            outfile=report_file,
        )


exp.run_steps()
