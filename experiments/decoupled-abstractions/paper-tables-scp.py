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
    'f07d81fd0b063119d86add881dc1ac8ab6b9f171', # 2022-01-11
    '14515ac7c622a1ea0e37069146eebb568a446472', # 2022-01-11
    'd10c0997191962a5e4b15eb1da3e3ce4f749addf', # 2022-03-03
    'c634daffdd65b58993a22bfa65218dece17751ca', # 2022-03-15
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

exp.add_fetcher('data/2022-01-11-baseline-decoupled-eval',filter=[remove_revision],filter_algorithm=[
    'fork-blind',
    'ifork-blind',
    'mmgeneral-blind',
    'fork-lmcut',
    'ifork-lmcut',
    'mmgeneral-lmcut',
])

exp.add_fetcher('data/2022-01-11-baseline-explicit-eval',filter=[remove_revision],filter_algorithm=[
    'explicit-blind',
    'explicit-lmcut',
    'explicit-scp-all-cegar-sys2-hc',
    'fork-explicit-scp-all-cegar-sys2-hc',
    'ifork-explicit-scp-all-cegar-sys2-hc',
    'mmgeneral-explicit-scp-all-cegar-sys2-hc',
],merge=True)

exp.add_fetcher('data/2022-01-11-baseline-explicit-approx-eval',filter=[remove_revision],filter_algorithm=[
    'fork-explicit-scp-allleafdisjoint-cegar-sys2-hc',
    'ifork-explicit-scp-allleafdisjoint-cegar-sys2-hc',
    'mmgeneral-explicit-scp-allleafdisjoint-cegar-sys2-hc',
],merge=True)

exp.add_fetcher('data/2022-01-11-baseline-explicit-singleleaf-eval',filter=[remove_revision],filter_algorithm=[
    'fork-explicit-scp-all-cegarsingleleaf-sys2singleleaf-hcsingleleaf',
    'ifork-explicit-scp-all-cegarsingleleaf-sys2singleleaf-hcsingleleaf',
    'mmgeneral-explicit-scp-all-cegarsingleleaf-sys2singleleaf-hcsingleleaf',
],merge=True)

exp.add_fetcher('data/2022-03-03-decoupled-scp-eval',filter=[remove_revision],filter_algorithm=[
    'fork-scpapprox-allleafdisjoint-cegar-sys2-hc',
    'ifork-scpapprox-allleafdisjoint-cegar-sys2-hc',
    'mmgeneral-scpapprox-allleafdisjoint-cegar-sys2-hc',
    'fork-scpapproxpoly-allleafdisjoint-cegar-sys2-hc',
    'ifork-scpapproxpoly-allleafdisjoint-cegar-sys2-hc',
    'mmgeneral-scpapproxpoly-allleafdisjoint-cegar-sys2-hc',
    'fork-scpsingleleaf-all-cegarsingleleaf-sys2singleleaf-hc1leaves',
    'ifork-scpsingleleaf-all-cegarsingleleaf-sys2singleleaf-hc1leaves',
    'mmgeneral-scpsingleleaf-all-cegarsingleleaf-sys2singleleaf-hc1leaves',
],merge=True)

