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
    'e2b2970b5c7d4d796afda89d0891128e52c6f3f0', # 2022-03-07
    '93cca10dd5b128806feaebd88a71bd9594e75de3', # 2022-03-08
    'b6571f1cb900d4348e914cea6eee7800baf5e92e', # 2022-03-11
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

# TODO: missing
# exp.add_fetcher('data/2022-01-11-baseline-decoupled-eval',filter=[remove_revision],filter_algorithm=[
    # 'fork-blind',
    # 'ifork-blind',
    # 'mmgeneral-blind',
    # 'fork-lmcut',
    # 'ifork-lmcut',
    # 'mmgeneral-lmcut',
# ])

# TODO: missing
# exp.add_fetcher('data/2022-03-08-explicit-baseline-blind-lmcut-autoscale-eval',filter=[remove_revision],filter_algorithm=[
    # 'explicit-blind',
    # 'explicit-lmcut',
# ])

exp.add_fetcher('data/2022-03-08-explicit-baseline-cpdbs-autoscale-eval',filter=[remove_revision],filter_algorithm=[
    'explicit-fork-cpdbs-hc',
    'explicit-ifork-cpdbs-hc',
    'explicit-mmgeneral-cpdbs-hc',
    'explicit-fork-cpdbs-hcleafdisjoint',
    'explicit-ifork-cpdbs-hcleafdisjoint',
    'explicit-mmgeneral-cpdbs-hcleafdisjoint',
    'explicit-fork-cpdbs-hc1leaves',
    'explicit-ifork-cpdbs-hc1leaves',
    'explicit-mmgeneral-cpdbs-hc1leaves',
    'explicit-fork-cpdbs-hc2leaves',
    'explicit-ifork-cpdbs-hc2leaves',
    'explicit-mmgeneral-cpdbs-hc2leaves',
    'explicit-fork-cpdbs-hc3leaves',
    'explicit-ifork-cpdbs-hc3leaves',
    'explicit-mmgeneral-cpdbs-hc3leaves',
    'explicit-explicit-cpdbs-hc',
],merge=True)

exp.add_fetcher('data/2022-03-07-decoupled-cpdbs-approx-singleleaf-autoscale-eval',filter=[remove_revision],filter_algorithm=[
    'fork-cpdbsapprox-hcdisjointleaf',
    'ifork-cpdbsapprox-hcdisjointleaf',
    'mmgeneral-cpdbsapprox-hcdisjointleaf',
    'fork-cpdbsapproxpoly-hcdisjointleaf',
    'ifork-cpdbsapproxpoly-hcdisjointleaf',
    'mmgeneral-cpdbsapproxpoly-hcdisjointleaf',
    'fork-cpdbssingleleaf-hc1leaf',
    'ifork-cpdbssingleleaf-hc1leaf',
    'mmgeneral-cpdbssingleleaf-hc1leaf',
],merge=True)

exp.add_fetcher('data/2022-03-11-decoupled-cpdbs-exp-autoscale-eval',filter=[remove_revision],filter_algorithm=[
    'fork-cpdbsexp_prune_noninc_static-hc',
    'fork-cpdbsexp_prune_noninc_leafid-hc',
    'fork-cpdbsexp_prune_copy_static-hc',
    'fork-cpdbsexp_prune_copy_leafid-hc',
    'fork-cpdbsexp_noprune_noninc_leafid-hc',
    'ifork-cpdbsexp_prune_noninc_static-hc',
    'ifork-cpdbsexp_prune_noninc_leafid-hc',
    'ifork-cpdbsexp_prune_copy_static-hc',
    'ifork-cpdbsexp_prune_copy_leafid-hc',
    'ifork-cpdbsexp_noprune_noninc_leafid-hc',
    'mmgeneral-cpdbsexp_prune_noninc_static-hc',
    'mmgeneral-cpdbsexp_prune_noninc_leafid-hc',
    'mmgeneral-cpdbsexp_prune_copy_static-hc',
    'mmgeneral-cpdbsexp_prune_copy_leafid-hc',
    'mmgeneral-cpdbsexp_noprune_noninc_leafid-hc',
],merge=True)

