#! /usr/bin/env python3

import math
import os
import re
import shlex
import subprocess
from pathlib import Path

from lab.environments import LocalEnvironment
from lab.parser import Parser
from lab.reports import Attribute, arithmetic_mean, geometric_mean

from downward import suites
from downward.experiment import FastDownwardExperiment
from downward.reports.absolute import AbsoluteReport
from downward.reports.compare import ComparativeReport
from downward.reports.scatter import ScatterPlotReport

import common_setup
import pea_parser
from bw_cluster_environments import BWUniEnvironment, HelixEnvironment

DIR = Path(__file__).resolve().parent
SCRIPT_NAME = Path(__file__).stem

REPO = os.environ.get("DOWNWARD_REPO", str(DIR.parent))
BENCHMARKS_DIR = os.environ.get(
    "DOWNWARD_BENCHMARKS",
    str(DIR.parent.parent.parent / "downward-benchmarks"))

REV = os.environ.get("DOWNWARD_REV", "4c77017cd896f416ab0329680374793135e7c26c")

BUILD = os.environ.get("EVAL_BUILD", "release64")
BUILD_JOBS = os.environ.get("EVAL_BUILD_JOBS", "8")
DATA_DIR = Path(os.environ.get("EVAL_DATA_DIR", str(DIR / "data")))
EMAIL = os.environ.get("EVAL_EMAIL")
PILOT = os.environ.get("EVAL_PILOT") == "1"
KEEP_RUNS = os.environ.get("EVAL_KEEP_RUNS") == "1"
ONLY_TASKS = [t for t in os.environ.get("EVAL_ONLY_TASKS", "").split(",") if t]
ONLY_ROWS = [r for r in os.environ.get("EVAL_ONLY_ROWS", "").split(",") if r]
RERUN = bool(ONLY_TASKS or ONLY_ROWS)
STDOUT_LIMITS = None
if os.environ.get("EVAL_STDOUT_LIMITS"):
    parts = os.environ["EVAL_STDOUT_LIMITS"].split(",")
    if len(parts) != 2 or not all(x.isdigit() for x in parts) \
            or int(parts[0]) > int(parts[1]):
        raise SystemExit("EVAL_STDOUT_LIMITS must be soft,hard in KiB with "
                         "soft <= hard, for example 51200,204800")
    STDOUT_LIMITS = (int(parts[0]), int(parts[1]))

LOCAL_PROCESSES = min(int(os.environ.get("EVAL_LOCAL_PROCESSES", "0") or 0),
                      os.cpu_count() or 1)
STAGE = os.environ.get("EVAL_STAGE", "")
if STAGE not in ("", "runs", "reports", "both"):
    raise SystemExit("EVAL_STAGE must be runs, reports or both")

TABLE_FORMATS = os.environ.get("EVAL_REPORT_FORMATS", "tex,html").split(",")
SCATTER_FORMATS = os.environ.get("EVAL_SCATTER_FORMATS", "png,tex").split(",")

for _what, _path, _var in (("planner repository", REPO, "DOWNWARD_REPO"),
                           ("benchmark directory", BENCHMARKS_DIR,
                            "DOWNWARD_BENCHMARKS")):
    if not os.path.isdir(_path):
        raise SystemExit(
            f"{_what} not found at {_path}. Set {_var} to the correct path.")

CLUSTER_ENV_CLASS = next(
    (cls for cls in (BWUniEnvironment, HelixEnvironment)
     if cls.is_present()), None)
REMOTE = CLUSTER_ENV_CLASS is not None
if REMOTE and not os.environ.get("EVAL_DATA_DIR"):
    raise SystemExit(
        "set EVAL_DATA_DIR to a workspace path (CLUSTER.md section 2): the "
        "full runs create about 1.4 million files, too many for $HOME")
if not STAGE:
    STAGE = "runs" if REMOTE and not LOCAL_PROCESSES else "both"
RUNS_STAGE = STAGE in ("runs", "both")
REPORTS_STAGE = STAGE in ("reports", "both")

CATEGORIES = {
    "explicit": "explicit search: A*, PEA*, EPEA*",
    "dec-on": "decoupled search with the g-value adaptation: A*, PEA*",
    "dec-off": "decoupled search without the g-value adaptation: "
               "A*, PEA*, EPEA*",
}
CATEGORY = os.environ.get("EVAL_CATEGORY", "")
if CATEGORY not in CATEGORIES:
    raise SystemExit(
        "set EVAL_CATEGORY to one of " + ", ".join(CATEGORIES)
        + " (one experiment per category, the supervisor's design)")

