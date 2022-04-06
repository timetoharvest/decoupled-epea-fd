#! /usr/bin/env python3

import itertools
import math
import os
from pathlib import Path
import subprocess

from lab.environments import LocalEnvironment, BaselSlurmEnvironment
from lab.reports import Attribute, geometric_mean, arithmetic_mean

from downward.experiment import FastDownwardExperiment

from downward.reports.absolute import AbsoluteReport
from downward.reports.compare import ComparativeReport
from downward.reports.scatter import ScatterPlotReport

DIR = os.path.dirname(os.path.abspath(__file__))
SCRIPT_NAME = os.path.splitext(os.path.basename(__file__))[0]
REVISIONS=[
    'e843f20ac7e877edb548acd4f0110ccbd0137525', # 2022-04-05
]

def remove_revision(run):
    unknown_rev = True
    for rev in REVISIONS:
        if rev in run['algorithm']:
            unknown_rev = False
    if unknown_rev:
        return False
    for rev in REVISIONS:
        run['algorithm'] = run['algorithm'].replace(f'{rev}-', '')
    return run

exp = FastDownwardExperiment()

exp.add_fetcher('data/2022-04-01-explicit-baseline-pdb-ipc-eval',filter=[remove_revision],filter_algorithm=[
    'pdb-greedy',
    'explicit-fork-pdb-greedy_1leaves',
    'explicit-ifork-pdb-greedy_1leaves',
    'explicit-mmgeneral-pdb-greedy_1leaves',
    'explicit-fork-pdb-greedy_2leaves',
    'explicit-ifork-pdb-greedy_2leaves',
    'explicit-mmgeneral-pdb-greedy_2leaves',
    'explicit-fork-pdb-greedy_3leaves',
    'explicit-ifork-pdb-greedy_3leaves',
    'explicit-mmgeneral-pdb-greedy_3leaves',
    'explicit-fork-pdb-greedy_infinityleaves',
    'explicit-ifork-pdb-greedy_infinityleaves',
    'explicit-mmgeneral-pdb-greedy_infinityleaves',
])

exp.add_fetcher('data/2022-04-01-decoupled-pdb-ipc-eval',filter=[remove_revision],filter_algorithm=[
    'fork-pdb-greedy_1leaves',
    'ifork-pdb-greedy_1leaves',
    'mmgeneral-pdb-greedy_1leaves',
    'fork-pdbpoly-greedy_1leaves',
    'ifork-pdbpoly-greedy_1leaves',
    'mmgeneral-pdbpoly-greedy_1leaves',
    'fork-pdb-greedy_2leaves',
    'ifork-pdb-greedy_2leaves',
    'mmgeneral-pdb-greedy_2leaves',
    'fork-pdbpoly-greedy_2leaves',
    'ifork-pdbpoly-greedy_2leaves',
    'mmgeneral-pdbpoly-greedy_2leaves',
    'fork-pdb-greedy_3leaves',
    'ifork-pdb-greedy_3leaves',
    'mmgeneral-pdb-greedy_3leaves',
    'fork-pdbpoly-greedy_3leaves',
    'ifork-pdbpoly-greedy_3leaves',
    'mmgeneral-pdbpoly-greedy_3leaves',
    'fork-pdb-greedy_infinityleaves',
    'ifork-pdb-greedy_infinityleaves',
    'mmgeneral-pdb-greedy_infinityleaves',
    'fork-pdbpoly-greedy_infinityleaves',
    'ifork-pdbpoly-greedy_infinityleaves',
    'mmgeneral-pdbpoly-greedy_infinityleaves',
],merge=True)

DECOUPLINGS = [
    'fork',
    'ifork',
    'mmgeneral',
]

HTML_OR_TEX = 'html'

# pattern attributes
num_affected_leaf_factors = Attribute('num_affected_leaf_factors', absolute=False, min_wins=True, function=arithmetic_mean)

# decoupled attributes
average_member_states = Attribute('average_member_states', absolute=False, min_wins=True, function=arithmetic_mean)

TABLE_ATTRIBUTES = [
    "cost",
    "coverage",
    "error",
    "evaluations",
    "expansions",
    "expansions_until_last_jump",
    "generated",
    "memory",
    "planner_memory",
    "planner_time",
    "quality",
    "run_dir",
    "score_evaluations",
    "score_expansions",
    "score_generated",
    "score_memory",
    "score_search_time",
    "score_total_time",
    "search_time",
    "total_time",

    num_affected_leaf_factors,
    average_member_states,
    ]

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

exp.add_report(AbsoluteReport(attributes=['coverage']))

for name in DECOUPLINGS:
    algorithms=[
        f'explicit-{name}-pdb-greedy_1leaves',
        f'{name}-pdb-greedy_1leaves',
        f'{name}-pdbpoly-greedy_1leaves',
        f'explicit-{name}-pdb-greedy_2leaves',
        f'{name}-pdb-greedy_2leaves',
        f'{name}-pdbpoly-greedy_2leaves',
        f'explicit-{name}-pdb-greedy_3leaves',
        f'{name}-pdb-greedy_3leaves',
        f'{name}-pdbpoly-greedy_3leaves',
        'pdb-greedy',
        f'explicit-{name}-pdb-greedy_infinityleaves',
        f'{name}-pdb-greedy_infinityleaves',
        f'{name}-pdbpoly-greedy_infinityleaves',
    ]
    f = Filter(name, algorithms)
    report_name=f'{exp.name}-absolute-{name}.{HTML_OR_TEX}'
    report_file=Path(exp.eval_dir) / f'{report_name}'
    exp.add_report(
        AbsoluteReport(
            attributes=TABLE_ATTRIBUTES,
            filter_algorithm=algorithms,
            filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            format=HTML_OR_TEX,
        ),
        name=report_name,
        outfile=report_file,
    )
    exp.add_step(f'publish-{report_name}', subprocess.call, ['publish', report_file])

PNG_OR_TEX='tex'
for name in DECOUPLINGS:
    report_name=f'{exp.name}-plot-searchtime-pdb-vs-pdbply-{name}.{PNG_OR_TEX}'
    report_file=Path(exp.eval_dir) / f'{report_name}'
    exp.add_report(
        ScatterPlotReport(
            attributes='search_time',
            filter_algorithm=[
                f'{name}-pdb-greedy_infinityleaves',
                f'{name}-pdbpoly-greedy_infinityleaves',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            # get_category=lambda run1, run2: run1['domain'],
            format=PNG_OR_TEX,
        ),
        name=report_name,
        outfile=report_file,
    )

exp.run_steps()
