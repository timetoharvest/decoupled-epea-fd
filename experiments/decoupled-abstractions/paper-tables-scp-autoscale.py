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

exp.add_fetcher('data/2022-04-05-explicit-baseline-blind-lmcut-autoscale-eval',filter=[remove_revision],filter_algorithm=[
    'explicit-blind',
    'explicit-lmcut',
])

exp.add_fetcher('data/2022-04-05-decoupled-baseline-blind-lmcut-autoscale-eval',filter=[remove_revision],filter_algorithm=[
    'fork-blind',
    'ifork-blind',
    'mmgeneral-blind',
    'fork-lmcut',
    'ifork-lmcut',
    'mmgeneral-lmcut',
],merge=True)

exp.add_fetcher('data/2022-04-05-explicit-baseline-scp-autoscale-eval',filter=[remove_revision],filter_algorithm=[
    'explicit-scp-all-cegarsys2hc',
    'explicit-fork-scp-all-cegarsys2hc_1leaves',
    'explicit-fork-scp-all-cegarsys2hc_2leaves',
    'explicit-fork-scp-all-cegarsys2hc_3leaves',
    'explicit-fork-scp-all-cegarsys2hc_infinityleaves',
    'explicit-ifork-scp-all-cegarsys2hc_1leaves',
    'explicit-ifork-scp-all-cegarsys2hc_2leaves',
    'explicit-ifork-scp-all-cegarsys2hc_3leaves',
    'explicit-ifork-scp-all-cegarsys2hc_infinityleaves',
    'explicit-mmgeneral-scp-all-cegarsys2hc_1leaves',
    'explicit-mmgeneral-scp-all-cegarsys2hc_2leaves',
    'explicit-mmgeneral-scp-all-cegarsys2hc_3leaves',
    'explicit-mmgeneral-scp-all-cegarsys2hc_infinityleaves',
    'explicit-fork-scp-allleafdisjoint-cegarsys2hc_1leaves',
    'explicit-fork-scp-allleafdisjoint-cegarsys2hc_2leaves',
    'explicit-fork-scp-allleafdisjoint-cegarsys2hc_3leaves',
    'explicit-fork-scp-allleafdisjoint-cegarsys2hc_infinityleaves',
    'explicit-ifork-scp-allleafdisjoint-cegarsys2hc_1leaves',
    'explicit-ifork-scp-allleafdisjoint-cegarsys2hc_2leaves',
    'explicit-ifork-scp-allleafdisjoint-cegarsys2hc_3leaves',
    'explicit-ifork-scp-allleafdisjoint-cegarsys2hc_infinityleaves',
    'explicit-mmgeneral-scp-allleafdisjoint-cegarsys2hc_1leaves',
    'explicit-mmgeneral-scp-allleafdisjoint-cegarsys2hc_2leaves',
    'explicit-mmgeneral-scp-allleafdisjoint-cegarsys2hc_3leaves',
    'explicit-mmgeneral-scp-allleafdisjoint-cegarsys2hc_infinityleaves',
],merge=True)

