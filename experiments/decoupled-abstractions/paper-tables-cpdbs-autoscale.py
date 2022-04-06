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
    'e843f20ac7e877edb548acd4f0110ccbd0137525', # 2022-04-01
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

exp.add_fetcher('data/2022-04-01-explicit-baseline-blind-lmcut-ipc-eval',filter=[remove_revision],filter_algorithm=[
    'explicit-blind',
    'explicit-lmcut',
])

exp.add_fetcher('data/2022-04-01-decoupled-baseline-blind-lmcut-ipc-eval',filter=[remove_revision],filter_algorithm=[
    'fork-blind',
    'ifork-blind',
    'mmgeneral-blind',
    'fork-lmcut',
    'ifork-lmcut',
    'mmgeneral-lmcut',
],merge=True)

exp.add_fetcher('data/2022-04-01-explicit-baseline-cpdbs-ipc-eval',filter=[remove_revision],filter_algorithm=[
    'explicit-cpdbs-hc',
    'explicit-fork-cpdbs-hc_1leaves',
    'explicit-fork-cpdbs-hc_2leaves',
    'explicit-fork-cpdbs-hc_3leaves',
    'explicit-fork-cpdbs-hc_infinityleaves',
    'explicit-ifork-cpdbs-hc_1leaves',
    'explicit-ifork-cpdbs-hc_2leaves',
    'explicit-ifork-cpdbs-hc_3leaves',
    'explicit-ifork-cpdbs-hc_infinityleaves',
    'explicit-mmgeneral-cpdbs-hc_1leaves',
    'explicit-mmgeneral-cpdbs-hc_2leaves',
    'explicit-mmgeneral-cpdbs-hc_3leaves',
    'explicit-mmgeneral-cpdbs-hc_infinityleaves',
    'explicit-fork-cpdbs-hcleafdisjoint_1leaves',
    'explicit-fork-cpdbs-hcleafdisjoint_2leaves',
    'explicit-fork-cpdbs-hcleafdisjoint_3leaves',
    'explicit-fork-cpdbs-hcleafdisjoint_infinityleaves',
    'explicit-ifork-cpdbs-hcleafdisjoint_1leaves',
    'explicit-ifork-cpdbs-hcleafdisjoint_2leaves',
    'explicit-ifork-cpdbs-hcleafdisjoint_3leaves',
    'explicit-ifork-cpdbs-hcleafdisjoint_infinityleaves',
    'explicit-mmgeneral-cpdbs-hcleafdisjoint_1leaves',
    'explicit-mmgeneral-cpdbs-hcleafdisjoint_2leaves',
    'explicit-mmgeneral-cpdbs-hcleafdisjoint_3leaves',
    'explicit-mmgeneral-cpdbs-hcleafdisjoint_infinityleaves',
],merge=True)