DEFAULT_HEURISTICS = ["hmax", "pdb"]
KNOWN_HEURISTICS = ("blind", "hmax", "lmcut", "pdb")
HEURISTIC_SET = [h for h in os.environ.get(
    "EVAL_HEURISTICS", ",".join(DEFAULT_HEURISTICS)).split(",") if h]
if not HEURISTIC_SET or any(h not in KNOWN_HEURISTICS for h in HEURISTIC_SET):
    raise SystemExit("EVAL_HEURISTICS must be a comma-separated subset of "
                     + ", ".join(KNOWN_HEURISTICS))
HEURISTIC_SUFFIX = "" if HEURISTIC_SET == DEFAULT_HEURISTICS \
    else "-" + "+".join(HEURISTIC_SET)
RUN_PEA0 = True
RUN_FULLCHECK = True
MIN_LEAVES = 2
FORK_IN_BOTH_ARMS = True

DECOUPLED_PDB_ROWS = "plain"
DEFAULT_FACTORINGS = ["fork", "ifork", "incarcs"]
FACTORINGS = [f for f in os.environ.get(
    "EVAL_FACTORINGS", ",".join(DEFAULT_FACTORINGS)).split(",") if f]
KNOWN_FACTORINGS = ("fork", "ifork", "incarcs", "lp_general", "xshape")
if not FACTORINGS or any(f not in KNOWN_FACTORINGS for f in FACTORINGS):
    raise SystemExit("EVAL_FACTORINGS must be a comma-separated subset of "
                     + ", ".join(KNOWN_FACTORINGS))

FACTORING_SUFFIX = "" if FACTORINGS == DEFAULT_FACTORINGS \
    else "-" + "+".join(FACTORINGS)
if "lp_general" in FACTORINGS and REMOTE and RUNS_STAGE \
        and os.environ.get("EVAL_LP_SOLVER_OK") != "1":
    raise SystemExit(
        "lp_general needs a CPLEX build (every run exits 32 otherwise). "
        "Set EVAL_LP_SOLVER_OK=1 only after `grep HAS_CPLEX` in flags.make "
        "succeeds and the decoupled gates pass with it")

LP_GENERAL_OPTIONS = ("factoring_time_limit=30, memory_limit=4000, "
                      "add_cg_sccs=true, strategy=mm_approx, "
                      "min_flexibility=0.8")

PLAIN_PDB = "pdb(pattern=greedy(max_states=1000000))"
CENTER_PDB = ("pdb(pattern=greedy(max_states=1000000, "
              "pattern_filter=n_leaves_patterns(n=0)))")
HEURISTICS = {h: f"{h}()" for h in HEURISTIC_SET if h != "pdb"}

def decoupling(factoring, arm):
    opts = [f"min_num_leaves={MIN_LEAVES}"]
    if factoring == "lp_general":
        opts.append(LP_GENERAL_OPTIONS)
    if arm == "off":
        opts.append("pruning=dominance(include_price_in_g=false)")
    return f"{factoring}({', '.join(opts)})"

def engine_rows(prefix, hstr, dec):
    pre = ["--decoupling", dec] if dec else []
    rows = {f"{prefix}-astar": pre + ["--search", f"astar({hstr})"]}
    if RUN_PEA0:
        rows[f"{prefix}-pea0"] = pre + [
            "--search", f"pea_astar({hstr}, partial_expansion=false)"]
    rows[f"{prefix}-pea"] = pre + ["--search", f"pea_astar({hstr})"]
    return rows

def epea_rows(prefix, pdb, dec):
    pre = ["--decoupling", dec] if dec else []
    rows = {f"{prefix}-epea": pre + ["--search", f"epea_astar({pdb})"]}
    if RUN_FULLCHECK:
        rows[f"{prefix}-epeafc"] = pre + [
            "--search", f"pea_astar({pdb}, osf=full_checking)"]
    return rows

