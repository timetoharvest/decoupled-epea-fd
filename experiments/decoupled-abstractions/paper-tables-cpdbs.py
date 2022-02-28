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
    '1d424e48c3ed22f69e30fd1e843a02e3c44535ed', # 2022-01-20
    'eaa07a7dcbee6c6f54252254cfd32aaf32b409fc', # 2022-02-16
    '6dcab07d143bdad32cb2c151c648c377207ed2f6', # 2022-02-17
    'ef5fdfb3514989c44f66bc16aededaf2fa301b36', # 2022-02-28
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

def filter_lmcut_blind(run):
    return 'blind' in run['algorithm'] or 'lmcut' in run['algorithm']

def filter_explicit_cdpbsc(run):
    return 'explicit-cpdbs-hc' in run['algorithm'] or 'explicit-cpdbs-hcsingleleaf' in run['algorithm']

def filter_exphc(run):
    return 'cpdbsexp-hc' in run['algorithm']

def filter_exphcsingleleaf(run):
    return 'cpdbsexp-hcsingleleaf' in run['algorithm']



exp = FastDownwardExperiment()
# blind, lmcut
# exp.add_fetcher('data/2022-01-11-baseline-decoupled-eval',filter=[remove_revision])

# blind, lmcut
# exp.add_fetcher('data/2022-01-11-baseline-explicit-eval',filter=[remove_revision,filter_lmcut_blind],merge=True)

# explicit-cpdbs-hc, explicit-cpdbs-hcsingleleaf
exp.add_fetcher('data/2022-01-11-baseline-explicit-cpdbs-eval',filter=[remove_revision,filter_explicit_cdpbsc],merge=True)

# cpdbsapprox, cpdbssingleleaf, cpdbsexp
exp.add_fetcher('data/2022-01-11-cpdbs-eval',filter=[remove_revision,filter_exphc],merge=True)

# cpdbsexp-hcsingleleaf
exp.add_fetcher('data/2022-01-20-cpdbs-more-exp-eval',filter=[remove_revision,filter_exphcsingleleaf],merge=True)

exp.add_fetcher('data/2022-02-16-explicit-decoupled-cpdbs-eval',filter=[remove_revision],merge=True)
exp.add_fetcher('data/2022-02-16-decoupled-cpdbs-exp-eval',filter=[remove_revision],merge=True)
exp.add_fetcher('data/2022-02-16-decoupled-cpdbs-expilp-eval',filter=[remove_revision],merge=True)
exp.add_fetcher('data/2022-02-17-decoupled-cpdbs-expilp-lp-eval',filter=[remove_revision],merge=True)
exp.add_fetcher('data/2022-02-17-decoupled-cpdbs-expilp-abslp-eval',filter=[remove_revision],merge=True)
exp.add_fetcher('data/2022-02-17-decoupled-cpdbs-expilp-leaflp-eval',filter=[remove_revision],merge=True)
exp.add_fetcher('data/2022-02-28-decoupled-cpdbs-approx-eval',filter=[remove_revision],merge=True)

DECOUPLINGS = [
    'fork',
    'ifork',
    'mmstrict',
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
        # f'{name}-explicit-cpdbs-hcleafdisjoint',
        f'{name}-explicit-cpdbs-hcsingleleaf',
        f'explicit-{name}-cpdbs-hc2leaves',
        f'explicit-{name}-cpdbs-hc3leaves',
        f'{name}-explicit-cpdbs-hc',
        f'{name}-cpdbsexp-hcsingleleaf',
        f'{name}-cpdbsexp-hc2leaves',
        f'{name}-cpdbsexp-hc3leaves',
        f'{name}-cpdbsexp-hc',
        f'{name}-cpdbsexpilp-hc1leaf',
        f'{name}-cpdbsexpilp-hc2leaves',
        f'{name}-cpdbsexpilp-hc3leaves',
        f'{name}-cpdbsexpilp-hc',
        f'{name}-cpdbsexpilp_abslp-hc1leaf',
        f'{name}-cpdbsexpilp_abslp-hc2leaves',
        f'{name}-cpdbsexpilp_abslp-hc3leaves',
        f'{name}-cpdbsexpilp_abslp-hc',
        f'{name}-cpdbsexpilp_leaflp-hc1leaf',
        f'{name}-cpdbsexpilp_leaflp-hc2leaves',
        f'{name}-cpdbsexpilp_leaflp-hc3leaves',
        f'{name}-cpdbsexpilp_leaflp-hc',
        f'{name}-cpdbsexpilp_lp-hc1leaf',
        f'{name}-cpdbsexpilp_lp-hc2leaves',
        f'{name}-cpdbsexpilp_lp-hc3leaves',
        f'{name}-cpdbsexpilp_lp-hc',
        f'{name}-cpdbsapprox-hc',
        f'{name}-cpdbsapprox-hcdisjointleaf',
    ]
    f = Filter(name, algorithms)
    report_name=f'{exp.name}-absolute-baseline-exp-expilp-{name}'
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