exp.add_fetcher('data/2022-04-01-decoupled-cpdbs-exp-ipc-eval',filter=[remove_revision],filter_algorithm=[
    'fork-cpdbsexp_prune_noninc_static-hc_1leaves',
    'fork-cpdbsexp_prune_noninc_leafid-hc_1leaves',
    'fork-cpdbsexp_prune_copy_static-hc_1leaves',
    'fork-cpdbsexp_prune_copy_leafid-hc_1leaves',
    'fork-cpdbsexp_noprune_noninc_leafid-hc_1leaves',
    'ifork-cpdbsexp_prune_noninc_static-hc_1leaves',
    'ifork-cpdbsexp_prune_noninc_leafid-hc_1leaves',
    'ifork-cpdbsexp_prune_copy_static-hc_1leaves',
    'ifork-cpdbsexp_prune_copy_leafid-hc_1leaves',
    'ifork-cpdbsexp_noprune_noninc_leafid-hc_1leaves',
    'mmgeneral-cpdbsexp_prune_noninc_static-hc_1leaves',
    'mmgeneral-cpdbsexp_prune_noninc_leafid-hc_1leaves',
    'mmgeneral-cpdbsexp_prune_copy_static-hc_1leaves',
    'mmgeneral-cpdbsexp_prune_copy_leafid-hc_1leaves',
    'mmgeneral-cpdbsexp_noprune_noninc_leafid-hc_1leaves',
    'fork-cpdbsexp_prune_noninc_static-hc_2leaves',
    'fork-cpdbsexp_prune_noninc_leafid-hc_2leaves',
    'fork-cpdbsexp_prune_copy_static-hc_2leaves',
    'fork-cpdbsexp_prune_copy_leafid-hc_2leaves',
    'fork-cpdbsexp_noprune_noninc_leafid-hc_2leaves',
    'ifork-cpdbsexp_prune_noninc_static-hc_2leaves',
    'ifork-cpdbsexp_prune_noninc_leafid-hc_2leaves',
    'ifork-cpdbsexp_prune_copy_static-hc_2leaves',
    'ifork-cpdbsexp_prune_copy_leafid-hc_2leaves',
    'ifork-cpdbsexp_noprune_noninc_leafid-hc_2leaves',
    'mmgeneral-cpdbsexp_prune_noninc_static-hc_2leaves',
    'mmgeneral-cpdbsexp_prune_noninc_leafid-hc_2leaves',
    'mmgeneral-cpdbsexp_prune_copy_static-hc_2leaves',
    'mmgeneral-cpdbsexp_prune_copy_leafid-hc_2leaves',
    'mmgeneral-cpdbsexp_noprune_noninc_leafid-hc_2leaves',
    'fork-cpdbsexp_prune_noninc_static-hc_3leaves',
    'fork-cpdbsexp_prune_noninc_leafid-hc_3leaves',
    'fork-cpdbsexp_prune_copy_static-hc_3leaves',
    'fork-cpdbsexp_prune_copy_leafid-hc_3leaves',
    'fork-cpdbsexp_noprune_noninc_leafid-hc_3leaves',
    'ifork-cpdbsexp_prune_noninc_static-hc_3leaves',
    'ifork-cpdbsexp_prune_noninc_leafid-hc_3leaves',
    'ifork-cpdbsexp_prune_copy_static-hc_3leaves',
    'ifork-cpdbsexp_prune_copy_leafid-hc_3leaves',
    'ifork-cpdbsexp_noprune_noninc_leafid-hc_3leaves',
    'mmgeneral-cpdbsexp_prune_noninc_static-hc_3leaves',
    'mmgeneral-cpdbsexp_prune_noninc_leafid-hc_3leaves',
    'mmgeneral-cpdbsexp_prune_copy_static-hc_3leaves',
    'mmgeneral-cpdbsexp_prune_copy_leafid-hc_3leaves',
    'mmgeneral-cpdbsexp_noprune_noninc_leafid-hc_3leaves',
    'fork-cpdbsexp_prune_noninc_static-hc_infinityleaves',
    'fork-cpdbsexp_prune_noninc_leafid-hc_infinityleaves',
    'fork-cpdbsexp_prune_copy_static-hc_infinityleaves',
    'fork-cpdbsexp_prune_copy_leafid-hc_infinityleaves',
    'fork-cpdbsexp_noprune_noninc_leafid-hc_infinityleaves',
    'ifork-cpdbsexp_prune_noninc_static-hc_infinityleaves',
    'ifork-cpdbsexp_prune_noninc_leafid-hc_infinityleaves',
    'ifork-cpdbsexp_prune_copy_static-hc_infinityleaves',
    'ifork-cpdbsexp_prune_copy_leafid-hc_infinityleaves',
    'ifork-cpdbsexp_noprune_noninc_leafid-hc_infinityleaves',
    'mmgeneral-cpdbsexp_prune_noninc_static-hc_infinityleaves',
    'mmgeneral-cpdbsexp_prune_noninc_leafid-hc_infinityleaves',
    'mmgeneral-cpdbsexp_prune_copy_static-hc_infinityleaves',
    'mmgeneral-cpdbsexp_prune_copy_leafid-hc_infinityleaves',
    'mmgeneral-cpdbsexp_noprune_noninc_leafid-hc_infinityleaves',
],merge=True)

exp.add_fetcher('data/2022-04-01-decoupled-cpdbs-approx-ipc-eval',filter=[remove_revision],filter_algorithm=[
    'fork-cpdbsapprox-hcleafdisjoint_1leaves',
    'fork-cpdbsapproxpoly-hcleafdisjoint_1leaves',
    'ifork-cpdbsapprox-hcleafdisjoint_1leaves',
    'ifork-cpdbsapproxpoly-hcleafdisjoint_1leaves',
    'mmgeneral-cpdbsapprox-hcleafdisjoint_1leaves',
    'mmgeneral-cpdbsapproxpoly-hcleafdisjoint_1leaves',
    'fork-cpdbsapprox-hcleafdisjoint_2leaves',
    'fork-cpdbsapproxpoly-hcleafdisjoint_2leaves',
    'ifork-cpdbsapprox-hcleafdisjoint_2leaves',
    'ifork-cpdbsapproxpoly-hcleafdisjoint_2leaves',
    'mmgeneral-cpdbsapprox-hcleafdisjoint_2leaves',
    'mmgeneral-cpdbsapproxpoly-hcleafdisjoint_2leaves',
    'fork-cpdbsapprox-hcleafdisjoint_3leaves',
    'fork-cpdbsapproxpoly-hcleafdisjoint_3leaves',
    'ifork-cpdbsapprox-hcleafdisjoint_3leaves',
    'ifork-cpdbsapproxpoly-hcleafdisjoint_3leaves',
    'mmgeneral-cpdbsapprox-hcleafdisjoint_3leaves',
    'mmgeneral-cpdbsapproxpoly-hcleafdisjoint_3leaves',
    'fork-cpdbsapprox-hcleafdisjoint_infinityleaves',
    'fork-cpdbsapproxpoly-hcleafdisjoint_infinityleaves',
    'ifork-cpdbsapprox-hcleafdisjoint_infinityleaves',
    'ifork-cpdbsapproxpoly-hcleafdisjoint_infinityleaves',
    'mmgeneral-cpdbsapprox-hcleafdisjoint_infinityleaves',
    'mmgeneral-cpdbsapproxpoly-hcleafdisjoint_infinityleaves',
],merge=True)

