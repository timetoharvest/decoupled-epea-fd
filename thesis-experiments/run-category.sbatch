#!/bin/bash -l
# One Slurm job per category on ONE compute node. The runs of the category
# execute in parallel inside this job (lab's LocalEnvironment, no job
# chain), then parse, fetch, compress and the reports follow in the same
# job. Submit from this directory, after the shell setup of CLUSTER.md
# (modules loaded, uv and validate on the PATH, EVAL_DATA_DIR set):
#
#     sbatch run-category.sbatch                  # three jobs: explicit, dec-on, dec-off
#     sbatch --array=2 run-category.sbatch        # one category: 0 explicit, 1 dec-on, 2 dec-off
#     EVAL_PILOT=1 sbatch run-category.sbatch     # the pilot (smoke subset) as node jobs
#     EVAL_FACTORINGS=lp_general EVAL_LP_SOLVER_OK=1 sbatch --array=1,2 run-category.sbatch
#     sbatch --mail-type=END,FAIL --mail-user=<address> run-category.sbatch
#
# Variables set in the submitting shell or in front of sbatch reach the
# job (sbatch exports the environment by default).
#
# LIMIT. The cpu partition allows at most 72 hours per job. The full suite
# (25,858 / 66,492 / 77,574 runs of up to 30 min each) cannot finish on one
# node in that time, so the experiment script refuses a full category here
# unless EVAL_FORCE_NODE_JOB=1 is set. Use this file for the pilot, for a
# trimmed category (scope knobs in the script) or when the runs are known to
# be short; the full runs go through submit-all.sh, which spreads each
# category over lab's job array on many nodes.
#
# RESOURCES. The two lines marked ADJUST describe one whole node;
# `sinfo -p cpu -o '%P %c %m %l'` prints cores, memory (MiB) and the
# walltime limit per node type. The number of parallel runs is derived
# from the job's memory so that every run keeps its 4 GiB planner limit.
#SBATCH --job-name=pea-epea-eval
#SBATCH --partition=cpu
#SBATCH --qos=normal
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=64            # ADJUST: cores of one node
#SBATCH --mem-per-cpu=3600M           # ADJUST: the supervisor's value for the 64-core nodes (236 GB usable)
#SBATCH --time=72:00:00
#SBATCH --array=0-2
#SBATCH --output=%x-%A_%a.log
#SBATCH --error=%x-%A_%a.err

set -eu
CATEGORIES=(explicit dec-on dec-off)
[ "${SLURM_ARRAY_TASK_ID:-x}" = 0 ] || [ "${SLURM_ARRAY_TASK_ID:-x}" = 1 ] || [ "${SLURM_ARRAY_TASK_ID:-x}" = 2 ] \
    || { echo "array index must be 0 (explicit), 1 (dec-on) or 2 (dec-off), got ${SLURM_ARRAY_TASK_ID:-none}" >&2; exit 1; }
export EVAL_CATEGORY="${CATEGORIES[$SLURM_ARRAY_TASK_ID]}"
cd "$SLURM_SUBMIT_DIR"
SCRIPT=2026-10-11-pea-epea-decoupled.py
[ -f "$SCRIPT" ] || { echo "submit from the thesis-experiments directory: $SCRIPT not found in $PWD" >&2; exit 1; }
command -v uv >/dev/null || { echo "uv is not on the PATH inside the job (CLUSTER.md section 2)" >&2; exit 1; }
command -v validate >/dev/null || { echo "validate (VAL) is not on the PATH inside the job: every run would fail (section 5)" >&2; exit 1; }

# Parallel runs: as many as the job's memory allows at 4300 MiB each (the
# planner limit is 4 GiB), at most one per core.
MEM_MB="${SLURM_MEM_PER_NODE:-$(( SLURM_CPUS_PER_TASK * ${SLURM_MEM_PER_CPU:-3600} ))}"
PROCS=$(( MEM_MB / 4300 ))
[ "$PROCS" -gt "$SLURM_CPUS_PER_TASK" ] && PROCS="$SLURM_CPUS_PER_TASK"
[ "$PROCS" -lt 1 ] && PROCS=1
export EVAL_LOCAL_PROCESSES="$PROCS"
export EVAL_STAGE="${EVAL_STAGE:-both}"

# lab asks on the terminal before it overwrites an experiment or eval
# directory; a batch job has no terminal, so stop here with a clear message.
# (Same naming rule as the script: the factoring suffix only for the
# decoupled categories, then -pilot.)
DATA="${EVAL_DATA_DIR:-$PWD/data}"
EXP="$DATA/${SCRIPT%.py}-$EVAL_CATEGORY"
FACT="${EVAL_FACTORINGS:-fork,ifork,incarcs}"
if [ "$EVAL_CATEGORY" != explicit ] && [ "$FACT" != fork,ifork,incarcs ]; then
    EXP="$EXP-${FACT//,/+}"
fi
[ "${EVAL_PILOT:-}" = 1 ] && EXP="$EXP-pilot"
for d in "$EXP" "$EXP-eval"; do
    if [ -e "$d" ]; then
        echo "$d exists: move or delete it before resubmitting this category" >&2
        exit 1
    fi
done

# The planner is compiled once into the shared revision cache. Array tasks
# that start together on different nodes must not compile at the same
# time (lab treats a cache entry without its build_successful marker as
# corrupted), so only the lowest index of this array compiles; the others
# wait for the marker, at most two hours, then try themselves (the build
# step also holds a lock file, which protects nodes that share flock).
CACHE="$DATA/revision-cache"
if [ "$SLURM_ARRAY_TASK_ID" != "${SLURM_ARRAY_TASK_MIN:-$SLURM_ARRAY_TASK_ID}" ]; then
    waited=0
    while ! ls "$CACHE"/*/build_successful >/dev/null 2>&1; do
        [ "$waited" -eq 0 ] && echo "waiting for array task $SLURM_ARRAY_TASK_MIN to compile the planner into $CACHE"
        if [ "$waited" -ge 7200 ]; then
            echo "no compiled planner in $CACHE after two hours; building here" >&2
            break
        fi
        sleep 60; waited=$((waited + 60))
    done
fi

echo "node $(hostname), category $EVAL_CATEGORY, $PROCS parallel runs, job memory $MEM_MB MiB, experiment $EXP, started $(date)"
uv run "./$SCRIPT" --all
echo "finished $(date)"
