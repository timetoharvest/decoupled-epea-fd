#!/bin/bash

set -u

BUILD="${1:-release32}"
FD="$(cd "$(dirname "$0")/../.." && pwd)"
BENCH="${DOWNWARD_BENCHMARKS:-$HOME/uni/bachelor/arbeit/downward-benchmarks}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

TASKS="
gripper/prob01.pddl
gripper/prob02.pddl
miconic/s3-0.pddl
miconic/s4-0.pddl
driverlog/p02.pddl
driverlog/p03.pddl
zenotravel/p02.pddl
zenotravel/p03.pddl
logistics00/probLOGISTICS-4-0.pddl
logistics00/probLOGISTICS-5-0.pddl
blocks/probBLOCKS-6-0.pddl
"
HEURISTICS="blind lmcut"

TIMEOUT=180
fail=0
total_alpha=0
printf '%-34s %-6s %-6s %8s %9s %9s %9s %7s %7s %8s\n' \
       task heur conf cost expanded evaluated generated distinct alpha maxopen

run_planner() {

    timeout "$TIMEOUT" python3 "$FD/fast-downward.py" --build "$BUILD" \
        --search-memory-limit 4G "$BENCH/$1" --search "$2" > "$3" 2>&1
}

get() { grep -m1 -oE "$2" "$1" | grep -oE '[0-9]+' | head -1; }

for task in $TASKS; do
  tname=$(basename "$task" .pddl)
  for h in $HEURISTICS; do
    declare -A cost expanded evaluated generated distinct alpha maxopen
    for conf in astar pea0 pea1; do
      case $conf in
        astar) search="astar($h())";;
        pea0)  search="pea_astar($h(), partial_expansion=false)";;
        pea1)  search="pea_astar($h())";;
      esac
      log="${tname}_${h}_${conf}.log"
      run_planner "$task" "$search" "$log"
      ec=$?
      if [ "$ec" != "0" ]; then
        echo "FAIL: $task $h $conf exited $ec (timeout or error)"
        fail=1
        continue
      fi
      cost[$conf]=$(get "$log" 'Plan cost: [0-9]+')
      expanded[$conf]=$(get "$log" 'Expanded [0-9]+ state')
      evaluated[$conf]=$(get "$log" 'Evaluated [0-9]+ state')
      generated[$conf]=$(get "$log" 'Generated [0-9]+ state')
      distinct[$conf]=$(get "$log" 'Distinct expansions: [0-9]+')
      alpha[$conf]=$(get "$log" 'Partial re-expansions .alpha.: [0-9]+')
      maxopen[$conf]=$(get "$log" 'Maximum open list size: [0-9]+')
      printf '%-34s %-6s %-6s %8s %9s %9s %9s %7s %7s %8s\n' \
        "$tname" "$h" "$conf" "${cost[$conf]:--}" "${expanded[$conf]:--}" \
        "${evaluated[$conf]:--}" "${generated[$conf]:--}" \
        "${distinct[$conf]:--}" "${alpha[$conf]:--}" "${maxopen[$conf]:--}"
    done

    if [ "${cost[astar]:-x}" != "${cost[pea0]:-y}" ] \
       || [ "${cost[astar]:-x}" != "${cost[pea1]:-z}" ]; then
      echo "FAIL: cost mismatch on $task/$h: astar=${cost[astar]:-?}" \
           "pea0=${cost[pea0]:-?} pea1=${cost[pea1]:-?}"
      fail=1
    fi
    for metric in expanded evaluated generated; do
      eval "a=\${${metric}[astar]:-x}; b=\${${metric}[pea0]:-y}"
      if [ "$a" != "$b" ]; then
        echo "FAIL: pea0 does not replicate astar on $task/$h:" \
             "$metric astar=$a pea0=$b"
        fail=1
      fi
    done
    for metric in evaluated generated; do
      eval "a=\${${metric}[astar]:-x}; b=\${${metric}[pea1]:-y}"
      if [ "$a" != "$b" ]; then

        echo "NOTE: pea1 $metric differs from astar on $task/$h:" \
             "astar=$a pea1=$b (pop-order effect, not a correctness issue)"
      fi
    done
    if [ -z "${maxopen[pea0]:-}" ] || [ -z "${maxopen[pea1]:-}" ]; then

      echo "FAIL: could not parse max open list size on $task/$h" \
           "(pea0='${maxopen[pea0]:-}' pea1='${maxopen[pea1]:-}')"
      fail=1
    elif [ "${maxopen[pea1]}" -gt "${maxopen[pea0]}" ]; then
      echo "FAIL: pea1 max open (${maxopen[pea1]}) exceeds pea0's" \
           "(${maxopen[pea0]}) on $task/$h"
      fail=1
    fi
    if [ -n "${distinct[pea1]:-}" ] && [ -n "${expanded[astar]:-}" ]; then
      delta=$(( distinct[pea1] - expanded[astar] ))
      if [ "$delta" -lt -1 ] || [ "$delta" -gt 1 ]; then
        echo "NOTE: distinct(pea1) vs expanded(astar) differ by $delta on" \
             "$task/$h (tie-breaking; hard-check is cost equality)"
      fi
    fi
    total_alpha=$(( total_alpha + ${alpha[pea1]:-0} ))
    unset cost expanded evaluated generated distinct alpha maxopen
  done
done

if [ "$total_alpha" -eq 0 ]; then
  echo "FAIL: alpha is 0 over all runs -- partial expansion never happened"
  fail=1
fi

if [ "$fail" -eq 0 ]; then
  echo "G2 PASS (build=$BUILD, total alpha=$total_alpha)"
else
  echo "G2 FAIL (build=$BUILD)"
fi
exit $fail