exp.add_fetcher('data/2022-04-05-decoupled-scp-exp-autoscale-eval',filter=[remove_revision],filter_algorithm=[
    'fork-scpexp_prune_noninc_static-all-cegarsys2hc_1leaves',
    'fork-scpexp_prune_noninc_leafid-all-cegarsys2hc_1leaves',
    'fork-scpexp_prune_copy_static-all-cegarsys2hc_1leaves',
    'fork-scpexp_prune_copy_leafid-all-cegarsys2hc_1leaves',
    'fork-scpexp_noprune_noninc_leafid-all-cegarsys2hc_1leaves',
    'ifork-scpexp_prune_noninc_static-all-cegarsys2hc_1leaves',
    'ifork-scpexp_prune_noninc_leafid-all-cegarsys2hc_1leaves',
    'ifork-scpexp_prune_copy_static-all-cegarsys2hc_1leaves',
    'ifork-scpexp_prune_copy_leafid-all-cegarsys2hc_1leaves',
    'ifork-scpexp_noprune_noninc_leafid-all-cegarsys2hc_1leaves',
    'mmgeneral-scpexp_prune_noninc_static-all-cegarsys2hc_1leaves',
    'mmgeneral-scpexp_prune_noninc_leafid-all-cegarsys2hc_1leaves',
    'mmgeneral-scpexp_prune_copy_static-all-cegarsys2hc_1leaves',
    'mmgeneral-scpexp_prune_copy_leafid-all-cegarsys2hc_1leaves',
    'mmgeneral-scpexp_noprune_noninc_leafid-all-cegarsys2hc_1leaves',
    'fork-scpexp_prune_noninc_static-all-cegarsys2hc_2leaves',
    'fork-scpexp_prune_noninc_leafid-all-cegarsys2hc_2leaves',
    'fork-scpexp_prune_copy_static-all-cegarsys2hc_2leaves',
    'fork-scpexp_prune_copy_leafid-all-cegarsys2hc_2leaves',
    'fork-scpexp_noprune_noninc_leafid-all-cegarsys2hc_2leaves',
    'ifork-scpexp_prune_noninc_static-all-cegarsys2hc_2leaves',
    'ifork-scpexp_prune_noninc_leafid-all-cegarsys2hc_2leaves',
    'ifork-scpexp_prune_copy_static-all-cegarsys2hc_2leaves',
    'ifork-scpexp_prune_copy_leafid-all-cegarsys2hc_2leaves',
    'ifork-scpexp_noprune_noninc_leafid-all-cegarsys2hc_2leaves',
    'mmgeneral-scpexp_prune_noninc_static-all-cegarsys2hc_2leaves',
    'mmgeneral-scpexp_prune_noninc_leafid-all-cegarsys2hc_2leaves',
    'mmgeneral-scpexp_prune_copy_static-all-cegarsys2hc_2leaves',
    'mmgeneral-scpexp_prune_copy_leafid-all-cegarsys2hc_2leaves',
    'mmgeneral-scpexp_noprune_noninc_leafid-all-cegarsys2hc_2leaves',
    'fork-scpexp_prune_noninc_static-all-cegarsys2hc_3leaves',
    'fork-scpexp_prune_noninc_leafid-all-cegarsys2hc_3leaves',
    'fork-scpexp_prune_copy_static-all-cegarsys2hc_3leaves',
    'fork-scpexp_prune_copy_leafid-all-cegarsys2hc_3leaves',
    'fork-scpexp_noprune_noninc_leafid-all-cegarsys2hc_3leaves',
    'ifork-scpexp_prune_noninc_static-all-cegarsys2hc_3leaves',
    'ifork-scpexp_prune_noninc_leafid-all-cegarsys2hc_3leaves',
    'ifork-scpexp_prune_copy_static-all-cegarsys2hc_3leaves',
    'ifork-scpexp_prune_copy_leafid-all-cegarsys2hc_3leaves',
    'ifork-scpexp_noprune_noninc_leafid-all-cegarsys2hc_3leaves',
    'mmgeneral-scpexp_prune_noninc_static-all-cegarsys2hc_3leaves',
    'mmgeneral-scpexp_prune_noninc_leafid-all-cegarsys2hc_3leaves',
    'mmgeneral-scpexp_prune_copy_static-all-cegarsys2hc_3leaves',
    'mmgeneral-scpexp_prune_copy_leafid-all-cegarsys2hc_3leaves',
    'mmgeneral-scpexp_noprune_noninc_leafid-all-cegarsys2hc_3leaves',
    'fork-scpexp_prune_noninc_static-all-cegarsys2hc_infinityleaves',
    'fork-scpexp_prune_noninc_leafid-all-cegarsys2hc_infinityleaves',
    'fork-scpexp_prune_copy_static-all-cegarsys2hc_infinityleaves',
    'fork-scpexp_prune_copy_leafid-all-cegarsys2hc_infinityleaves',
    'fork-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves',
    'ifork-scpexp_prune_noninc_static-all-cegarsys2hc_infinityleaves',
    'ifork-scpexp_prune_noninc_leafid-all-cegarsys2hc_infinityleaves',
    'ifork-scpexp_prune_copy_static-all-cegarsys2hc_infinityleaves',
    'ifork-scpexp_prune_copy_leafid-all-cegarsys2hc_infinityleaves',
    'ifork-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves',
    'mmgeneral-scpexp_prune_noninc_static-all-cegarsys2hc_infinityleaves',
    'mmgeneral-scpexp_prune_noninc_leafid-all-cegarsys2hc_infinityleaves',
    'mmgeneral-scpexp_prune_copy_static-all-cegarsys2hc_infinityleaves',
    'mmgeneral-scpexp_prune_copy_leafid-all-cegarsys2hc_infinityleaves',
    'mmgeneral-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves',
],merge=True)

