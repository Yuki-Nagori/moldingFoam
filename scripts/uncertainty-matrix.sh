#!/usr/bin/env bash
# Task-069 joint grid/time sensitivity matrix; not independent GCI evidence.
set -euo pipefail
mode="${1:---dry-run}"
[[ "$mode" == "--dry-run" || "$mode" == "--emit" || "$mode" == "--run" ]] || { echo "usage: $0 [--dry-run|--emit|--run]" >&2; exit 2; }
rows=(
  $'validation/thermoelastic\t0.5\t0.5\tfree-end-deflection-m'
  $'validation/thermoelastic\t1\t1\tfree-end-deflection-m'
  $'validation/thermoelastic\t2\t2\tfree-end-deflection-m'
  $'validation/moldCHT\t0.5\t0.5\tcavity-temperature-K'
  $'validation/moldCHT\t1\t1\tcavity-temperature-K'
  $'validation/moldCHT\t2\t2\tcavity-temperature-K'
  $'validation/warpagePlate\t0.5\t0.5\tfree-end-deflection-m'
  $'validation/warpagePlate\t1\t1\tfree-end-deflection-m'
  $'validation/warpagePlate\t2\t2\tfree-end-deflection-m'
)
printf 'case\tgridScale\tdtScale\tquantity\n'
printf '%b\n' "${rows[@]}"
[[ "$mode" != "--run" ]] && { [[ "$mode" == "--dry-run" ]] && echo "uncertainty-matrix: dry-run only" >&2; exit 0; }
if [[ -n "${UNCERTAINTY_MATRIX_LIMIT:-}" ]]; then
  [[ "$UNCERTAINTY_MATRIX_LIMIT" =~ ^[1-9]$ ]] || { echo "limit must be 1..9" >&2; exit 2; }
  rows=("${rows[@]:0:${UNCERTAINTY_MATRIX_LIMIT}}")
fi
repoDir=$(cd "$(dirname "$0")/.." && pwd)
outDir="${UNCERTAINTY_MATRIX_DIR:-$repoDir/.uncertainty-matrix}"
mkdir -p "$outDir"
outDir=$(cd "$outDir" && pwd)
# Unique session directories preserve prior runs and all failed inputs/outputs.
outDir=$(mktemp -d "$outDir/run-XXXXXXXX")
echo "uncertainty results=$outDir"
printf 'case\tgridScale\tdtScale\tquantity\trc\twall_seconds\tvalue\treference\trelative_error\tthreshold\twithin_threshold\ttime\n' > "$outDir/results.tsv"
overall=0
for row in "${rows[@]}"; do
  IFS=$'\t' read -r rel scale dtscale quantity <<< "$row"
  name=$(basename "$rel")
  work="$outDir/$name-$scale-$dtscale"
  mkdir "$work"
  cp -r "$repoDir/$rel"/. "$work/"
  runner=run-validation.sh
  [[ "$name" != moldCHT ]] || runner=run-moldcht.sh
  start=$(date +%s); rc=0
  (
    set -e
    python3 "$repoDir/scripts/prepare-uncertainty-case.py" "$work" "$name" "$scale" "$dtscale" || exit $?
    export UNCERTAINTY_MATRIX_MODE=1 UNCERTAINTY_METRICS_FILE="$work/metrics.json"
    timeout "${UNCERTAINTY_MATRIX_TIMEOUT:-900}" bash "$repoDir/scripts/$runner" "$work"
  ) > "$work/runner.log" 2>&1 || rc=$?
  wall=$(( $(date +%s)-start ))
  status=0
  python3 - "$work/metrics.json" "$outDir/results.tsv" "$rel" "$scale" "$dtscale" "$quantity" "$rc" "$wall" <<'PYROW' || status=$?
import csv, json, math, pathlib, sys
metrics, table, case, grid, dt, quantity, rc, wall = sys.argv[1:]
values = ['']*6
try:
    m = json.loads(pathlib.Path(metrics).read_text())
    values = [m[k] for k in ('value','reference','relative_error','threshold','within_threshold','time')]
    if m['quantity'] != quantity or not all(math.isfinite(v) for v in values):
        raise ValueError('invalid metrics')
except (OSError, ValueError, KeyError, TypeError):
    if rc == '0':
        rc = '1'
    values = ['']*6
with open(table, 'a') as output:
    csv.writer(output, delimiter='\t').writerow([case, grid, dt, quantity, rc, wall]+values)
sys.exit(int(rc) != 0)
PYROW
  [[ "$status" == 0 ]] || overall=1
  echo "$name grid=$scale dt=$dtscale solver_rc=$rc measurement_status=$status wall=${wall}s"
done
exit "$overall"