# TODO: missing
# exp.add_fetcher('data/2022-03-16-decoupled-cpdbs-approx-filter-eval',filter=[remove_revision],filter_algorithm=[
    # 'fork-cpdbsapproxpoly-hcdisjointleaf_1leaves',
    # 'ifork-cpdbsapproxpoly-hcdisjointleaf_1leaves',
    # 'mmgeneral-cpdbsapproxpoly-hcdisjointleaf_1leaves',
    # 'fork-cpdbsapproxpoly-hcdisjointleaf_2leaves',
    # 'ifork-cpdbsapproxpoly-hcdisjointleaf_2leaves',
    # 'mmgeneral-cpdbsapproxpoly-hcdisjointleaf_2leaves',
    # 'fork-cpdbsapproxpoly-hcdisjointleaf_31leaves',
    # 'ifork-cpdbsapproxpoly-hcdisjointleaf_3leaves',
    # 'mmgeneral-cpdbsapproxpoly-hcdisjointleaf_3leaves',
# ],merge=True)

# TODO: missing
# exp.add_fetcher('data/2022-03-16-decoupled-cpdbs-exp-filter-eval',filter=[remove_revision],filter_algorithm=[
    # 'fork-cpdbsexp_noprune_noninc_leafid-hc',
    # 'fork-cpdbsexp_noprune_noninc_leafid-hc_1leaves',
    # 'fork-cpdbsexp_noprune_noninc_leafid-hc_2leaves',
    # 'fork-cpdbsexp_noprune_noninc_leafid-hc_3leaves',
    # 'fork-cpdbsexp_prune_noninc_static-hc',
    # 'fork-cpdbsexp_prune_noninc_static-hc_1leaves',
    # 'fork-cpdbsexp_prune_noninc_static-hc_2leaves',
    # 'fork-cpdbsexp_prune_noninc_static-hc_3leaves',
    # 'fork-cpdbsexp_prune_noninc_leafid-hc',
    # 'fork-cpdbsexp_prune_noninc_leafid-hc_1leaves',
    # 'fork-cpdbsexp_prune_noninc_leafid-hc_2leaves',
    # 'fork-cpdbsexp_prune_noninc_leafid-hc_3leaves',
    # 'fork-cpdbsexp_prune_copy_static-hc',
    # 'fork-cpdbsexp_prune_copy_static-hc_1leaves',
    # 'fork-cpdbsexp_prune_copy_static-hc_2leaves',
    # 'fork-cpdbsexp_prune_copy_static-hc_3leaves',
    # 'fork-cpdbsexp_prune_copy_leafid-hc',
    # 'fork-cpdbsexp_prune_copy_leafid-hc_1leaves',
    # 'fork-cpdbsexp_prune_copy_leafid-hc_2leaves',
    # 'fork-cpdbsexp_prune_copy_leafid-hc_3leaves',
    # 'ifork-cpdbsexp_noprune_noninc_leafid-hc',
    # 'ifork-cpdbsexp_noprune_noninc_leafid-hc_1leaves',
    # 'ifork-cpdbsexp_noprune_noninc_leafid-hc_2leaves',
    # 'ifork-cpdbsexp_noprune_noninc_leafid-hc_3leaves',
    # 'ifork-cpdbsexp_prune_noninc_static-hc',
    # 'ifork-cpdbsexp_prune_noninc_static-hc_1leaves',
    # 'ifork-cpdbsexp_prune_noninc_static-hc_2leaves',
    # 'ifork-cpdbsexp_prune_noninc_static-hc_3leaves',
    # 'ifork-cpdbsexp_prune_noninc_leafid-hc',
    # 'ifork-cpdbsexp_prune_noninc_leafid-hc_1leaves',
    # 'ifork-cpdbsexp_prune_noninc_leafid-hc_2leaves',
    # 'ifork-cpdbsexp_prune_noninc_leafid-hc_3leaves',
    # 'ifork-cpdbsexp_prune_copy_static-hc',
    # 'ifork-cpdbsexp_prune_copy_static-hc_1leaves',
    # 'ifork-cpdbsexp_prune_copy_static-hc_2leaves',
    # 'ifork-cpdbsexp_prune_copy_static-hc_3leaves',
    # 'ifork-cpdbsexp_prune_copy_leafid-hc',
    # 'ifork-cpdbsexp_prune_copy_leafid-hc_1leaves',
    # 'ifork-cpdbsexp_prune_copy_leafid-hc_2leaves',
    # 'ifork-cpdbsexp_prune_copy_leafid-hc_3leaves',
    # 'mmgeneral-cpdbsexp_noprune_noninc_leafid-hc',
    # 'mmgeneral-cpdbsexp_noprune_noninc_leafid-hc_1leaves',
    # 'mmgeneral-cpdbsexp_noprune_noninc_leafid-hc_2leaves',
    # 'mmgeneral-cpdbsexp_noprune_noninc_leafid-hc_3leaves',
    # 'mmgeneral-cpdbsexp_prune_noninc_static-hc',
    # 'mmgeneral-cpdbsexp_prune_noninc_static-hc_1leaves',
    # 'mmgeneral-cpdbsexp_prune_noninc_static-hc_2leaves',
    # 'mmgeneral-cpdbsexp_prune_noninc_static-hc_3leaves',
    # 'mmgeneral-cpdbsexp_prune_noninc_leafid-hc',
    # 'mmgeneral-cpdbsexp_prune_noninc_leafid-hc_1leaves',
    # 'mmgeneral-cpdbsexp_prune_noninc_leafid-hc_2leaves',
    # 'mmgeneral-cpdbsexp_prune_noninc_leafid-hc_3leaves',
    # 'mmgeneral-cpdbsexp_prune_copy_static-hc',
    # 'mmgeneral-cpdbsexp_prune_copy_static-hc_1leaves',
    # 'mmgeneral-cpdbsexp_prune_copy_static-hc_2leaves',
    # 'mmgeneral-cpdbsexp_prune_copy_static-hc_3leaves',
    # 'mmgeneral-cpdbsexp_prune_copy_leafid-hc',
    # 'mmgeneral-cpdbsexp_prune_copy_leafid-hc_1leaves',
    # 'mmgeneral-cpdbsexp_prune_copy_leafid-hc_2leaves',
    # 'mmgeneral-cpdbsexp_prune_copy_leafid-hc_3leaves',
