# -*- coding: utf-8 -*-

import os
import subprocess
import sys

from collections import defaultdict

from lab import tools

from lab.reports import Attribute, geometric_mean, arithmetic_mean


DEFAULT_OPTIMAL_SUITE = [
    'agricola-opt18-strips', 'airport', 'barman-opt11-strips',
    'barman-opt14-strips', 'blocks', 'childsnack-opt14-strips',
    'data-network-opt18-strips', 'depot', 'driverlog',
    'elevators-opt08-strips', 'elevators-opt11-strips',
    'floortile-opt11-strips', 'floortile-opt14-strips', 'freecell',
    'ged-opt14-strips', 'grid', 'gripper', 'hiking-opt14-strips',
    'logistics00', 'logistics98', 'miconic', 'movie', 'mprime',
    'mystery', 'nomystery-opt11-strips', 'openstacks-opt08-strips',
    'openstacks-opt11-strips', 'openstacks-opt14-strips',
    'openstacks-strips', 'organic-synthesis-opt18-strips',
    'organic-synthesis-split-opt18-strips', 'parcprinter-08-strips',
    'parcprinter-opt11-strips', 'parking-opt11-strips',
    'parking-opt14-strips', 'pathways', 'pegsol-08-strips',
    'pegsol-opt11-strips', 'petri-net-alignment-opt18-strips',
    'pipesworld-notankage', 'pipesworld-tankage', 'psr-small', 'rovers',
    'satellite', 'scanalyzer-08-strips', 'scanalyzer-opt11-strips',
    'snake-opt18-strips', 'sokoban-opt08-strips',
    'sokoban-opt11-strips', 'spider-opt18-strips', 'storage',
    'termes-opt18-strips', 'tetris-opt14-strips',
    'tidybot-opt11-strips', 'tidybot-opt14-strips', 'tpp',
    'transport-opt08-strips', 'transport-opt11-strips',
    'transport-opt14-strips', 'trucks-strips', 'visitall-opt11-strips',
    'visitall-opt14-strips', 'woodworking-opt08-strips',
    'woodworking-opt11-strips', 'zenotravel']

DEFAULT_SATISFICING_SUITE = [
    'agricola-sat18-strips', 'airport',
    'barman-sat11-strips', 'barman-sat14-strips', 'blocks',
    'childsnack-sat14-strips', 'data-network-sat18-strips', 'depot', 'driverlog',
    'elevators-sat08-strips', 'elevators-sat11-strips',
    'floortile-sat11-strips', 'floortile-sat14-strips', 'freecell',
    'folding-sat23-norm', 'ged-sat14-strips', 'grid',
    'gripper', 'hiking-sat14-strips', 'logistics00', 'logistics98',
    'maintenance-sat14-adl', 'miconic', 'movie', 'mprime', 'mystery',
    'nomystery-sat11-strips',
    'openstacks-sat08-strips', 'openstacks-sat11-strips', 'openstacks-sat14-strips',
    'openstacks-strips', 'organic-synthesis-sat18-strips',
    'organic-synthesis-split-sat18-strips', 'parcprinter-08-strips',
    'parcprinter-sat11-strips', 'parking-sat11-strips',
    'parking-sat14-strips', 'pathways',
    'pegsol-08-strips', 'pegsol-sat11-strips',
    'pipesworld-notankage', 'pipesworld-tankage',
    'psr-small', 'quantum-layout-sat23-strips', 'rovers',
    'recharging-robots-sat23-norm',
    'ricochet-robots-sat23-adl', 'rubiks-cube-sat23-norm', 'satellite',
    'scanalyzer-08-strips', 'scanalyzer-sat11-strips', 'schedule',
    'slitherlink-sat23-norm',
    'snake-sat18-strips', 'sokoban-sat08-strips',
    'sokoban-sat11-strips', 'spider-sat18-strips', 'storage',
    'termes-sat18-strips', 'tetris-sat14-strips',
    'thoughtful-sat14-strips', 'tidybot-sat11-strips', 'tpp',
    'transport-sat08-strips', 'transport-sat11-strips',
    'transport-sat14-strips', 'trucks-strips',
    'visitall-sat11-strips', 'visitall-sat14-strips',
    'woodworking-sat08-strips', 'woodworking-sat11-strips',
    'zenotravel']

DEFAULT_UNSOLVABILITY_SUITE = [
    "bag-barman", "bag-gripper", "bag-transport", "bottleneck", "cave-diving",
    "chessboard-pebbling", "diagnosis", "document-transfer", "over-nomystery",
    "over-rovers", "over-tpp", "pegsol", "pegsol-row5", "sliding-tiles", "tetris"
]

ATTRIBUTES = [
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
    "run_dir",
    "search_time",
    "total_time",

    Attribute('search_out_of_memory', absolute=True, min_wins=True),
    Attribute('search_out_of_time', absolute=True, min_wins=True),

    # TODO possibly define new attributes here; you likely need to add a new parser for new attributes

]


def get_script():
    """Get file name of main script."""
    return tools.get_script_path()


def get_script_dir():
    """Get directory of main script.

    Usually a relative directory (depends on how it was called by the user.)"""
    return os.path.dirname(get_script())


def get_experiment_name():
    """Get name for experiment.

    Derived from the absolute filename of the main script, e.g.
    "/ham/spam/eggs.py" => "spam-eggs"."""
    script = os.path.abspath(get_script())
    #script_dir = os.path.basename(os.path.dirname(script))
    script_base = os.path.splitext(os.path.basename(script))[0]
    #return "%s-%s" % (script_dir, script_base)
    return script_base


def get_data_dir():
    """Get data dir for the experiment.

    This is the subdirectory "data" of the directory containing
    the main script."""
    return os.path.join(get_script_dir(), "data", get_experiment_name())


def get_repo_base():
    """Get base directory of the repository, as an absolute path.

    Search upwards in the directory tree from the main script until a
    directory with a subdirectory named ".git" is found.

    Abort if the repo base cannot be found."""
    path = os.path.abspath(get_script_dir())
    while os.path.dirname(path) != path:
        if os.path.exists(os.path.join(path, ".git")):
            return path
        path = os.path.dirname(path)
    sys.exit("repo base could not be found")


def get_algo_nick(revision, config_nick):
    return f"{revision}-{config_nick}"


def add_compress_and_delete_runs_step(exp):
    runs_dir = exp.path
    exp.add_step(
        "compress-runs-dir",
        subprocess.call,
        [
            "tar",
            "-cjf",
            f"{runs_dir[:-1] if runs_dir.endswith('/') else runs_dir}.tar.gz",
            f"{runs_dir if runs_dir.endswith('/') else runs_dir + '/'}",
        ],
    )
    exp.add_step(
        "delete-runs-dir",
        subprocess.call,
        [
            "rm",
            "-rf",
            runs_dir
        ],
    )
