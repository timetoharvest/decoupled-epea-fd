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
    'f07d81fd0b063119d86add881dc1ac8ab6b9f171', # 2022-01-...
    'bc40844ff952e620cc4f671d5e01553009029422', # 2022-01-...
    'ce7d06bd41494b28a3f5ea1e2b3ee78f83b5c502', # 2022-01-24
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

def lmcut_and_blind(run):
    return 'blind' in run['algorithm'] or 'lmcut' in run['algorithm']

def exclude_greedy(run):
    return 'greedy' not in run['algorithm']

def duplicate_num_affected_leaf_factors(run):
    if run.get('maximum_num_affected_leaf_factors'):
        run['num_affected_leaf_factors'] = run['maximum_num_affected_leaf_factors']
    return run

exp = FastDownwardExperiment()
exp.add_fetcher('data/2022-01-11-baseline-decoupled-eval',filter=[remove_revision,lmcut_and_blind,duplicate_num_affected_leaf_factors])
exp.add_fetcher('data/2022-01-11-baseline-explicit-eval',filter=[remove_revision,lmcut_and_blind,duplicate_num_affected_leaf_factors],merge=True)
exp.add_fetcher('data/2022-01-13-baseline-pdb-eval',filter=[remove_revision,exclude_greedy,duplicate_num_affected_leaf_factors],merge=True)
exp.add_fetcher('data/2022-01-13-baseline-dec-pdb-eval',filter=[remove_revision,duplicate_num_affected_leaf_factors],merge=True)
exp.add_fetcher('data/2022-01-13-pdb-eval',filter=[remove_revision,duplicate_num_affected_leaf_factors],merge=True)
exp.add_fetcher('data/2022-01-13-pdb-greedy-eval',filter=[remove_revision,duplicate_num_affected_leaf_factors],merge=True)
exp.add_fetcher('data/2022-02-24-decoupled-pdb-eval',filter=[remove_revision],merge=True)


DECOUPLINGS = [
    'fork',
    'ifork',
    'mmstrict',
    'mmgeneral',
]

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

HTML_OR_TEX = 'html'
for name in DECOUPLINGS:
    algorithms=[
        'explicit-blind',
        f'{name}-blind',
        'explicit-lmcut',
        f'{name}-lmcut',
        'explicit-pdb-greedy',
        f'{name}-pdb-greedy',
        f'{name}-pdbpoly-greedy',
        # 'explicit-pdb-cegar-rnd2022',
        f'{name}-explicit-pdb-cegar-rnd2022',
        f'{name}-pdb-cegar-rnd2022',
        f'{name}-pdbpoly-cegar-rnd2022',
    ]
    f = Filter(name, algorithms)
    report_name=f'{exp.name}-pdb-absolute-with-baselines-{name}'
    report_file=Path(exp.eval_dir) / f'{report_name}.{HTML_OR_TEX}'
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

plot_pairs=[
    ['blind', 'pdb-cegar-rnd2022'],
    ['lmcut', 'pdb-cegar-rnd2022'],
]
plot_attributes=['expansions', 'search_time']
for plot_pair in plot_pairs:
    for attribute in plot_attributes:
        for name in DECOUPLINGS:
            f = Filter(name, [plot_pair[0], plot_pair[1]])
            report_name=f'{exp.name}-plot-{attribute}-{name}-{plot_pair[0]}-vs-{name}-{plot_pair[1]}'
            report_file=Path(exp.eval_dir) / f'{report_name}.png'
            exp.add_report(
                ScatterPlotReport(
                    attributes=attribute,
                    filter_algorithm=[f'{name}-{plot_pair[0]}', f'{name}-{plot_pair[1]}'],
                    # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
                    get_category=lambda run1, run2: run1['domain']
                ),
                name=report_name,
                outfile=report_file,
            )


exp.run_steps()