exp.add_fetcher('data/2022-04-01-decoupled-cpdbs-singleleaf-ipc-eval',filter=[remove_revision],filter_algorithm=[
    'fork-cpdbssingleleaf-hc_1leaves',
    'ifork-cpdbssingleleaf-hc_1leaves',
    'mmgeneral-cpdbssingleleaf-hc_1leaves',
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
        f'{name}-blind',
        'explicit-lmcut',
        f'{name}-lmcut',

        f'explicit-{name}-cpdbs-hc_1leaves',
        f'explicit-{name}-cpdbs-hc_2leaves',
        f'explicit-{name}-cpdbs-hc_3leaves',
        f'explicit-{name}-cpdbs-hc_infinityleaves',
        'explicit-cpdbs-hc',
        f'explicit-{name}-cpdbs-hcleafdisjoint_1leaves',
        f'explicit-{name}-cpdbs-hcleafdisjoint_2leaves',
        f'explicit-{name}-cpdbs-hcleafdisjoint_3leaves',
        f'explicit-{name}-cpdbs-hcleafdisjoint_infinityleaves',

        f'{name}-cpdbsapprox-hcleafdisjoint_1leaves',
        f'{name}-cpdbsapproxpoly-hcleafdisjoint_1leaves',
        f'{name}-cpdbsapprox-hcleafdisjoint_2leaves',
        f'{name}-cpdbsapproxpoly-hcleafdisjoint_2leaves',
        f'{name}-cpdbsapprox-hcleafdisjoint_3leaves',
        f'{name}-cpdbsapproxpoly-hcleafdisjoint_3leaves',
        f'{name}-cpdbsapprox-hcleafdisjoint_infinityleaves',
        f'{name}-cpdbsapproxpoly-hcleafdisjoint_infinityleaves',

        f'{name}-cpdbssingleleaf-hcleafdisjoint_1leaves',

        f'{name}-cpdbsexp_noprune_noninc_leafid-hc_1leaves',
        f'{name}-cpdbsexp_noprune_noninc_leafid-hc_2leaves',
        f'{name}-cpdbsexp_noprune_noninc_leafid-hc_3leaves',
        f'{name}-cpdbsexp_noprune_noninc_leafid-hc_infinityleaves',
        f'{name}-cpdbsexp_prune_noninc_static-hc_1leaves',
        f'{name}-cpdbsexp_prune_noninc_static-hc_2leaves',
        f'{name}-cpdbsexp_prune_noninc_static-hc_3leaves',
        f'{name}-cpdbsexp_prune_noninc_static-hc_infinityleaves',
        f'{name}-cpdbsexp_prune_noninc_leafid-hc_1leaves',
        f'{name}-cpdbsexp_prune_noninc_leafid-hc_2leaves',
        f'{name}-cpdbsexp_prune_noninc_leafid-hc_3leaves',
        f'{name}-cpdbsexp_prune_noninc_leafid-hc_infinityleaves',
        f'{name}-cpdbsexp_prune_copy_static-hc_1leaves',
        f'{name}-cpdbsexp_prune_copy_static-hc_2leaves',
        f'{name}-cpdbsexp_prune_copy_static-hc_3leaves',
        f'{name}-cpdbsexp_prune_copy_static-hc_infinityleaves',
        f'{name}-cpdbsexp_prune_copy_leafid-hc_1leaves',
        f'{name}-cpdbsexp_prune_copy_leafid-hc_2leaves',
        f'{name}-cpdbsexp_prune_copy_leafid-hc_3leaves',
        f'{name}-cpdbsexp_prune_copy_leafid-hc_infinityleaves',
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

exp.run_steps()
