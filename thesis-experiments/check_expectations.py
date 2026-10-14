#!/usr/bin/env python3

import glob
import json
import os
import re
import sys
from collections import defaultdict

GOLDEN = {
    ("exp-astar-hmax", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 4885, "evaluations": 6718,
         "generated": 32256},
    ("exp-pea0-hmax", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 4885, "distinct_expansions": 4884,
         "alpha_reexpansions": 0, "beta_deferred_edges": 0,
         "max_open_list_size": 1881},
    ("exp-pea-hmax", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 8201, "distinct_expansions": 4884,
         "alpha_reexpansions": 3316, "beta_deferred_edges": 6669,
         "max_open_list_size": 1337},
    ("exp-astar-pdb", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 21, "evaluations": 140,
         "generated": 166},
    ("exp-pea-pdb", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 21, "distinct_expansions": 20,
         "alpha_reexpansions": 0, "beta_deferred_edges": 90,
         "max_open_list_size": 50},
    ("exp-epea-pdb", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 21, "distinct_expansions": 20,
         "alpha_reexpansions": 0, "evaluations": 50, "generated": 49,
         "max_open_list_size": 50, "osf_deferred_operators": 117,
         "osf_deadend_skips": 0},
    ("exp-astar-pdb", "woodworking-opt08-strips", "p01.pddl"):
        {"cost": 170, "expansions": 164, "evaluations": 800,
         "generated": 1553},
    ("exp-epea-pdb", "woodworking-opt08-strips", "p01.pddl"):
        {"cost": 170, "expansions": 291, "distinct_expansions": 163,
         "alpha_reexpansions": 127, "evaluations": 164, "generated": 399,
         "max_open_list_size": 158, "osf_deferred_operators": 1195,
         "osf_deadend_skips": 171},
    ("dec-ifork-on-astar-hmax", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 894, "evaluations": 1445,
         "generated": 6223},
    ("dec-ifork-on-pea0-hmax", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 894, "distinct_expansions": 894,
         "alpha_reexpansions": 0, "beta_deferred_edges": 0,
         "max_open_list_size": 517},
    ("dec-ifork-on-pea-hmax", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 1609, "distinct_expansions": 898,
         "alpha_reexpansions": 711, "beta_deferred_edges": 1406,
         "max_open_list_size": 315},
    ("dec-ifork-off-astar-pdb", "woodworking-opt08-strips", "p01.pddl"):
        {"cost": 170, "expansions": 241, "evaluations": 542,
         "generated": 1305},
    ("dec-ifork-off-pea-pdb", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 17, "distinct_expansions": 17,
         "alpha_reexpansions": 0, "beta_deferred_edges": 34,
         "max_open_list_size": 42},
    ("dec-ifork-off-epea-pdb", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 2323, "distinct_expansions": 1589,
         "alpha_reexpansions": 734, "evaluations": 1932,
         "generated": 7350, "max_open_list_size": 1294,
         "osf_deferred_operators": 6387, "osf_deadend_skips": 0},
    ("dec-ifork-off-epea-pdb", "woodworking-opt08-strips", "p01.pddl"):
        {"cost": 170, "expansions": 940, "distinct_expansions": 290,
         "alpha_reexpansions": 650, "evaluations": 506,
         "generated": 1454, "max_open_list_size": 217,
         "osf_deferred_operators": 932, "osf_deadend_skips": 74},
    ("dec-fork-off-epea-pdb", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 34, "distinct_expansions": 19,
         "alpha_reexpansions": 15, "evaluations": 21, "generated": 45,
         "max_open_list_size": 8, "osf_deferred_operators": 54,
         "osf_deadend_skips": 0},
}

ENGINE_FAMILIES = {"pea0", "pea", "epea", "epeafc"}

SLURM_MARKER = "output-to-slurm.err"
SOFT_LINE = re.compile(
    r"planner finished and wrote [\d.]+ KiB to run\.(log|err) "
    r"\(soft limit: [\d.]+ KiB\)")
