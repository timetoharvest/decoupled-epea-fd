#!/bin/bash
# Submit the categories as lab's job chains, one after another, from a
# login node (CLUSTER.md section 7). Each category becomes one chain of
# Slurm jobs: build, start (one array job over all runs of the category),
# parse, fetch, compress and delete of the runs directory. Variables set in
# the shell or in front of the command (EVAL_FACTORINGS, EVAL_PILOT,
# EVAL_KEEP_RUNS, EVAL_DATA_DIR, EVAL_EMAIL, ...) apply to every category.
#
#     ./submit-all.sh                      # explicit, dec-on, dec-off
#     ./submit-all.sh dec-on dec-off       # a subset, in this order
#     EVAL_PILOT=1 ./submit-all.sh         # the three pilots
set -eu
cd "$(dirname "$0")"
command -v uv >/dev/null || { echo "uv is not on the PATH (CLUSTER.md section 5)" >&2; exit 1; }
command -v validate >/dev/null || { echo "validate (VAL) is not on the PATH: every run would fail (section 5)" >&2; exit 1; }
categories=("$@")
[ $# -eq 0 ] && categories=(explicit dec-on dec-off)
for category in "${categories[@]}"; do
    echo "=== submitting $category"
    EVAL_CATEGORY="$category" uv run ./2026-10-11-pea-epea-decoupled.py --all
done
echo "=== submitted; watch with: squeue -u $USER"
