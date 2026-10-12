#!/usr/bin/env python3

import json
import re
import sys

PATTERNS = [
    ("distinct_expansions", r"Distinct expansions: (\d+) state\(s\)\.", int),
    ("alpha_reexpansions", r"Partial re-expansions \(alpha\): (\d+)", int),
    ("beta_deferred_edges", r"Deferred surplus edges \(beta\): (\d+)", int),
    ("max_open_list_size", r"Maximum open list size: (\d+)", int),

    ("osf_deferred_operators",
     r"Deferred surplus operators \(osf\): (\d+)", int),
    ("osf_deadend_skips",
     r"Dead-end successors never generated \(osf\): (\d+)", int),
]

def parse_text(text):
    found = {}
    for name, regex, cast in PATTERNS:
        match = re.search(regex, text)
        if match:
            found[name] = cast(match.group(1))
    return found

def run_cli(paths):
    result = {}
    for path in paths:
        if path == "-":
            text = sys.stdin.read()
        else:
            with open(path, "r") as handle:
                text = handle.read()
        result[path] = parse_text(text)
    json.dump(result, sys.stdout, indent=2, sort_keys=True)
    sys.stdout.write("\n")

def get_parser():
    from lab.parser import Parser

    parser = Parser()
    for name, regex, cast in PATTERNS:
        parser.add_pattern(name, regex, type=cast)
    return parser

if __name__ == "__main__":
    if len(sys.argv) > 1:
        run_cli(sys.argv[1:])
    else:
        sys.stderr.write(
            "usage: pea_parser.py LOGFILE [LOGFILE ...]  (or '-' for stdin)\n"
            "From a lab experiment script use "
            "exp.add_parser(pea_parser.get_parser()) instead.\n")
        sys.exit(2)