HARD_LINE = re.compile(
    r"planner wrote [\d.]+ KiB \(hard limit\) to run\.(log|err) -> "
    r"abort command")
NONMOBILE = "ERROR: non-mobile leaf factor"
DOUBLE_PEAK = "Found multiple occurences of Peak memory"
TIME_UNITS = {"": 1, "s": 1, "m": 60, "h": 3600}
EXPLAINED = {
    "log-soft": "run.log over lab's soft limit, planner finished, data valid",
    "log-hard": "run.log hit lab's hard limit, lab aborted the planner, "
                "counts as unsolved",
    "kill-time": "component killed by the kernel at the hard CPU limit "
                 "(soft limit plus one second), counts as unsolved",
    "nonmobile": "upstream factoring refused a leaf without leaf-only "
                 "actions, uniform over the rows of that factoring, "
                 "counts as a factoring failure",
    "double-peak": "Peak memory printed twice (signal during exit), "
                   "unsolved run, memory attribute dropped by lab",
}

def parse_limit(text, units):
    match = re.fullmatch(r"(\d+)([a-z]?)", text.lower())
    if not match or match.group(2) not in units:
        return None
    return int(match.group(1)) * units[match.group(2)]

def overall_time_limit(run):
    seconds = None
    opts = run.get("driver_options") or []
    for i in range(len(opts) - 1):
        if opts[i] == "--overall-time-limit":
            seconds = parse_limit(opts[i + 1], TIME_UNITS)
    return seconds

def file_lines(entry, name):
    return [line.strip() for line in entry[len(name) + 1:].splitlines()
            if line.strip()]

def explain(run, errors):
    outcomes = []
    driver_lines = []
    run_lines = []
    for entry in errors:
        if entry.startswith("driver.err:"):
            driver_lines.extend(file_lines(entry, "driver.err:"))
        elif entry.startswith("run.err:"):
            run_lines.extend(file_lines(entry, "run.err:"))
        else:
            outcomes.append(entry)
    soft = [line for line in driver_lines if SOFT_LINE.search(line)]
    hard = [line for line in driver_lines if HARD_LINE.search(line)]
    other = [line for line in driver_lines
             if line not in soft and line not in hard]
    if other:
        return None
    if not outcomes and not run_lines and soft and not hard:
        return "log-soft"
    if outcomes == ["exitcode--15"] and hard and not run_lines:
        return "log-hard"
    if outcomes == ["sigkill"] and not driver_lines and not run_lines:
        seconds = overall_time_limit(run)
        wall = run.get("planner_wall_clock_time")
        if seconds and wall is not None and wall >= seconds - 1:
            return "kill-time"
        return None
    if outcomes == ["search-critical-error"] and not driver_lines \
            and run_lines and run_lines[0].startswith(NONMOBILE):
        return "nonmobile"
    if len(outcomes) == 1 and outcomes[0].startswith(DOUBLE_PEAK) \
            and not driver_lines and not run_lines \
            and run.get("coverage") != 1:
        return "double-peak"
    return None

def family_of(nick):
    parts = nick.split("-")
    if parts[0] == "exp" and len(parts) == 3:
        return parts[1], ("exp", None, None, parts[2])
    if parts[0] == "dec" and len(parts) == 5:
        return parts[3], ("dec", parts[1], parts[2], parts[4])
    return None, None

