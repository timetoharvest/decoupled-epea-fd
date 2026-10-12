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
zenotravel/p03.pddl
logistics00/probLOGISTICS-4-0.pddl
"
MAIN_DECOUPLINGS="ifork incarcs"

CP="pdb(pattern=greedy(max_states=1000000, pattern_filter=n_leaves_patterns(n=0)))"
PRUNE="pruning=dominance(include_price_in_g=false)"

TIMEOUT=300
fail=0
total_alpha_direct=0
total_osfdef=0
total_gen_pea1=0
total_gen_direct=0
printf '%-34s %-8s %-7s %6s %9s %9s %9s %8s %7s %8s %7s\n' \
       task dec conf cost expanded evaluated generated distinct alpha maxopen osfdef

run_planner() {

    timeout "$TIMEOUT" python3 "$FD/fast-downward.py" --build "$BUILD" \
        --search-memory-limit 4G "$BENCH/$1" --decoupling "$2" \
        --search "$3" > "$4" 2>&1
}

get() { grep -m1 -oE "$2" "$1" | grep -oE '[0-9]+' | head -1; }

for task in $TASKS; do
  tname=$(basename "$task" .pddl)
  for dec in $MAIN_DECOUPLINGS; do
    decstr="${dec}(min_num_leaves=1, ${PRUNE})"
    declare -A cost expanded evaluated generated distinct alpha maxopen osfdef
    for conf in astar pea0 pea1 direct verify; do
      case $conf in
        astar)  search="astar($CP)";;
        pea0)   search="pea_astar($CP, partial_expansion=false)";;
        pea1)   search="pea_astar($CP)";;
        direct) search="epea_astar($CP)";;
        verify) search="epea_astar($CP, verify_osf=true)";;
      esac
      log="${tname}_${dec}_${conf}.log"
      run_planner "$task" "$decstr" "$search" "$log"
      ec=$?
      if [ "$ec" != "0" ]; then
        echo "FAIL: $task $dec $conf exited $ec (timeout, error, or verify mismatch)"
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
      osfdef[$conf]=$(get "$log" 'Deferred surplus operators .osf.: [0-9]+')
      printf '%-34s %-8s %-7s %6s %9s %9s %9s %8s %7s %8s %7s\n' \
        "$tname" "$dec" "$conf" "${cost[$conf]:--}" "${expanded[$conf]:--}" \
        "${evaluated[$conf]:--}" "${generated[$conf]:--}" \
        "${distinct[$conf]:--}" "${alpha[$conf]:--}" "${maxopen[$conf]:--}" \
        "${osfdef[$conf]:--}"
    done

    for conf in pea0 pea1 direct verify; do
      if [ "${cost[astar]:-x}" != "${cost[$conf]:-y}" ]; then
        echo "FAIL: cost mismatch on $task/$dec: astar=${cost[astar]:-?}" \
             "$conf=${cost[$conf]:-?}"
        fail=1
      fi
    done
    for metric in expanded evaluated generated; do
      eval "a=\${${metric}[astar]:-x}; b=\${${metric}[pea0]:-y}"
      if [ "$a" != "$b" ]; then
        echo "FAIL: pea0 does not replicate astar on $task/$dec: $metric" \
             "a=$a b=$b"
        fail=1
      fi
    done

    for conf in pea1 direct; do
      eval "e=\${expanded[$conf]:-}; d=\${distinct[$conf]:-}; al=\${alpha[$conf]:-}"
      if [ -z "$e" ] || [ -z "$d" ] || [ -z "$al" ]; then
        echo "FAIL: could not parse pop-identity inputs on $task/$dec ($conf)"
        fail=1
      elif [ "$e" != "$(( d + al ))" ]; then
        echo "FAIL: SDA pop identity broken on $task/$dec ($conf):" \
             "$e != $d + $al"
        fail=1
      fi
    done

    for metric in evaluated generated; do
      eval "p=\${${metric}[pea1]:-}; d=\${${metric}[direct]:-}"
      if [ -z "$p" ] || [ -z "$d" ]; then
        echo "FAIL: could not parse $metric for pea1/direct on $task/$dec"
        fail=1
      elif [ "$d" -gt "$p" ]; then
        echo "NOTE: direct $metric ($d) exceeds pea1's ($p) on $task/$dec" \
             "(SDA layer-serving effect -- thesis-relevant observation)"
      fi
    done

    for metric in distinct alpha maxopen; do
      eval "d=\${${metric}[direct]:-x}; v=\${${metric}[verify]:-y}"
      if [ "$d" != "$v" ]; then
        echo "FAIL: verify trajectory drifts from direct on $task/$dec:" \
             "$metric direct=$d verify=$v"
        fail=1
      fi
    done
    if [ -z "${osfdef[direct]:-}" ]; then
      echo "FAIL: could not parse parked-operator count on $task/$dec"
      fail=1
    elif [ "${osfdef[direct]}" -eq 0 ]; then
      echo "NOTE: no operator parked on $task/$dec (degenerate layering)"
    fi
    total_alpha_direct=$(( total_alpha_direct + ${alpha[direct]:-0} ))
    total_osfdef=$(( total_osfdef + ${osfdef[direct]:-0} ))
    total_gen_pea1=$(( total_gen_pea1 + ${generated[pea1]:-0} ))
    total_gen_direct=$(( total_gen_direct + ${generated[direct]:-0} ))
    unset cost expanded evaluated generated distinct alpha maxopen osfdef
  done