def build_configs(category):
    configs = {}
    with_pdb = "pdb" in HEURISTIC_SET
    if category == "explicit":
        heuristics = dict(HEURISTICS, **({"pdb": PLAIN_PDB} if with_pdb else {}))
        for h, hstr in heuristics.items():
            for fam, opts in engine_rows("x", hstr, None).items():
                configs[f"exp-{fam[2:]}-{h}"] = opts
        if with_pdb:
            for fam, opts in epea_rows("x", PLAIN_PDB, None).items():
                configs[f"exp-{fam[2:]}-pdb"] = opts
        return configs
    arm = category[len("dec-"):]
    heuristics = dict(HEURISTICS, **({"pdb": CENTER_PDB if DECOUPLED_PDB_ROWS == "center"
                                      else PLAIN_PDB} if with_pdb else {}))
    for factoring in FACTORINGS:
        if factoring == "fork" and arm == "on" and not FORK_IN_BOTH_ARMS:
            continue
        dec = decoupling(factoring, arm)
        for h, hstr in heuristics.items():
            for fam, opts in engine_rows("x", hstr, dec).items():
                configs[f"dec-{factoring}-{arm}-{fam[2:]}-{h}"] = opts
        if arm == "off" and with_pdb:
            for fam, opts in epea_rows("x", CENTER_PDB, dec).items():
                configs[f"dec-{factoring}-off-{fam[2:]}-pdb"] = opts
    return configs

SUITE = sorted(set(common_setup.DEFAULT_OPTIMAL_SUITE)
               | {"quantum-layout-opt23-strips"})

TEST_SUITE = ["logistics00:probLOGISTICS-4-0.pddl", "rovers:p03.pddl",
              "woodworking-opt08-strips:p01.pddl"]
THIRD = next((f for f in FACTORINGS if f not in ("fork", "ifork")), "ifork")
TEST_NICKS = {
    "explicit": [
        "exp-astar-hmax", "exp-pea0-hmax", "exp-pea-hmax",
        "exp-astar-pdb", "exp-pea0-pdb", "exp-pea-pdb",
        "exp-epea-pdb", "exp-epeafc-pdb"],
    "dec-on": [
        "dec-ifork-on-astar-hmax", "dec-ifork-on-pea0-hmax",
        "dec-ifork-on-pea-hmax", "dec-ifork-on-astar-pdb",
        "dec-ifork-on-pea-pdb", "dec-fork-on-astar-hmax",
        "dec-fork-on-pea-hmax"],
    "dec-off": [
        "dec-ifork-off-astar-pdb", "dec-ifork-off-pea0-pdb",
        "dec-ifork-off-pea-pdb", "dec-ifork-off-epea-pdb",
        "dec-ifork-off-epeafc-pdb", "dec-fork-off-astar-hmax",
        "dec-fork-off-pea-hmax", "dec-fork-off-epea-pdb",
        f"dec-{THIRD}-off-astar-hmax", f"dec-{THIRD}-off-epea-pdb",
        "dec-ifork-off-pea-hmax", "dec-ifork-off-pea0-hmax"],
}

ABSTAINED_EXIT_CODE = 19

def get_factoring_parser():
    parser = Parser()
    parser.add_pattern("factoring_strategy", r"using (.+) factoring profile",
                       type=str)
    parser.add_pattern("factoring_profile",
                       r"Info: factoring (?:is|type set to) (.+)", type=str)
    parser.add_pattern("factoring_hash", r"Factoring hash: (\d+)", type=int)
    parser.add_pattern("num_leaves", r"number leaf factors: (\d+)", type=int)
    parser.add_pattern("factoring_time", r"factoring time ([0-9.e+-]+)s",
                       type=float)
    parser.add_pattern("pdb_pattern",
                       r"greedy pattern generator pattern: (\[.*\])",
                       type=str)
    parser.add_pattern("pdb_affected_leaf_factors",
                       r"greedy pattern generator number of affected "
                       r"leaf factors: (\d+)", type=int)

    parser.add_pattern("average_member_states",
                       r"Average number of member states \(generated "
                       r"decoupled states\): (.+)", type=float)
    return parser

def fix_abstained(content, props):
    if CATEGORY != "explicit":
        props["abstained"] = int(
            props.get("planner_exit_code") == ABSTAINED_EXIT_CODE)
    if props.get("planner_exit_code") == ABSTAINED_EXIT_CODE:
        props["error"] = "search-decoupled-abstained"
        props["abstained"] = 1
        props["unsolvable"] = 0
        errors = props.get("unexplained_errors")
        if errors:
            props["unexplained_errors"] = [
                e for e in errors if "exitcode-19" not in e]

def get_abstain_parser():
    parser = Parser()
    parser.add_function(fix_abstained, file="driver.log")
    return parser

NONMOBILE = "ERROR: non-mobile leaf factor"

def factoring_refused(run):
    if run.get("error") == "search-decoupled-abstained":
        return True
    return any(NONMOBILE in e for e in run.get("unexplained_errors") or [])