exp.add_fetcher('data/2022-04-05-decoupled-scp-approx-autocsale-eval',filter=[remove_revision],filter_algorithm=[
    'fork-scpapprox-allleafdisjoint-cegarsys2hc_1leaves',
    'fork-scpapproxpoly-allleafdisjoint-cegarsys2hc_1leaves',
    'ifork-scpapprox-allleafdisjoint-cegarsys2hc_1leaves',
    'ifork-scpapproxpoly-allleafdisjoint-cegarsys2hc_1leaves',
    'mmgeneral-scpapprox-allleafdisjoint-cegarsys2hc_1leaves',
    'mmgeneral-scpapproxpoly-allleafdisjoint-cegarsys2hc_1leaves',
    'fork-scpapprox-allleafdisjoint-cegarsys2hc_2leaves',
    'fork-scpapproxpoly-allleafdisjoint-cegarsys2hc_2leaves',
    'ifork-scpapprox-allleafdisjoint-cegarsys2hc_2leaves',
    'ifork-scpapproxpoly-allleafdisjoint-cegarsys2hc_2leaves',
    'mmgeneral-scpapprox-allleafdisjoint-cegarsys2hc_2leaves',
    'mmgeneral-scpapproxpoly-allleafdisjoint-cegarsys2hc_2leaves',
    'fork-scpapprox-allleafdisjoint-cegarsys2hc_3leaves',
    'fork-scpapproxpoly-allleafdisjoint-cegarsys2hc_3leaves',
    'ifork-scpapprox-allleafdisjoint-cegarsys2hc_3leaves',
    'ifork-scpapproxpoly-allleafdisjoint-cegarsys2hc_3leaves',
    'mmgeneral-scpapprox-allleafdisjoint-cegarsys2hc_3leaves',
    'mmgeneral-scpapproxpoly-allleafdisjoint-cegarsys2hc_3leaves',
    'fork-scpapprox-allleafdisjoint-cegarsys2hc_infinityleaves',
    'fork-scpapproxpoly-allleafdisjoint-cegarsys2hc_infinityleaves',
    'ifork-scpapprox-allleafdisjoint-cegarsys2hc_infinityleaves',
    'ifork-scpapproxpoly-allleafdisjoint-cegarsys2hc_infinityleaves',
    'mmgeneral-scpapprox-allleafdisjoint-cegarsys2hc_infinityleaves',
    'mmgeneral-scpapproxpoly-allleafdisjoint-cegarsys2hc_infinityleaves',
],merge=True)