done

for task in $TASKS; do
  tname=$(basename "$task" .pddl)
  decstr="fork(min_num_leaves=1, ${PRUNE})"
  declare -A cost expanded distinct alpha
  for conf in astar pea1 direct; do
    case $conf in
      astar)  search="astar($CP)";;
      pea1)   search="pea_astar($CP)";;
      direct) search="epea_astar($CP)";;
    esac
    log="${tname}_fork_${conf}.log"
    run_planner "$task" "$decstr" "$search" "$log"
    if [ "$?" != "0" ]; then
      echo "FAIL: fork control $task $conf exited nonzero"
      fail=1
      continue
    fi
    cost[$conf]=$(get "$log" 'Plan cost: [0-9]+')
    expanded[$conf]=$(get "$log" 'Expanded [0-9]+ state')
    distinct[$conf]=$(get "$log" 'Distinct expansions: [0-9]+')
    alpha[$conf]=$(get "$log" 'Partial re-expansions .alpha.: [0-9]+')
  done
  for conf in pea1 direct; do
    if [ "${cost[astar]:-x}" != "${cost[$conf]:-y}" ]; then
      echo "FAIL: fork control cost mismatch on $task: astar=${cost[astar]:-?}" \
           "$conf=${cost[$conf]:-?}"
      fail=1
    fi
    eval "e=\${expanded[$conf]:-}; d=\${distinct[$conf]:-}; al=\${alpha[$conf]:-}"
    if [ -z "$e" ] || [ -z "$d" ] || [ -z "$al" ]; then

      echo "FAIL: could not parse fork-control pop-identity inputs on" \
           "$task ($conf)"
      fail=1
    elif [ "$e" != "$(( d + al ))" ] && [ "$e" != "$(( d + al + 1 ))" ]; then
      echo "FAIL: fork control pop identity broken on $task ($conf)"
      fail=1
    fi
  done
  unset cost expanded distinct alpha
done

guard() {

    timeout "$TIMEOUT" python3 "$FD/fast-downward.py" --build "$BUILD" \
        "$BENCH/gripper/prob01.pddl" --decoupling "$2" --search "$3" \
        > "guard_$1.log" 2>&1
    if [ "$?" != "34" ]; then
        echo "FAIL: guard $1 did not exit 34"
        fail=1
    elif ! grep -q "$4" "guard_$1.log"; then
        echo "FAIL: guard $1 exited 34 but not via its own message ($4)"
        fail=1
    fi
}
guard adaptation-on "incarcs(min_num_leaves=1)"                       "epea_astar($CP)" "requires include_price_in_g=false in the pruning option"
guard leaf-pattern  "incarcs(min_num_leaves=1, ${PRUNE})"             "epea_astar(pdb())" "CENTER ONLY pattern"
guard frontier      "incarcs(min_num_leaves=1, pruning=cost_frontier())" "epea_astar($CP)" "plain dominance/duplicate pruning flavors"

if [ "$total_osfdef" -eq 0 ]; then
  echo "FAIL: no operator was ever parked -- the OSF never engaged"
  fail=1
fi
if [ "$total_alpha_direct" -eq 0 ]; then
  echo "FAIL: direct alpha is 0 over all runs -- layers were never served"
  fail=1
fi
if [ "$total_gen_direct" -ge "$total_gen_pea1" ]; then
  echo "FAIL: total Generated direct ($total_gen_direct) not strictly" \
       "below pea1's ($total_gen_pea1)"
  fail=1
fi

if [ "$fail" -eq 0 ]; then
  echo "G5-decoupled PASS (build=$BUILD, direct alpha=$total_alpha_direct," \
       "parked ops=$total_osfdef," \
       "generated pea1=$total_gen_pea1 vs direct=$total_gen_direct)"
else
  echo "G5-decoupled FAIL (build=$BUILD)"
fi
exit $fail