class AbstainFilter:

    def __init__(self, algos):
        self.algos = set(algos)
        self.excluded_tasks = set()

    def collect(self, run):
        if run["algorithm"] in self.algos and factoring_refused(run):
            self.excluded_tasks.add((run["domain"], run["problem"]))
        return True

    def filter(self, run):
        return (run["domain"], run["problem"]) not in self.excluded_tasks

CONFIGS = build_configs(CATEGORY)
SMOKE = (not REMOTE) or PILOT
TIME_LIMIT = "30m" if REMOTE else "1m"
if not SMOKE:
    SUITE_USED = SUITE
else:
    SUITE_USED = TEST_SUITE
    CONFIGS = {n: c for n, c in CONFIGS.items() if n in TEST_NICKS[CATEGORY]}
    if CATEGORY == "dec-off":

        CONFIGS["dec-abstainprobe-astar-hmax"] = [
            "--decoupling", "ifork(min_num_leaves=10)",
            "--search", "astar(hmax())"]
MEMORY_LIMIT = "4G"
if ONLY_TASKS:
    SUITE_USED = ONLY_TASKS
if ONLY_ROWS:
    unknown = [r for r in ONLY_ROWS if r not in CONFIGS]
    if unknown:
        raise SystemExit(
            f"EVAL_ONLY_ROWS: unknown row(s) {', '.join(unknown)} for "
            f"category {CATEGORY}, known rows are {', '.join(sorted(CONFIGS))}")
    CONFIGS = {n: c for n, c in CONFIGS.items() if n in ONLY_ROWS}

N_TASKS = len(suites.build_suite(BENCHMARKS_DIR, SUITE_USED))
N_RUNS = len(CONFIGS) * N_TASKS

def slurm_time_limit_per_task(n_runs, max_tasks):
    per_task = math.ceil(n_runs / max_tasks)
    hours = max(12, math.ceil(per_task * 0.5 * 1.1) + 1)
    if hours > 72:
        raise SystemExit(
            f"{per_task} runs per array task need {hours} h, above the cpu "
            "partition's 72 h: split the category (fewer configurations) "
            "before submitting")
    return f"{hours:02d}:00:00"

FORWARDED = {k: v for k, v in {
    "EVAL_CATEGORY": CATEGORY, "EVAL_FACTORINGS": ",".join(FACTORINGS),
    "EVAL_HEURISTICS": ",".join(HEURISTIC_SET),
    "EVAL_MERGE_DIRS": os.environ.get("EVAL_MERGE_DIRS"),
    "EVAL_BUILD": BUILD, "EVAL_BUILD_JOBS": BUILD_JOBS,
    "EVAL_DATA_DIR": str(DATA_DIR),
    "EVAL_REPORT_FORMATS": os.environ.get("EVAL_REPORT_FORMATS"),
    "EVAL_SCATTER_FORMATS": os.environ.get("EVAL_SCATTER_FORMATS"),
    "EVAL_EMAIL": EMAIL, "EVAL_PILOT": "1" if PILOT else None,
    "EVAL_KEEP_RUNS": "1" if KEEP_RUNS else None, "EVAL_STAGE": STAGE,
    "EVAL_ONLY_TASKS": ",".join(ONLY_TASKS), "EVAL_ONLY_ROWS": ",".join(ONLY_ROWS),
    "EVAL_STDOUT_LIMITS": os.environ.get("EVAL_STDOUT_LIMITS"),
    "EVAL_LP_SOLVER_OK": os.environ.get("EVAL_LP_SOLVER_OK"),
    "DOWNWARD_REPO": REPO, "DOWNWARD_BENCHMARKS": BENCHMARKS_DIR,
    "DOWNWARD_REV": REV,
}.items() if v}
JOB_SETUP = "\n".join(f"export {k}={shlex.quote(v)}" for k, v in FORWARDED.items())

if REMOTE and LOCAL_PROCESSES:

    worst_case_hours = N_RUNS * 0.5 / LOCAL_PROCESSES
    if worst_case_hours > 72 and os.environ.get("EVAL_FORCE_NODE_JOB") != "1":
        raise SystemExit(
            f"{N_RUNS} runs on {LOCAL_PROCESSES} processes need up to "
            f"{worst_case_hours:.0f} h (30 min per run), beyond the 72 h "
            "walltime of one node job: submit this category as lab's job "
            "chain instead (submit-all.sh), or run a pilot / one factoring, "
            "or set EVAL_FORCE_NODE_JOB=1 if the runs are known to be short")
    ENVIRONMENT = LocalEnvironment(processes=LOCAL_PROCESSES)
