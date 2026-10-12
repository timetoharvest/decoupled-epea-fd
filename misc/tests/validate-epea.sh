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

TIMEOUT=180
fail=0
total_alpha_direct=0
total_osfdef=0
total_gen_pea1=0
total_gen_direct=0
printf '%-34s %-9s %8s %9s %9s %9s %7s %7s %8s %7s %7s\n' \
       task conf cost expanded evaluated generated distinct alpha maxopen beta osfdef

run_planner() {

    timeout "$TIMEOUT" python3 "$FD/fast-downward.py" --build "$BUILD" \
        --search-memory-limit 4G "$BENCH/$1" --search "$2" > "$3" 2>&1
}

get() { grep -m1 -oE "$2" "$1" | grep -oE '[0-9]+' | head -1; }

for task in $TASKS; do
  tname=$(basename "$task" .pddl)
  declare -A cost expanded evaluated generated distinct alpha maxopen beta osfdef
  for conf in astar pea0 pea1 fullcheck direct verify; do
    case $conf in
      astar)     search="astar(pdb())";;
      pea0)      search="pea_astar(pdb(), partial_expansion=false)";;
      pea1)      search="pea_astar(pdb())";;
      fullcheck) search="pea_astar(pdb(), osf=full_checking)";;
      direct)    search="epea_astar(pdb())";;
      verify)    search="epea_astar(pdb(), verify_osf=true)";;
    esac
    log="${tname}_${conf}.log"
    run_planner "$task" "$search" "$log"
    ec=$?
    if [ "$ec" != "0" ]; then
      echo "FAIL: $task $conf exited $ec (timeout, error, or verify mismatch)"
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
    beta[$conf]=$(get "$log" 'Deferred surplus edges .beta.: [0-9]+')
    osfdef[$conf]=$(get "$log" 'Deferred surplus operators .osf.: [0-9]+')
    printf '%-34s %-9s %8s %9s %9s %9s %7s %7s %8s %7s %7s\n' \
      "$tname" "$conf" "${cost[$conf]:--}" "${expanded[$conf]:--}" \
      "${evaluated[$conf]:--}" "${generated[$conf]:--}" \
      "${distinct[$conf]:--}" "${alpha[$conf]:--}" "${maxopen[$conf]:--}" \
      "${beta[$conf]:--}" "${osfdef[$conf]:--}"
  done

  for conf in pea0 pea1 fullcheck direct verify; do
    if [ "${cost[astar]:-x}" != "${cost[$conf]:-y}" ]; then
      echo "FAIL: cost mismatch on $task: astar=${cost[astar]:-?}" \
           "$conf=${cost[$conf]:-?}"
      fail=1
    fi
  done
  for metric in expanded evaluated generated; do
    eval "a=\${${metric}[astar]:-x}; b=\${${metric}[pea0]:-y}"
    if [ "$a" != "$b" ]; then
      echo "FAIL: pea0 does not replicate astar on $task: $metric a=$a b=$b"
      fail=1
    fi
  done

  for metric in expanded evaluated generated distinct alpha maxopen beta; do
    eval "a=\${${metric}[pea1]:-x}; b=\${${metric}[fullcheck]:-y}"
    if [ "$a" != "$b" ]; then
      echo "FAIL: fullcheck differs from pea1 on $task: $metric" \
           "pea1=$a fullcheck=$b"
      fail=1
    fi
  done

  for metric in evaluated generated; do
    eval "p=\${${metric}[pea1]:-}; d=\${${metric}[direct]:-}"
    if [ -z "$p" ] || [ -z "$d" ]; then
      echo "FAIL: could not parse $metric for pea1/direct on $task"
      fail=1
    elif [ "$d" -gt "$p" ]; then
      echo "FAIL: direct $metric ($d) exceeds pea1's ($p) on $task"
      fail=1
    fi
  done

  if [ -n "${expanded[direct]:-}" ] && [ -n "${distinct[direct]:-}" ] \
     && [ -n "${alpha[direct]:-}" ]; then
    if [ "${expanded[direct]}" != \
         "$(( distinct[direct] + alpha[direct] + 1 ))" ]; then
      echo "FAIL: pop identity broken on $task (direct):" \
           "${expanded[direct]} != ${distinct[direct]} +" \
           "${alpha[direct]} + 1"
      fail=1
    fi
  else
    echo "FAIL: could not parse pop-identity inputs on $task (direct)"
    fail=1
  fi

  if [ -z "${osfdef[direct]:-}" ] || [ -z "${beta[pea1]:-}" ]; then
    echo "FAIL: could not parse osfdef/beta on $task"
    fail=1
  elif [ "${osfdef[direct]}" -lt "${beta[pea1]}" ]; then
    echo "FAIL: osf-beta (${osfdef[direct]}) below beta (${beta[pea1]})" \
         "on $task"
    fail=1
  fi

  for metric in distinct alpha maxopen; do
    eval "d=\${${metric}[direct]:-x}; v=\${${metric}[verify]:-y}"
    if [ "$d" != "$v" ]; then
      echo "FAIL: verify trajectory drifts from direct on $task: $metric" \
           "direct=$d verify=$v"
      fail=1
    fi
  done

  if [ -n "${maxopen[direct]:-}" ] && [ -n "${maxopen[pea0]:-}" ] \
     && [ "${maxopen[direct]}" -gt "${maxopen[pea0]}" ]; then
    echo "NOTE: direct max open (${maxopen[direct]}) exceeds the A*" \
         "baseline's (${maxopen[pea0]}) on $task (JAIR-9 inversion)"
  fi
  total_alpha_direct=$(( total_alpha_direct + ${alpha[direct]:-0} ))
  total_osfdef=$(( total_osfdef + ${osfdef[direct]:-0} ))
  total_gen_pea1=$(( total_gen_pea1 + ${generated[pea1]:-0} ))
  total_gen_direct=$(( total_gen_direct + ${generated[direct]:-0} ))
  unset cost expanded evaluated generated distinct alpha maxopen beta osfdef