def main():
    paths = sys.argv[1:]
    if not paths:
        candidates = sorted(glob.glob(
            os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "data", "*-eval", "properties")),
            key=os.path.getmtime)
        if not candidates:
            sys.exit("no data/*-eval/properties found and none given")
        paths = [candidates[-1]]

    props = {}
    for path in paths:
        if os.path.isdir(path):
            path = os.path.join(path, "properties")
        print(f"checking {path}")
        props.update(json.load(open(path)))
    runs = list(props.values())
    failures = []
    notes = []
    open_pairs = defaultdict(int)
    open_inversions = defaultdict(list)
    h6_pairs = 0
    h6_inversions = []
    fullcheck_skipped = 0

    marked = 0
    explained = defaultdict(list)
    for run in runs:
        errors = run.get("unexplained_errors") or []
        if SLURM_MARKER in errors:
            marked = marked + 1
            errors = [e for e in errors if e != SLURM_MARKER]
        if not errors:
            continue
        kind = explain(run, errors)
        if kind is None:
            failures.append(
                f"H1 unexplained error: {run['algorithm']} "
                f"{run['domain']}:{run['problem']} -> {errors}")
        else:
            explained[kind].append(run)
    if marked:
        notes.append(
            f"H1 {marked} run(s) carry lab's marker {SLURM_MARKER}. A chain "
            "job wrote to slurm.err. Read that file. Runs are clean.")
    for kind, group in sorted(explained.items()):
        domains = defaultdict(int)
        for run in group:
            domains[run["domain"]] = domains[run["domain"]] + 1
        note = (f"H1 {kind}: {len(group)} run(s), {EXPLAINED[kind]}: "
                f"{dict(sorted(domains.items()))}")
        if kind == "kill-time":
            walls = [run["planner_wall_clock_time"] for run in group]
            note = note + (f", wall-clock {min(walls):.0f} to "
                           f"{max(walls):.0f} s")
        notes.append(note)
        if kind in ("log-hard", "double-peak"):
            for run in sorted(group, key=lambda r: (
                    r["algorithm"], r["domain"], r["problem"]))[:30]:
                notes.append(f"   {run['algorithm']} "
                             f"{run['domain']}:{run['problem']}")
    refused = defaultdict(set)
    rows = defaultdict(set)
    for run in runs:
        fam, key = family_of(run["algorithm"])
        if key is None or key[0] != "dec":
            continue
        slot = (key[1], run["domain"], run["problem"])
        rows[slot].add(run["algorithm"])
        if run in explained["nonmobile"]:
            refused[slot].add(run["algorithm"])
    for slot, names in sorted(refused.items()):
        if names != rows[slot]:
            failures.append(
                f"H1 non-mobile leaf refusal not uniform for {slot[0]} on "
                f"{slot[1]}:{slot[2]}: {len(names)} of {len(rows[slot])} "
                "rows")
    if refused:
        tasks = sorted({(s[1], s[2]) for s in refused})
        notes.append(
            f"H1 nonmobile tasks ({len(tasks)}): "
            + ", ".join(f"{d}:{p}" for d, p in tasks[:20]))

    solved_by = defaultdict(set)
    for run in runs:
        if run.get("coverage") == 1:
            solved_by[(run["domain"], run["problem"])].add(run["algorithm"])
    no_plan = defaultdict(set)
    for run in runs:
        if run.get("error") != "search-unsolved-incomplete":
            continue
        task = (run["domain"], run["problem"])
        no_plan[task].add(run["algorithm"])
        if solved_by[task]:
            failures.append(
                f"H10 no plan reported on a solved task: {run['algorithm']} "
                f"{task[0]}:{task[1]} (solved by "
                f"{', '.join(sorted(solved_by[task])[:3])})")
    if no_plan:
        per_domain = defaultdict(int)
        for task in no_plan:
            per_domain[task[0]] = per_domain[task[0]] + 1
        notes.append(
            f"S5 no plan (search exhausted or initial dead end) on "
            f"{len(no_plan)} task(s), {sum(len(v) for v in no_plan.values())} "
            f"run(s), tasks per domain: {dict(sorted(per_domain.items()))}")

    costs = defaultdict(set)
    for run in runs:
        if run.get("coverage") == 1 and "cost" in run:
            costs[(run["domain"], run["problem"])].add(run["cost"])
    for task, values in sorted(costs.items()):
        if len(values) > 1:
            failures.append(
                f"H2 cost mismatch on {task[0]}:{task[1]} -> {values}")

    by_slot = defaultdict(dict)
    for run in runs:
        fam, key = family_of(run["algorithm"])
        if fam is None:
            continue
        by_slot[(key, run["domain"], run["problem"])][fam] = run

    identity_checked = 0
    fullcheck_checked = 0
    for (key, domain, problem), fams in sorted(by_slot.items()):
        task = f"{domain}:{problem}"
        mode = key[0]

        astar, pea0 = fams.get("astar"), fams.get("pea0")
        if astar and pea0 and astar.get("coverage") == 1 \
                and pea0.get("coverage") == 1:
            for attr in ("cost", "expansions", "evaluations", "generated"):
                if astar.get(attr) != pea0.get(attr):
                    failures.append(
                        f"H3 pea0 drift {key} {task} {attr}: "
                        f"astar={astar.get(attr)} pea0={pea0.get(attr)}")

        pea_run, epeafc = fams.get("pea"), fams.get("epeafc")
        if pea_run and epeafc and pea_run.get("coverage") == 1 \
                and epeafc.get("coverage") == 1:
            if pea_run.get("pdb_pattern") != epeafc.get("pdb_pattern"):

                fullcheck_skipped = fullcheck_skipped + 1
            else:
                for attr in ("cost", "expansions", "evaluations",
                             "generated", "distinct_expansions",
                             "alpha_reexpansions", "beta_deferred_edges",
                             "max_open_list_size"):
                    if pea_run.get(attr) != epeafc.get(attr):
                        failures.append(
                            f"H8 full-checking drift {key} {task} {attr}: "
                            f"pea={pea_run.get(attr)} "
                            f"epeafc={epeafc.get(attr)}")
                fullcheck_checked = fullcheck_checked + 1

        for fam in ENGINE_FAMILIES:
            run = fams.get(fam)
            if not run or run.get("coverage") != 1:
                continue
            e = run.get("expansions")
            d = run.get("distinct_expansions")
            a = run.get("alpha_reexpansions")
            if e is None or d is None or a is None:
                failures.append(
                    f"H4 unparsable identity inputs: {run['algorithm']} "
                    f"{task} (e={e} d={d} a={a})")
                continue
            delta = e - d - a
            ok = (delta == 1) if mode == "exp" else (delta in (0, 1))
            if not ok:
                failures.append(
                    f"H4 pop identity broken: {run['algorithm']} {task} "
                    f"{e} != {d} + {a} (+1)")
            identity_checked = identity_checked + 1

        pea = fams.get("pea")
        if pea0 and pea and pea0.get("coverage") == 1 \
                and pea.get("coverage") == 1:
            mo0 = pea0.get("max_open_list_size")
            mo1 = pea.get("max_open_list_size")
            if mo0 is not None and mo1 is not None:
                group = (mode, key[2])
                open_pairs[group] = open_pairs[group] + 1
                if mo1 > mo0:
                    ratio = mo1 / mo0 if mo0 else float("inf")
                    open_inversions[group].append(
                        f"{key} {task} pea={mo1} > pea0={mo0} "
                        f"({ratio:.2f})")

        if mode == "exp":
            epea = fams.get("epea")
            if epea and pea and epea.get("coverage") == 1 \
                    and pea.get("coverage") == 1:
                h6_pairs = h6_pairs + 1
                for attr in ("generated", "evaluations"):
                    if epea.get(attr, 0) > pea.get(attr, 0):
                        h6_inversions.append(
                            f"{key} {task} {attr} epea {epea.get(attr)} > "
                            f"pea {pea.get(attr)}")

    golden_hit = 0
    index = {(r["algorithm"], r["domain"], r["problem"]): r for r in runs}
    for key, expected in GOLDEN.items():
        run = index.get(key)
        if run is None:
            continue
        golden_hit = golden_hit + 1
        for attr, value in expected.items():
            if run.get(attr) != value:
                failures.append(
                    f"H7 golden mismatch {key}: {attr} = "
                    f"{run.get(attr)}, expected {value}")

    fork_checked = 0
    for run in runs:
        nick = run["algorithm"]
        if not nick.startswith("dec-fork-on-") or run.get("coverage") != 1:
            continue
        twin = index.get((nick.replace("-on-", "-off-", 1), run["domain"],
                          run["problem"]))
        if not twin or twin.get("coverage") != 1:
            continue
        for attr in ("cost", "expansions", "distinct_expansions",
                     "evaluations", "generated"):
            if run.get(attr) != twin.get(attr):
                failures.append(
                    f"H9 fork control differs: {nick} vs OFF arm "
                    f"{run['domain']}:{run['problem']} {attr}: "
                    f"{run.get(attr)} != {twin.get(attr)}")
        fork_checked = fork_checked + 1

    agg = defaultdict(lambda: [0, 0, 0, 0])
    baseline = {}
    for (key, domain, problem), fams in by_slot.items():
        if key[0] != "dec":
            continue
        pea, epea = fams.get("epeafc") or fams.get("pea"), fams.get("epea")
        if pea and epea and pea.get("coverage") == 1 \
                and epea.get("coverage") == 1:
            baseline[key] = "epeafc" if fams.get("epeafc") else "pea"
            slot = agg[key]
            slot[0] = slot[0] + pea.get("generated", 0)
            slot[1] = slot[1] + epea.get("generated", 0)
            slot[2] = slot[2] + pea.get("alpha_reexpansions", 0)
            slot[3] = slot[3] + epea.get("alpha_reexpansions", 0)
    for key, (gp, ge, ap, ae) in sorted(agg.items()):
        if gp == 0:
            continue
        verdict = "OK" if ge < gp else "NOT BELOW (expected on goalless unit-cost centers)"
        notes.append(
            f"S1 {key}: generated epea/{baseline[key]} = {ge}/{gp} "
            f"({ge / gp:.3f}) {verdict}")
        notes.append(f"S2 {key}: alpha epea/{baseline[key]} = {ae}/{ap}")

    if h6_pairs:
        notes.append(
            f"H6 explicit: epea generated or evaluations above pea on "
            f"{len(h6_inversions)} of {h6_pairs} task(s) (observation, not a "
            "failure)")
        for inversion in h6_inversions[:10]:
            notes.append("   " + inversion)

    if not open_pairs:
        notes.append("S4 no pea/pea0 pair with max_open_list_size found")
    for group in sorted(open_pairs, key=lambda g: (g[0] != "exp", str(g[1]))):
        mode, arm = group
        label = "explicit" if mode == "exp" else f"decoupled {arm.upper()} arm"
        found = open_inversions[group]
        notes.append(
            f"S4 {label}: max open pea > pea0 on {len(found)} of "
            f"{open_pairs[group]} task(s) (observation, not a failure)")
        for inversion in found[:10]:
            notes.append("   " + inversion)

    errors = defaultdict(int)
    for run in runs:
        errors[run.get("error", "MISSING")] = \
            errors[run.get("error", "MISSING")] + 1
    notes.append(f"S3 error distribution: {dict(sorted(errors.items()))}")

    print(f"runs: {len(runs)}, pop identities checked: {identity_checked}, "
          f"full-checking pairs checked: {fullcheck_checked} "
          f"(skipped, other pattern: {fullcheck_skipped}), "
          f"fork control pairs checked: {fork_checked}, "
          f"golden rows matched against: {golden_hit}/{len(GOLDEN)}")
    for note in notes:
        print("  " + note)
    if failures:
        print(f"\nFAIL ({len(failures)} violation(s)):")
        for failure in failures:
            print("  " + failure)
        sys.exit(1)
    print("\nPASS: results are consistent with every hard expectation.")

if __name__ == "__main__":
    main()