elif REMOTE and RUNS_STAGE:
    ENVIRONMENT = CLUSTER_ENV_CLASS(
        email=EMAIL,
        memory_per_cpu="4200M",
        time_limit_per_task=slurm_time_limit_per_task(
            N_RUNS, CLUSTER_ENV_CLASS.MAX_TASKS),
        setup=JOB_SETUP)
else:
    ENVIRONMENT = LocalEnvironment(processes=4)

def parse_slurm_time(text):
    text = text.strip()
    if not text or text.lower() in ("infinite", "unlimited", "n/a"):
        return None
    days = 0
    if "-" in text:
        day_part, text = text.split("-", 1)
        days = int(day_part)
        parts = [int(x) for x in text.split(":")] + [0, 0]
        hours, minutes, seconds = parts[0], parts[1], parts[2]
    else:
        parts = [int(x) for x in text.split(":")]
        if len(parts) == 1:
            hours, minutes, seconds = 0, parts[0], 0
        elif len(parts) == 2:
            hours, minutes, seconds = 0, parts[0], parts[1]
        else:
            hours, minutes, seconds = parts[0], parts[1], parts[2]
    return days * 24 + hours + minutes / 60 + seconds / 3600

def check_cluster_limits(env):
    def query(command):
        try:
            return subprocess.run(command, capture_output=True, text=True,
                                  timeout=60, check=True).stdout
        except (OSError, subprocess.SubprocessError) as exc:
            print(f"warning: {' '.join(command)} failed ({exc}). Check "
                  "the limits by hand (CLUSTER.md section 7)")
            return ""

    found = []
    needed = parse_slurm_time(env.time_limit_per_task)
    partition = query(["scontrol", "show", "partition", env.partition])
    match = re.search(r"MaxTime=(\S+)", partition)
    if match:
        found.append(f"walltime {match.group(1)}")
        limit = parse_slurm_time(match.group(1))
        if limit is not None and needed is not None and needed > limit:
            raise SystemExit(
                f"the derived time limit per array task, "
                f"{env.time_limit_per_task}, exceeds the walltime limit of "
                f"partition {env.partition} ({match.group(1)}). Split the "
                "category (scope knobs) before submitting")
    out = query(["scontrol", "show", "config"])
    match = re.search(r"MaxArraySize\s*=\s*(\d+)", out)
    num_tasks = math.ceil(N_RUNS / math.ceil(N_RUNS / env.MAX_TASKS))
    if match:
        found.append(f"MaxArraySize {match.group(1)}")
        if num_tasks > int(match.group(1)) - 1:
            raise SystemExit(
                f"{num_tasks} array tasks, but Slurm's MaxArraySize is "
                f"{match.group(1)} (highest index {int(match.group(1)) - 1}): "
                "lower MAX_TASKS in bw_cluster_environments.py")
    match = re.search(r"MaxMemPerCPU=(\w+)", partition)
    if match:
        found.append(f"MaxMemPerCPU {match.group(1)}")
        if match.group(1).isdigit() and 0 < int(match.group(1)) < 4200:
            print(f"warning: MaxMemPerCPU of partition {env.partition} is "
                  f"{match.group(1)} MB, below the 4200M requested per run. "
                  "Slurm allocates two cores per array task (half the "
                  "throughput, no failure)")
    print(f"cluster limits of partition {env.partition}: "
          + (", ".join(found) if found else "not readable")
          + f". Submission needs {env.time_limit_per_task} per task "
          f"and {num_tasks} array tasks")

if REMOTE and RUNS_STAGE and not LOCAL_PROCESSES \
        and not os.environ.get("SLURM_JOB_ID"):
    check_cluster_limits(ENVIRONMENT)

class Experiment(FastDownwardExperiment):

    def _add_runs(self):
        super()._add_runs()
        if STDOUT_LIMITS:
            for run in self.runs:
                kwargs = run.commands["planner"][1]
                kwargs["soft_stdout_limit"] = STDOUT_LIMITS[0]
                kwargs["hard_stdout_limit"] = STDOUT_LIMITS[1]

exp = Experiment(
    path=str(DATA_DIR / (f"{SCRIPT_NAME}-{CATEGORY}"
                         + (FACTORING_SUFFIX if CATEGORY != "explicit" else "")
                         + HEURISTIC_SUFFIX
                         + ("-pilot" if PILOT else "")
                         + ("-rerun" if RERUN else ""))),
    environment=ENVIRONMENT,
    revision_cache=str(DATA_DIR / "revision-cache"))

