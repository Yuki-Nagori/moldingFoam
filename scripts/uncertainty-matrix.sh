#!/usr/bin/env bash
# Task-069 three-grid/three-time-step matrix.
set -euo pipefail
mode="${1:---dry-run}"
[[ "$mode" == "--dry-run" || "$mode" == "--emit" || "$mode" == "--run" ]] || { echo "usage: $0 [--dry-run|--emit|--run]" >&2; exit 2; }
rows=(
  $'validation/thermoelastic\t0.5\t0.5\tfree-end-deflection'
  $'validation/thermoelastic\t1\t1\tfree-end-deflection'
  $'validation/thermoelastic\t2\t2\tfree-end-deflection'
  $'validation/moldCHT\t0.5\t0.5\tboundary-heat-flux'
  $'validation/moldCHT\t1\t1\tboundary-heat-flux'
  $'validation/moldCHT\t2\t2\tboundary-heat-flux'
  $'validation/warpagePlate\t0.5\t0.5\tfree-end-deflection'
  $'validation/warpagePlate\t1\t1\tfree-end-deflection'
  $'validation/warpagePlate\t2\t2\tfree-end-deflection'
)
printf 'case\tgridScale\tdtScale\tquantity\n'
printf '%b\n' "${rows[@]}"
[[ "$mode" != "--run" ]] && { [[ "$mode" == "--dry-run" ]] && echo "uncertainty-matrix: dry-run only" >&2; exit 0; }
if [[ -n "${UNCERTAINTY_MATRIX_LIMIT:-}" ]]; then
  rows=("${rows[@]:0:${UNCERTAINTY_MATRIX_LIMIT}}")
fi
repoDir=$(cd "$(dirname "$0")/.." && pwd)
outDir="${UNCERTAINTY_MATRIX_DIR:-$repoDir/.uncertainty-matrix}"
mkdir -p "$outDir"
printf 'case\tgridScale\tdtScale\tquantity\trc\twall\n' > "$outDir/results.tsv"
for row in "${rows[@]}"; do
  IFS=$'\t' read -r rel scale dtscale quantity <<< "$row"
  work=$(mktemp -d)
  cp -r "$repoDir/$rel"/. "$work/"
  python3 - "$work/system/blockMeshDict" "$work/system/controlDict" "$scale" "$dtscale" <<'PY'
import re, sys
mesh, ctrl, scale, dtscale = sys.argv[1],sys.argv[2],float(sys.argv[3]),float(sys.argv[4])
s=open(mesh).read(); m=re.search(r'\bblocks\b\s*\(\s*(.*?)\n\s*\)\s*;',s,re.S)
if not m: raise SystemExit('blocks section missing')
def f(x): return x.group(1)+f'({max(1,round(int(x.group(2))*scale))} {max(1,round(int(x.group(3))*scale))} {x.group(4)})'
new=re.sub(r'(\)\s*)\(\s*(\d+)\s+(\d+)\s+(\d+)\s*\)',f,m.group(1))
if new==m.group(1): raise SystemExit('mesh resolution unchanged')
open(mesh,'w').write(s[:m.start(1)]+new+s[m.end(1):])
c=open(ctrl).read(); hit=[m for m in re.finditer(r'(?m)^(deltaT\s+)([^;]+);',c)]
if not hit: raise SystemExit('deltaT missing')
old=float(hit[0].group(2)); c=c[:hit[0].start()]+hit[0].group(1)+format(old*dtscale,'.12g')+';'+c[hit[0].end():]; open(ctrl,'w').write(c)
PY
  start=$(date +%s); rc=0
  (cd "$work" && timeout "${UNCERTAINTY_MATRIX_TIMEOUT:-900}" bash "$repoDir/scripts/run-validation.sh" "$work") > "$outDir/$(basename "$rel")-${scale}-${dtscale}.log" 2>&1 || rc=$?
  wall=$(( $(date +%s)-start ))
  printf '%s\t%s\t%s\t%s\t%s\t%ss\n' "$rel" "$scale" "$dtscale" "$quantity" "$rc" "$wall" >> "$outDir/results.tsv"
  rm -rf "$work"
  [[ "$rc" == 0 ]] || exit "$rc"
done