exp.add_fetcher('data/2022-04-05-decoupled-scp-singleleaf-autoscale-eval',filter=[remove_revision],filter_algorithm=[
    'fork-scpsingleleaf-all-cegarsys2hc_1leaves',
    'ifork-scpsingleleaf-all-cegarsys2hc_1leaves',
    'mmgeneral-scpsingleleaf-all-cegarsys2hc_1leaves',
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

        f'explicit-{name}-scp-all-cegarsys2hc_1leaves',
        f'explicit-{name}-scp-all-cegarsys2hc_2leaves',
        f'explicit-{name}-scp-all-cegarsys2hc_3leaves',
        f'explicit-{name}-scp-all-cegarsys2hc_infinityleaves',
        'explicit-scp-all-cegarsys2hc',
        f'explicit-{name}-scp-allleafdisjoint-cegarsys2hc_1leaves',
        f'explicit-{name}-scp-allleafdisjoint-cegarsys2hc_2leaves',
        f'explicit-{name}-scp-allleafdisjoint-cegarsys2hc_3leaves',
        f'explicit-{name}-scp-allleafdisjoint-cegarsys2hc_infinityleaves',

        f'{name}-scpapprox-allleafdisjoint-cegarsys2hc_1leaves',
        f'{name}-scpapproxpoly-allleafdisjoint-cegarsys2hc_1leaves',
        f'{name}-scpapprox-allleafdisjoint-cegarsys2hc_2leaves',
        f'{name}-scpapproxpoly-allleafdisjoint-cegarsys2hc_2leaves',
        f'{name}-scpapprox-allleafdisjoint-cegarsys2hc_3leaves',
        f'{name}-scpapproxpoly-allleafdisjoint-cegarsys2hc_3leaves',
        f'{name}-scpapprox-allleafdisjoint-cegarsys2hc_infinityleaves',
        f'{name}-scpapproxpoly-allleafdisjoint-cegarsys2hc_infinityleaves',

        f'{name}-scpsingleleaf-all-cegarsys2hc_1leaves',

        f'{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_1leaves',
        f'{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_2leaves',
        f'{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_3leaves',
        f'{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves',
        f'{name}-scpexp_prune_noninc_static-all-cegarsys2hc_1leaves',
        f'{name}-scpexp_prune_noninc_static-all-cegarsys2hc_2leaves',
        f'{name}-scpexp_prune_noninc_static-all-cegarsys2hc_3leaves',
        f'{name}-scpexp_prune_noninc_static-all-cegarsys2hc_infinityleaves',
        f'{name}-scpexp_prune_noninc_leafid-all-cegarsys2hc_1leaves',
        f'{name}-scpexp_prune_noninc_leafid-all-cegarsys2hc_2leaves',
        f'{name}-scpexp_prune_noninc_leafid-all-cegarsys2hc_3leaves',
        f'{name}-scpexp_prune_noninc_leafid-all-cegarsys2hc_infinityleaves',
        f'{name}-scpexp_prune_copy_static-all-cegarsys2hc_1leaves',
        f'{name}-scpexp_prune_copy_static-all-cegarsys2hc_2leaves',
        f'{name}-scpexp_prune_copy_static-all-cegarsys2hc_3leaves',
        f'{name}-scpexp_prune_copy_static-all-cegarsys2hc_infinityleaves',
        f'{name}-scpexp_prune_copy_leafid-all-cegarsys2hc_1leaves',
        f'{name}-scpexp_prune_copy_leafid-all-cegarsys2hc_2leaves',
        f'{name}-scpexp_prune_copy_leafid-all-cegarsys2hc_3leaves',
        f'{name}-scpexp_prune_copy_leafid-all-cegarsys2hc_infinityleaves',
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
        'explicit-scp-all-cegarsys2hc',
        f'explicit-{name}-scp-all-cegarsys2hc_infinityleaves',
        f'explicit-{name}-scp-allleafdisjoint-cegarsys2hc_infinityleaves',
        f'explicit-{name}-scp-all-cegarsys2hc_1leaves',
        f'{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves',
        f'{name}-scpexp_prune_noninc_leafid-all-cegarsys2hc_infinityleaves',
        f'{name}-scpexp_prune_copy_leafid-all-cegarsys2hc_infinityleaves',
        f'{name}-scpexp_prune_copy_static-all-cegarsys2hc_infinityleaves',
        f'{name}-scpapproxpoly-allleafdisjoint-cegarsys2hc_infinityleaves',
        f'{name}-scpsingleleaf-all-cegarsys2hc_1leaves',
    ]
    f = Filter(name, algorithms)
    report_name=f'{exp.name}-absolute-{name}.{HTML_OR_TEX}'
    report_file=Path(exp.eval_dir) / f'{report_name}'
    exp.add_report(
        AbsoluteReport(
            attributes=['coverage'],
            filter_algorithm=algorithms,
            filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            format=HTML_OR_TEX,
        ),
        name=report_name,
        outfile=report_file,
    )

PNG_OR_TEX='tex'
for name in DECOUPLINGS:
    report_name=f'{exp.name}-plot-expansions-explicit-exp-vs-leafdisjoint-{name}.{PNG_OR_TEX}'
    report_file=Path(exp.eval_dir) / f'{report_name}'
    exp.add_report(
        ScatterPlotReport(
            attributes='expansions',
            filter_algorithm=[
                f'explicit-{name}-scp-all-cegarsys2hc_infinityleaves',
                f'explicit-{name}-scp-allleafdisjoint-cegarsys2hc_infinityleaves',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            # get_category=lambda run1, run2: run1['domain'],
            format=PNG_OR_TEX,
        ),
        name=report_name,
        outfile=report_file,
    )
    report_name=f'{exp.name}-plot-expansions-explicit-exp-vs-singleleaf-{name}.{PNG_OR_TEX}'
    report_file=Path(exp.eval_dir) / f'{report_name}'
    exp.add_report(
        ScatterPlotReport(
            attributes='expansions',
            filter_algorithm=[
                f'explicit-{name}-scp-all-cegarsys2hc_infinityleaves',
                f'explicit-{name}-scp-all-cegarsys2hc_1leaves',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            # get_category=lambda run1, run2: run1['domain'],
            format=PNG_OR_TEX,
        ),
        name=report_name,
        outfile=report_file,
    )

PNG_OR_TEX='tex'
for name in DECOUPLINGS:
    report_name=f'{exp.name}-plot-expansions-decoupled-exp-vs-leafdisjoint-{name}.{PNG_OR_TEX}'
    report_file=Path(exp.eval_dir) / f'{report_name}'
    exp.add_report(
        ScatterPlotReport(
            attributes='expansions',
            filter_algorithm=[
                f'{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves',
                f'{name}-scpapproxpoly-allleafdisjoint-cegarsys2hc_infinityleaves',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            # get_category=lambda run1, run2: run1['domain'],
            format=PNG_OR_TEX,
        ),
        name=report_name,
        outfile=report_file,
    )
    report_name=f'{exp.name}-plot-expansions-decoupled-exp-vs-singleleaf-{name}.{PNG_OR_TEX}'
    report_file=Path(exp.eval_dir) / f'{report_name}'
    exp.add_report(
        ScatterPlotReport(
            attributes='expansions',
            filter_algorithm=[
                f'{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves',
                f'{name}-scpsingleleaf-all-cegarsys2hc_1leaves',
            ],
            # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            # get_category=lambda run1, run2: run1['domain'],
            format=PNG_OR_TEX,
        ),
        name=report_name,
        outfile=report_file,
    )
PNG_OR_TEX='tex'
name = 'mmgeneral'
report_name=f'{exp.name}-plot-searchtime-decoupled-exp-vs-leafdisjoint-{name}.{PNG_OR_TEX}'
report_file=Path(exp.eval_dir) / f'{report_name}'
exp.add_report(
    ScatterPlotReport(
        attributes='search_time',
        filter_algorithm=[
            f'{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves',
            f'{name}-scpapproxpoly-allleafdisjoint-cegarsys2hc_infinityleaves',
        ],
        # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
        # get_category=lambda run1, run2: run1['domain'],
        format=PNG_OR_TEX,
    ),
    name=report_name,
    outfile=report_file,
)
report_name=f'{exp.name}-plot-searchtime-decoupled-exp-vs-singleleaf-{name}.{PNG_OR_TEX}'
report_file=Path(exp.eval_dir) / f'{report_name}'
exp.add_report(
    ScatterPlotReport(
        attributes='search_time',
        filter_algorithm=[
            f'{name}-scpexp_noprune_noninc_leafid-all-cegarsys2hc_infinityleaves',
            f'{name}-scpsingleleaf-all-cegarsys2hc_1leaves',
        ],
        # filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
        # get_category=lambda run1, run2: run1['domain'],
        format=PNG_OR_TEX,
    ),
    name=report_name,
    outfile=report_file,
)

exp.run_steps()