print(f"category {CATEGORY} ({CATEGORIES[CATEGORY]}), stage {STAGE}"
      + (f", factorings {','.join(FACTORINGS)}" if CATEGORY != "explicit" else "")
      + f", heuristics {','.join(HEURISTIC_SET)}"
      + ": "
      f"{len(CONFIGS)} configurations x {N_TASKS} tasks = {N_RUNS} runs"
      + (" (rerun subset)" if RERUN else "")
      + (f", log limits {STDOUT_LIMITS[0]}/{STDOUT_LIMITS[1]} KiB"
         if STDOUT_LIMITS else "")
      + (f", Slurm time limit per array task "
         f"{ENVIRONMENT.time_limit_per_task}"
         + (" (cluster pilot)" if PILOT else "")
         if REMOTE and RUNS_STAGE and not LOCAL_PROCESSES else
         (f" (node job, {LOCAL_PROCESSES} parallel runs, worst case "
          f"{N_RUNS * 0.5 / LOCAL_PROCESSES:.0f} h)" if REMOTE and LOCAL_PROCESSES
          else (" (reports only)" if REMOTE else " (smoke test)"))))

for nick, component_options in sorted(CONFIGS.items()):
    exp.add_algorithm(
        nick, REPO, REV, component_options,
        build_options=[BUILD, f"-j{BUILD_JOBS}"],
        driver_options=["--build", BUILD,
                        "--overall-time-limit", TIME_LIMIT,
                        "--overall-memory-limit", MEMORY_LIMIT])

exp.add_suite(BENCHMARKS_DIR, SUITE_USED)

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(get_factoring_parser())
exp.add_parser(pea_parser.get_parser())
exp.add_parser(get_abstain_parser())
exp.add_parser(exp.PLANNER_PARSER)

