#!/usr/bin/env bash
# Bounded single-variable diagnostics for task 070.
set -euo pipefail
caseDir=$(cd "${1:?usage: $0 <case> [output.tsv]}" && pwd)
output=${2:-structural-convergence.tsv}
timeoutS=${STRUCTURAL_DIAG_TIMEOUT:-120}
repoDir=$(cd "$(dirname "$0")/../.." && pwd)
if [[ -z "${WM_PROJECT_DIR:-}" ]]; then
  echo 'error: source OpenFOAM-14 etc/bashrc before running diagnostics' >&2
  exit 2
fi
mkdir -p "$(dirname "$output")"
workRoot="$(dirname "$output")/structural-convergence-work-$$"
mkdir "$workRoot"
echo "diagnostic work root: $workRoot" >&2
printf 'variant\tchange\trc\twall_seconds\tfinal_time\tconvergence\tlog\n' > "$output"
for item in $'A0\tbaseline' $'B1\tD.relTol=0' $'B2\taccelerationFactor=1.0' $'B3\tnCorrectors=3'; do
  IFS=$'\t' read -r variant change <<< "$item"
  work="$workRoot/$variant"; cp -r "$caseDir" "$work"
  case "$variant" in
    B1) sed -i 's/relTol[[:space:]]\+0\.99/relTol 0/' "$work/system/fvSolution" ;;
    B2) sed -i 's/accelerationFactor[[:space:]]\+1\.9/accelerationFactor 1.0/' "$work/system/fvSolution" ;;
    B3) sed -i '/compactNormalStress/a\    nCorrectors 3;' "$work/system/fvSolution" ;;
  esac
  start=$(date +%s); rc=0
  (cd "$work" && timeout "$timeoutS" bash "$repoDir/scripts/run-validation.sh" "$work") > "$work/runner.log" 2>&1 || rc=$?
  wall=$(( $(date +%s) - start )); final=$(grep -E '^Time = [0-9.]+s' "$work/runner.log" | tail -1 | sed -E 's/.*= ([0-9.]+)s/\1/' || true)
  convergence=unavailable
  if [[ -f "$work/log.foamRun" ]]; then
    python3 "$repoDir/scripts/check-structural-convergence.py" "$work" > "$work/convergence.json" 2>&1 && convergence=converged || convergence=not_converged
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$variant" "$change" "$rc" "$wall" "${final:-unavailable}" "$convergence" "$work/runner.log" >> "$output"
done
awk -F '\t' 'NR > 1 && ($3 != 0 || $6 == "not_converged") {bad=1} END {exit bad}' "$output"