done

for task in $TASKS; do
  tname=$(basename "$task" .pddl)
  run_planner "$task" "pea_astar(lmcut())" "${tname}_lm_pea1.log"
  ec1=$?
  run_planner "$task" "pea_astar(lmcut(), osf=full_checking)" \
              "${tname}_lm_fc.log"
  ec2=$?
  if [ "$ec1" != "0" ] || [ "$ec2" != "0" ]; then
    echo "FAIL: lmcut parity pair exited $ec1/$ec2 on $task"
    fail=1
    continue
  fi
  for pat in 'Plan cost: [0-9]+' 'Expanded [0-9]+ state' \
             'Evaluated [0-9]+ state' 'Generated [0-9]+ state' \
             'Distinct expansions: [0-9]+' \
             'Partial re-expansions .alpha.: [0-9]+' \
             'Maximum open list size: [0-9]+'; do
    a=$(get "${tname}_lm_pea1.log" "$pat")
    b=$(get "${tname}_lm_fc.log" "$pat")
    if [ -z "$a" ] || [ "$a" != "$b" ]; then
      echo "FAIL: lmcut fullcheck parity broken on $task ('$pat': $a vs $b)"
      fail=1
    fi
  done
done

guard() {

    if [ -n "${4:-}" ]; then
        timeout "$TIMEOUT" python3 "$FD/fast-downward.py" --build "$BUILD" \
            "$BENCH/gripper/prob01.pddl" --decoupling "$4" --search "$2" \
            > "guard_$1.log" 2>&1
    else
        timeout "$TIMEOUT" python3 "$FD/fast-downward.py" --build "$BUILD" \
            "$BENCH/gripper/prob01.pddl" --search "$2" \
            > "guard_$1.log" 2>&1
    fi
    if [ "$?" != "34" ]; then
        echo "FAIL: guard $1 did not exit 34"
        fail=1
    elif ! grep -q "$3" "guard_$1.log"; then
        echo "FAIL: guard $1 exited 34 but not via its own message ($3)"
        fail=1
    fi
}
guard nonpdb   "epea_astar(lmcut())" \
               "requires eval to be a single pdb"
guard degen    "pea_astar(pdb(), osf=direct_pdb, partial_expansion=false)" \
               "requires partial_expansion=true"
guard verifyno "pea_astar(pdb(), verify_osf=true)" \
               "verify_osf=true requires osf=direct_pdb"

guard decoup   "epea_astar(pdb())" \
               "requires include_price_in_g=false in the pruning option" \
               "incarcs(min_num_leaves=1)"

if [ "$total_alpha_direct" -eq 0 ]; then
  echo "FAIL: direct alpha is 0 over all runs -- layers were never served"
  fail=1
fi
if [ "$total_osfdef" -eq 0 ]; then
  echo "FAIL: no operator was ever parked -- the OSF never engaged"
  fail=1
fi
if [ "$total_gen_direct" -ge "$total_gen_pea1" ]; then
  echo "FAIL: total Generated direct ($total_gen_direct) not strictly" \
       "below pea1's ($total_gen_pea1)"
  fail=1
fi

if [ "$fail" -eq 0 ]; then
  echo "G4-explicit PASS (build=$BUILD, direct alpha=$total_alpha_direct," \
       "parked ops=$total_osfdef," \
       "generated pea1=$total_gen_pea1 vs direct=$total_gen_direct)"
else
  echo "G4-explicit FAIL (build=$BUILD)"
fi
exit $fail
