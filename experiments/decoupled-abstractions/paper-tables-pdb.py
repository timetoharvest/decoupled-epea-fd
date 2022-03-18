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
    'e99b8c6ced5b5d195f2210d3de9f21cfa4dd5784', # 2022-01-05
    '6771467f81f8d1e6fb3ebc19114d833b9932950f', # 2022-03-03
    'c634daffdd65b58993a22bfa65218dece17751ca', # 2022-03-16
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

exp.add_fetcher('data/2022-01-05-baseline-pdb-eval',filter=[remove_revision],filter_algorithm=[
    'pdbgreedy',
])

exp.add_fetcher('data/2022-03-03-decoupled-pdb-eval',filter=[remove_revision],filter_algorithm=[
    'fork-pdb-greedy',
    'ifork-pdb-greedy',
    'mmgeneral-pdb-greedy',
    'fork-pdbpoly-greedy',
    'ifork-pdbpoly-greedy',
    'mmgeneral-pdbpoly-greedy',
    'fork-pdb-greedy1leaves',
    'ifork-pdb-greedy1leaves',
    'mmgeneral-pdb-greedy1leaves',
    'fork-pdbpoly-greedy1leaves',
    'ifork-pdbpoly-greedy1leaves',
    'mmgeneral-pdbpoly-greedy1leaves',
    'fork-pdb-greedy2leaves',
    'ifork-pdb-greedy2leaves',
    'mmgeneral-pdb-greedy2leaves',
    'fork-pdbpoly-greedy2leaves',
    'ifork-pdbpoly-greedy2leaves',
    'mmgeneral-pdbpoly-greedy2leaves',
    'fork-pdb-greedy3leaves',
    'ifork-pdb-greedy3leaves',
    'mmgeneral-pdb-greedy3leaves',
    'fork-pdbpoly-greedy3leaves',
    'ifork-pdbpoly-greedy3leaves',
    'mmgeneral-pdbpoly-greedy3leaves',
],merge=True)

exp.add_fetcher('data/2022-03-16-explicit-decoupled-pdb-eval',filter=[remove_revision],filter_algorithm=[
    'explicit-fork-pdb-greedy_1leaves',
    'explicit-ifork-pdb-greedy_1leaves',
    'explicit-mmgeneral-pdb-greedy_1leaves',
    'explicit-fork-pdb-greedy_2leaves',
    'explicit-ifork-pdb-greedy_2leaves',
    'explicit-mmgeneral-pdb-greedy_2leaves',
    'explicit-fork-pdb-greedy_3leaves',
    'explicit-ifork-pdb-greedy_3leaves',
    'explicit-mmgeneral-pdb-greedy_3leaves',
],merge=True)

exp.add_fetcher('data/2022-03-17-explicit-decoupled-pdb-eval',filter=[remove_revision],filter_algorithm=[
    f'explicit-fork-pdb-greedy',
    f'explicit-ifork-pdb-greedy',
    f'explicit-mmgeneral-pdb-greedy',
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
        f'{name}-pdb-greedy1leaves',
        f'{name}-pdbpoly-greedy1leaves',
        f'explicit-{name}-pdb-greedy_2leaves',
        f'{name}-pdb-greedy2leaves',
        f'{name}-pdbpoly-greedy2leaves',
        f'explicit-{name}-pdb-greedy_3leaves',
        f'{name}-pdb-greedy3leaves',
        f'{name}-pdbpoly-greedy3leaves',
        'pdbgreedy',
        f'explicit-{name}-pdb-greedy',
        f'{name}-pdb-greedy',
        f'{name}-pdbpoly-greedy',
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

HTML_OR_TEX='tex'
for name in DECOUPLINGS:
    algorithms=[
        # f'explicit-{name}-pdb-greedy_1leaves',
        # f'{name}-pdb-greedy1leaves',
        # f'{name}-pdbpoly-greedy1leaves',
        # f'explicit-{name}-pdb-greedy_2leaves',
        # f'{name}-pdb-greedy2leaves',
        # f'{name}-pdbpoly-greedy2leaves',
        # f'explicit-{name}-pdb-greedy_3leaves',
        # f'{name}-pdb-greedy3leaves',
        # f'{name}-pdbpoly-greedy3leaves',
        'pdbgreedy',
        f'explicit-{name}-pdb-greedy',
        f'{name}-pdb-greedy',
        f'{name}-pdbpoly-greedy',
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

PNG_OR_TEX='tex'
for name in DECOUPLINGS:
    report_name=f'{exp.name}-plot-searchtime-pdb-vs-pdbply-{name}.{PNG_OR_TEX}'
    report_file=Path(exp.eval_dir) / f'{report_name}'
    exp.add_report(
        ScatterPlotReport(
            attributes='search_time',
            filter_algorithm=[
                f'{name}-pdb-greedy',
                f'{name}-pdbpoly-greedy',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            # get_category=lambda run1, run2: run1['domain'],
            format=PNG_OR_TEX,
        ),
        name=report_name,
        outfile=report_file,
    )

exp.run_steps()
