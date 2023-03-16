#! /usr/bin/env python3

import itertools
import math
import os
from pathlib import Path
import subprocess

from lab.reports import Attribute, geometric_mean, arithmetic_mean

from downward.experiment import FastDownwardExperiment

from downward.reports.absolute import AbsoluteReport
from downward.reports.scatter import ScatterPlotReport

DIR = os.path.dirname(os.path.abspath(__file__))
SCRIPT_NAME = os.path.splitext(os.path.basename(__file__))[0]
REVISIONS=[
    '5d086b8eeb583fa0e91e8220b05e0699c16719ae', # 2022-11-24: all symbolic PDB configs
    '80cd5a3626706bf47def05f53d941ff28fae180c', # 2023-02-22: all MS configs
    'a657826557ddeee8ab70162265e932c0949f6b8b', # 2023-03-06: random linear MS configs
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

exp.add_fetcher('data/2022-11-24-gamer-pdbs-all-eval',filter=[remove_revision],filter_algorithm=[
    "gamerpdbs-explicit",
    "lpmm-gamerpdbs-compliant-explicit",
    "lpmm-gamerpdbs-explistate",
    "lpmm-gamerpdbs-compliant-explistate",
    "lpmm-gamerpdbs-addops",
    "lpmm-gamerpdbs-compliant-addops",
    "lpmm-gamerpdbs-compliant-recursive",
])

exp.add_fetcher('data/2023-02-22-icaps23-crc-ms-eval',filter=[remove_revision],filter_algorithm=[
    "b50k-sbmiasm-explicit",
    "lpmm-b50k-compliantsbmiasm-explicit",
    "lpmm-b50k-sbmiasm-explistate",  
    "lpmm-b50k-compliantsbmiasm-explistate",
    "lpmm-b50k-sbmiasm-exact",
    "lpmm-b50k-compliantsbmiasm-exact",
],merge=True)

exp.add_fetcher('data/2023-03-06-linear-random-compliant-merge-eval',filter=[remove_revision],filter_algorithm=[
    "lpmm-b50k-linearrand-exact",
    "lpmm-b50k-compliantlinearrand-exact",
],merge=True)


HTML_OR_TEX = 'tex'

class Filter:
    def __init__(self, dec, algos):
        self.decoupling = dec
        self.algos = algos
        self.excluded_tasks = set()
        self.strongly_compliant_tasks = set()

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


algorithms=[
        f'b50k-sbmiasm-explicit',
        f'lpmm-b50k-compliantsbmiasm-explicit',
        f'lpmm-b50k-sbmiasm-explistate',
        f'lpmm-b50k-compliantsbmiasm-explistate',
        f'lpmm-b50k-sbmiasm-exact',
        f'lpmm-b50k-compliantsbmiasm-exact',
        f'gamerpdbs-explicit',
        f'lpmm-gamerpdbs-compliant-explicit',
        f'lpmm-gamerpdbs-explistate',
        f'lpmm-gamerpdbs-compliant-explistate',
        f'lpmm-gamerpdbs-addops',
        f'lpmm-gamerpdbs-compliant-addops',
        f'lpmm-gamerpdbs-compliant-recursive',
]
factoring = "lpmm"
f = Filter(factoring, algorithms)
report_name=f'{exp.name}-absolute-{factoring}.{HTML_OR_TEX}'
report_file=Path(exp.eval_dir) / f'{report_name}'
exp.add_report(
        AbsoluteReport(
            attributes=["coverage"],
            filter_algorithm=algorithms,
            filter=[f.collect_non_factorable_tasks,f.filter_interesting_tasks],
            format=HTML_OR_TEX,
        ),
        name=report_name,
        outfile=report_file,
)


PLOT_CONFIG_PAIRS_BY_ATTRIBUTE = {
    "search_time" :   [["lpmm-b50k-sbmiasm-exact",     "lpmm-b50k-compliantsbmiasm-exact"],
                       ["lpmm-b50k-linearrand-exact",  "lpmm-b50k-compliantlinearrand-exact"],
                       ["lpmm-gamerpdbs-addops",       "lpmm-gamerpdbs-compliant-recursive"]],
    "evaluations" :   [["lpmm-b50k-linearrand-exact",  "lpmm-b50k-compliantlinearrand-exact"]]
}


PNG_OR_TEX='tex'
for attr in PLOT_CONFIG_PAIRS_BY_ATTRIBUTE.keys():
    for c1, c2 in PLOT_CONFIG_PAIRS_BY_ATTRIBUTE[attr]:
        report_name=f'{exp.name}-plot-{attr}-{c1}-vs-{c2}.{PNG_OR_TEX}'
        report_file=Path(exp.eval_dir) / f'{report_name}'
        exp.add_report(
            ScatterPlotReport(
                attributes=attr,
                filter_algorithm=[c1, c2],
                get_category=lambda run1, run2: str(run1['ms_strongly_compliant']) if "linearrand-exact" in run1["algorithm"] else "",
                format=PNG_OR_TEX,
                show_missing=False,
            ),
            name=report_name,
            outfile=report_file,
        )


exp.run_steps()