exp.add_fetcher('data/2022-03-15-decoupled-scp-exp-eval',filter=[remove_revision],filter_algorithm=[
    'fork-scpexp_prune_noninc_static-all-cegar-sys2-hc',
    'fork-scpexp_prune_noninc_leafid-all-cegar-sys2-hc',
    'fork-scpexp_prune_rec_static-all-cegar-sys2-hc',
    'fork-scpexp_prune_rec_leafid-all-cegar-sys2-hc',
    'fork-scpexp_prune_copy_static-all-cegar-sys2-hc',
    'fork-scpexp_prune_copy_leafid-all-cegar-sys2-hc',
    'fork-scpexp_noprune_noninc_static-all-cegar-sys2-hc',
    'fork-scpexp_noprune_noninc_leafid-all-cegar-sys2-hc',
    'fork-scpexp_noprune_rec_static-all-cegar-sys2-hc',
    'fork-scpexp_noprune_rec_leafid-all-cegar-sys2-hc',
    'fork-scpexp_noprune_copy_static-all-cegar-sys2-hc',
    'fork-scpexp_noprune_copy_leafid-all-cegar-sys2-hc',
    'ifork-scpexp_prune_noninc_static-all-cegar-sys2-hc',
    'ifork-scpexp_prune_noninc_leafid-all-cegar-sys2-hc',
    'ifork-scpexp_prune_rec_static-all-cegar-sys2-hc',
    'ifork-scpexp_prune_rec_leafid-all-cegar-sys2-hc',
    'ifork-scpexp_prune_copy_static-all-cegar-sys2-hc',
    'ifork-scpexp_prune_copy_leafid-all-cegar-sys2-hc',
    'ifork-scpexp_noprune_noninc_static-all-cegar-sys2-hc',
    'ifork-scpexp_noprune_noninc_leafid-all-cegar-sys2-hc',
    'ifork-scpexp_noprune_rec_static-all-cegar-sys2-hc',
    'ifork-scpexp_noprune_rec_leafid-all-cegar-sys2-hc',
    'ifork-scpexp_noprune_copy_static-all-cegar-sys2-hc',
    'ifork-scpexp_noprune_copy_leafid-all-cegar-sys2-hc',
    'mmgeneral-scpexp_prune_noninc_static-all-cegar-sys2-hc',
    'mmgeneral-scpexp_prune_noninc_leafid-all-cegar-sys2-hc',
    'mmgeneral-scpexp_prune_rec_static-all-cegar-sys2-hc',
    'mmgeneral-scpexp_prune_rec_leafid-all-cegar-sys2-hc',
    'mmgeneral-scpexp_prune_copy_static-all-cegar-sys2-hc',
    'mmgeneral-scpexp_prune_copy_leafid-all-cegar-sys2-hc',
    'mmgeneral-scpexp_noprune_noninc_static-all-cegar-sys2-hc',
    'mmgeneral-scpexp_noprune_noninc_leafid-all-cegar-sys2-hc',
    'mmgeneral-scpexp_noprune_rec_static-all-cegar-sys2-hc',
    'mmgeneral-scpexp_noprune_rec_leafid-all-cegar-sys2-hc',
    'mmgeneral-scpexp_noprune_copy_static-all-cegar-sys2-hc',
    'mmgeneral-scpexp_noprune_copy_leafid-all-cegar-sys2-hc',
],merge=True)


DECOUPLINGS = [
    'fork',
    'ifork',
    'mmgeneral',
]

HTML_OR_TEX = 'html'

# pattern generator attributes
pattern_collection_computation_time = Attribute('pattern_collection_computation_time', absolute=False, min_wins=True, function=geometric_mean)
pattern_collection_num_patterns = Attribute('pattern_collection_num_patterns', absolute=True, min_wins=False, function=sum)

# pattern filter attributes
average_num_affected_leaf_factors = Attribute('average_num_affected_leaf_factors', absolute=False, min_wins=True, function=arithmetic_mean)
maximum_num_affected_leaf_factors = Attribute('maximum_num_affected_leaf_factors', absolute=False, min_wins=True, function=arithmetic_mean)

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

    pattern_collection_computation_time,
    pattern_collection_num_patterns,
    average_num_affected_leaf_factors,
    maximum_num_affected_leaf_factors,
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
        'explicit-blind',
        'explicit-lmcut',
        'explicit-scp-all-cegar-sys2-hc',
        f'{name}-explicit-scp-all-cegar-sys2-hc',
        f'{name}-explicit-scp-allleafdisjoint-cegar-sys2-hc',
        f'{name}-explicit-scp-all-cegarsingleleaf-sys2singleleaf-hcsingleleaf',
        f'{name}-scpapprox-allleafdisjoint-cegar-sys2-hc',
        f'{name}-scpapproxpoly-allleafdisjoint-cegar-sys2-hc',
        f'{name}-scpsingleleaf-all-cegarsingleleaf-sys2singleleaf-hc1leaves',
        f'{name}-scpexp_noprune_noninc_static-all-cegar-sys2-hc',
        f'{name}-scpexp_noprune_noninc_leafid-all-cegar-sys2-hc',
        f'{name}-scpexp_noprune_rec_static-all-cegar-sys2-hc',
        f'{name}-scpexp_noprune_rec_leafid-all-cegar-sys2-hc',
        f'{name}-scpexp_noprune_copy_static-all-cegar-sys2-hc',
        f'{name}-scpexp_noprune_copy_leafid-all-cegar-sys2-hc',
        f'{name}-scpexp_prune_noninc_static-all-cegar-sys2-hc',
        f'{name}-scpexp_prune_noninc_leafid-all-cegar-sys2-hc',
        f'{name}-scpexp_prune_rec_static-all-cegar-sys2-hc',
        f'{name}-scpexp_prune_rec_leafid-all-cegar-sys2-hc',
        f'{name}-scpexp_prune_copy_static-all-cegar-sys2-hc',
        f'{name}-scpexp_prune_copy_leafid-all-cegar-sys2-hc',
    ]
    f = Filter(name, algorithms)
    report_name=f'{exp.name}-absolute-{name}'
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

exp.run_steps()