def locked_build():
    import fcntl
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    with open(DATA_DIR / "revision-cache.lock", "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        exp.build()

def compress_and_delete_runs():
    runs_dir = Path(exp.path.rstrip("/"))
    archive = str(runs_dir) + ".tar.gz"
    code = subprocess.call(["tar", "-czf", archive, "-C", str(runs_dir.parent),
                            runs_dir.name + "/"])
    properties = Path(exp.eval_dir) / "properties"
    fetched = 0
    if properties.is_file():
        with open(properties) as f:
            fetched = sum(1 for line in f if '"run_dir":' in line)
    if code != 0 or fetched != N_RUNS:
        print(f"keeping {runs_dir}. Tar exit code {code}, {fetched} of "
              f"{N_RUNS} runs in {properties}. Repeat parse and fetch, "
              "then delete by hand")
        return 1
    subprocess.call(["rm", "-rf", str(runs_dir)])
    return 0

if RUNS_STAGE:
    exp.add_step("build", locked_build if LOCAL_PROCESSES else exp.build)
    exp.add_step("start", exp.start_runs)
    exp.add_step("parse", exp.parse)
    exp.add_fetcher(name="fetch")
    if REMOTE and not KEEP_RUNS:

        exp.add_step("compress-and-delete-runs-dir", compress_and_delete_runs)

ENGINE_ATTRIBUTES = [
    Attribute("distinct_expansions", min_wins=True, function=geometric_mean),
    Attribute("alpha_reexpansions", min_wins=True, function=arithmetic_mean),
    Attribute("beta_deferred_edges", min_wins=True, function=arithmetic_mean),
    Attribute("max_open_list_size", min_wins=True, function=geometric_mean),
    Attribute("osf_deferred_operators", min_wins=True,
              function=arithmetic_mean),
    Attribute("osf_deadend_skips", min_wins=True, function=arithmetic_mean),
]
DECOUPLED_ATTRIBUTES = [
    Attribute("abstained", absolute=True, min_wins=True),
    Attribute("num_leaves", min_wins=False, function=arithmetic_mean),
    Attribute("average_member_states", min_wins=True,
              function=arithmetic_mean),
    Attribute("pdb_affected_leaf_factors", absolute=True, min_wins=True),
]

STOCK_ATTRIBUTES = [a for a in common_setup.ATTRIBUTES
                    if a not in ("search_out_of_memory", "search_out_of_time")]
ATTRIBUTES = (STOCK_ATTRIBUTES + ENGINE_ATTRIBUTES
              + (DECOUPLED_ATTRIBUTES if CATEGORY != "explicit" else [])
              + ["score_*"])

def report_name(*parts):
    return "-".join([SCRIPT_NAME, CATEGORY] + list(parts))

FAMILY_ORDER = ["astar", "pea0", "pea", "epeafc", "epea"]
ENGINE_FAMILIES = {"pea0", "pea", "epeafc", "epea"}
OSF_FAMILIES = {"epeafc", "epea"}
STOCK_ATTRS = ["coverage", "cost", "total_time", "search_time", "memory",
               "expansions", "evaluations", "generated"]
ENGINE_ATTRS = STOCK_ATTRS + ["distinct_expansions", "alpha_reexpansions",
                              "beta_deferred_edges", "max_open_list_size"]
OSF_ATTRS = ENGINE_ATTRS + ["osf_deferred_operators", "osf_deadend_skips"]
SCATTER_ATTRS = ["total_time", "memory", "expansions", "evaluations",
                 "generated"]
SCATTER_ENGINE_ATTRS = ["max_open_list_size", "distinct_expansions",
                        "alpha_reexpansions"]
if SMOKE:

    SCATTER_ATTRS, SCATTER_ENGINE_ATTRS = ["total_time"], ["max_open_list_size"]

def slots():
    found = {}
    for nick in CONFIGS:
        parts = nick.split("-")
        if parts[0] == "exp" and len(parts) == 3:
            key, fam = ("exp", parts[2]), parts[1]
        elif parts[0] == "dec" and len(parts) == 5:
            key, fam = (f"dec-{parts[1]}-{parts[2]}", parts[4]), parts[3]
        else:
            continue
        found.setdefault(key, {})[fam] = nick
    return found

def pair_attributes(fam_x, fam_y):
    if {fam_x, fam_y} <= OSF_FAMILIES:
        return OSF_ATTRS
    if {fam_x, fam_y} <= ENGINE_FAMILIES:
        return ENGINE_ATTRS
    return STOCK_ATTRS

PAIRS_BY_KIND = {}
for key, fams in slots().items():
    present = [f for f in FAMILY_ORDER if f in fams]
    for i, fx in enumerate(present):
        for fy in present[i + 1:]:
            PAIRS_BY_KIND.setdefault((fx, fy), []).append((fams[fx], fams[fy]))

def export_csv(eval_dir, outfile):
    import csv
    import json
    props = json.load(open(os.path.join(eval_dir, "properties")))
    keys = sorted({k for run in props.values() for k in run})
    with open(outfile, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["run_id"] + keys)
        for run_id, run in sorted(props.items()):
            row = [run_id]
            for k in keys:
                v = run.get(k, "")
                row.append(json.dumps(v) if isinstance(v, (list, dict)) else v)
            writer.writerow(row)
    print(f"wrote {outfile}: {len(props)} runs x {len(keys)} attributes")

def write_tex_fragments(eval_dir):
    import glob
    import re
    pattern = re.compile(
        r"\\subsection\*\{([^}]*)\}|\\hypertarget\{([^}]*)\}\{\}"
        r"|\\begin\{tabular\}.*?\\end\{tabular\}", re.S)
    for path in sorted(glob.glob(os.path.join(eval_dir, "*.tex"))):
        if path.endswith("-fragment.tex"):
            continue
        body = open(path).read().split("\\begin{document}", 1)[-1]
        if "\\begin{tikzpicture}" in body:

            start = body.index("\\begin{tikzpicture}")
            end = body.index("\\end{tikzpicture}") + len("\\end{tikzpicture}")
            picture = body[start:end]
            values = []
            for block in re.findall(r"coordinates\s*\{([^}]*)\}", picture):
                for x, y in re.findall(r"\(\s*([^,\s]+)\s*,\s*([^)\s]+)\s*\)",
                                       block):
                    values.extend([float(x), float(y)])
            positive = [v for v in values if v > 0]
            diagonal = re.compile(
                r"\\draw\[color=black\] \(axis cs:[^)]*\) -- \(axis cs:[^)]*\);")
            if positive:
                lo, hi = min(positive), max(values)
                picture = diagonal.sub(
                    f"\\\\draw[color=black] (axis cs:{lo:g},{lo:g}) -- "
                    f"(axis cs:{hi:g},{hi:g});", picture)
            else:
                picture = diagonal.sub("", picture)
            picture = re.sub(r"(height|width)=[0-9.]+in", r"\1=\\figwidth",
                             picture)
            open(path[:-4] + "-fragment.tex", "w").write(
                f"% plot of {os.path.basename(path)}, extracted by the "
                "experiment script. Requires \\usepackage{pgfplots}\n"
                "\\providecommand{\\figwidth}{0.75\\linewidth}\n"
                + picture + "\n")
            print(f"{os.path.basename(path)}: plot -> fragment")
            continue
        out = [f"% tables of {os.path.basename(path)}, extracted by the "
               "experiment script. Requires \\usepackage{booktabs}",
               "\\providecommand{\\numtasks}[1]{\\small{(#1)}}", ""]
        heading, target, count = "", "", 0
        for m in pattern.finditer(body):
            if m.group(1) is not None:
                heading = m.group(1)
            elif m.group(2) is not None:
                target = m.group(2)
            elif heading != "info":

                table = m.group(0)

                table = re.sub(r"\\htmladdnormallink\{([^{}]*)\}\{[^}]*\}", r"\1", table)
                table = re.sub(r"(?<!\\)_", r"\\_", table)
                out.extend([f"% ==== {heading} [{target}]", table, ""])
                count = count + 1
        open(path[:-4] + "-fragment.tex", "w").write("\n".join(out))
        print(f"{os.path.basename(path)}: {count} tables -> fragment")

def family_of(nick):
    parts = nick.split("-")
    if parts[0] == "exp" and len(parts) == 3:
        return parts[1]
    if parts[0] == "dec" and len(parts) == 5:
        return parts[3]
    return None

def attributes_named(names):
    return [a for a in ATTRIBUTES if a in names]

def add_reports():
    for fmt in TABLE_FORMATS:
        exp.add_report(AbsoluteReport(attributes=ATTRIBUTES, format=fmt),
                       outfile=f"{report_name('all')}.{fmt}")

        for name, families, attrs in (("engine", ENGINE_FAMILIES, ENGINE_ATTRS),
                                      ("osf", OSF_FAMILIES, OSF_ATTRS)):
            algos = sorted(n for n in CONFIGS if family_of(n) in families)
            if algos:
                exp.add_report(
                    AbsoluteReport(attributes=attributes_named(attrs),
                                   format=fmt, filter_algorithm=algos),
                    outfile=f"{report_name(name)}.{fmt}")
        if CATEGORY != "explicit":
            for factoring in FACTORINGS:
                algos = sorted(n for n in CONFIGS
                               if n.startswith(f"dec-{factoring}-"))
                if not algos:
                    continue
                f = AbstainFilter(algos)
                exp.add_report(
                    AbsoluteReport(attributes=ATTRIBUTES, format=fmt,
                                   filter_algorithm=algos,
                                   filter=[f.collect, f.filter]),
                    outfile=f"{report_name(factoring)}.{fmt}")
        for (fx, fy), algorithm_pairs in PAIRS_BY_KIND.items():
            exp.add_report(
                ComparativeReport(algorithm_pairs,
                                  attributes=pair_attributes(fx, fy),
                                  format=fmt),
                outfile=f"{report_name('compare', f'{fx}-vs-{fy}')}.{fmt}")
    for (fx, fy), algorithm_pairs in PAIRS_BY_KIND.items():
        attrs = SCATTER_ATTRS + (SCATTER_ENGINE_ATTRS
                                 if {fx, fy} <= ENGINE_FAMILIES else [])
        for x, y in algorithm_pairs:
            for attribute in attrs:
                for fmt in SCATTER_FORMATS:
                    exp.add_report(
                        ScatterPlotReport(attributes=[attribute],
                                          filter_algorithm=[x, y],
                                          scale="log", format=fmt),
                        outfile=f"{report_name('scatter', attribute, x, y)}"
                                f".{fmt}")
    if "tex" in TABLE_FORMATS:
        exp.add_step("tex-fragments", write_tex_fragments, exp.eval_dir)
    exp.add_step("export-csv", export_csv, exp.eval_dir,
                 os.path.join(exp.eval_dir, f"{report_name('runs')}.csv"))

if REPORTS_STAGE:

    for i, src in enumerate(d for d in os.environ.get(
            "EVAL_MERGE_DIRS", "").split(",") if d):
        src = os.path.abspath(src)
        if not os.path.isfile(os.path.join(src, "properties")):
            raise SystemExit(f"EVAL_MERGE_DIRS: no properties in {src}")
        exp.add_fetcher(src=src, name=f"merge-{i + 1}", merge=True)
    add_reports()

exp.run_steps()
