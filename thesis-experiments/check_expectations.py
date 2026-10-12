#!/usr/bin/env python3

import glob
import json
import os
import sys
from collections import defaultdict

GOLDEN = {
    ("exp-astar-blind", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 11853, "evaluations": 13324,
         "generated": 77203},
    ("exp-pea0-blind", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 11853, "distinct_expansions": 11852,
         "alpha_reexpansions": 0, "beta_deferred_edges": 0,
         "max_open_list_size": 1590},
    ("exp-pea-blind", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 19860, "distinct_expansions": 11852,
         "alpha_reexpansions": 8007, "beta_deferred_edges": 13322,
         "max_open_list_size": 1544},
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
    ("dec-ifork-on-astar-blind", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 1740, "evaluations": 1944,
         "generated": 12045},
    ("dec-ifork-on-pea0-blind", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 1740, "distinct_expansions": 1740,
         "alpha_reexpansions": 0, "beta_deferred_edges": 0,
         "max_open_list_size": 321},
    ("dec-ifork-on-pea-blind", "logistics00", "probLOGISTICS-4-0.pddl"):
        {"cost": 20, "expansions": 3162, "distinct_expansions": 1755,
         "alpha_reexpansions": 1407, "beta_deferred_edges": 1963,
         "max_open_list_size": 289},
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
    for run in runs:
        errors = run.get("unexplained_errors") or []
        if SLURM_MARKER in errors:
            marked = marked + 1
            errors = [e for e in errors if e != SLURM_MARKER]
        if errors:
            failures.append(
                f"H1 unexplained error: {run['algorithm']} "
                f"{run['domain']}:{run['problem']} -> {errors}")
    if marked:
        notes.append(
            f"H1 {marked} run(s) carry lab's marker {SLURM_MARKER}. A chain "
            "job wrote to slurm.err. Read that file. Runs are clean.")

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