# ],merge=True)


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
        'explicit-cpdbs-hc',
        f'explicit-{name}-cpdbs-hc',
        f'explicit-{name}-cpdbs-hcleafdisjoint',
        f'explicit-{name}-cpdbs-hc1leaves',
        f'explicit-{name}-cpdbs-hc2leaves',
        f'explicit-{name}-cpdbs-hc3leaves',
        # f'{name}-cpdbsapprox-hcdisjointleaf',
        f'{name}-cpdbsapproxpoly-hcdisjointleaf',
        f'{name}-cpdbsapproxpoly-hcdisjointleaf_1leaves',
        f'{name}-cpdbsapproxpoly-hcdisjointleaf_2leaves',
        f'{name}-cpdbsapproxpoly-hcdisjointleaf_3leaves',
        f'{name}-cpdbssingleleaf-hc1leaf',
        f'{name}-cpdbsexp_noprune_noninc_leafid-hc',
        f'{name}-cpdbsexp_noprune_noninc_leafid-hc_1leaves',
        f'{name}-cpdbsexp_noprune_noninc_leafid-hc_2leaves',
        f'{name}-cpdbsexp_noprune_noninc_leafid-hc_3leaves',
        f'{name}-cpdbsexp_prune_noninc_static-hc',
        f'{name}-cpdbsexp_prune_noninc_static-hc_1leaves',
        f'{name}-cpdbsexp_prune_noninc_static-hc_2leaves',
        f'{name}-cpdbsexp_prune_noninc_static-hc_3leaves',
        f'{name}-cpdbsexp_prune_noninc_leafid-hc',
        f'{name}-cpdbsexp_prune_noninc_leafid-hc_1leaves',
        f'{name}-cpdbsexp_prune_noninc_leafid-hc_2leaves',
        f'{name}-cpdbsexp_prune_noninc_leafid-hc_3leaves',
        f'{name}-cpdbsexp_prune_copy_static-hc',
        f'{name}-cpdbsexp_prune_copy_static-hc_1leaves',
        f'{name}-cpdbsexp_prune_copy_static-hc_2leaves',
        f'{name}-cpdbsexp_prune_copy_static-hc_3leaves',
        f'{name}-cpdbsexp_prune_copy_leafid-hc',
        f'{name}-cpdbsexp_prune_copy_leafid-hc_1leaves',
        f'{name}-cpdbsexp_prune_copy_leafid-hc_2leaves',
        f'{name}-cpdbsexp_prune_copy_leafid-hc_3leaves',
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
