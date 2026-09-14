#!/usr/bin/env bash
# 047: contract-case grid / time-step convergence study.
#
# Copies case-contract to a scratch directory, rescales the in-plane mesh
# resolution by a factor (and, optionally, relaxes the time-step controls)
# and runs the full acceptance path (blockMesh -> decompose -> foamRun ->
# reconstruct -> verify-case.py). The contract case itself is never
# touched.
#
# Usage (inside the OpenFOAM environment, from the repo root):
#   bash scripts/contract-grid-study.sh <scale> [options]
#
#   scale        in-plane resolution multiplier applied to every block
#                (0.5 = coarse, 1 = contract mesh 174x40x2, 2 = fine)
#   --alphaCo X  override controlDict maxAlphaCo (default: contract value)
#   --maxDeltaT Y  override controlDict maxDeltaT
#   --nprocs N   subdomains (default 4; the fine grid may use 8)
#   --tag NAME   result tag (default: scaleN)
#
# Result line (appended to $GRID_STUDY_DIR/results.txt):
#   RESULT tag=.. scale=.. alphaCo=.. maxDeltaT=.. nprocs=.. rc=.. wall=..s
#          steps=.. execTime=.. masscon=.. switch=.. accepted=..

set -uo pipefail

scriptDir=$(cd "$(dirname "$0")" && pwd)
repoDir=$(cd "$scriptDir/.." && pwd)
caseSrc="$repoDir/case-contract"
root="${GRID_STUDY_DIR:-$HOME/grid-study}"

export OMPI_MCA_rmaps_base_oversubscribe=1

scale=""
alphaCo=""
maxDeltaT=""
nprocs=4
tag=""

while [ "$#" -gt 0 ]
do
    case "$1" in
        --alphaCo) alphaCo="$2"; shift 2 ;;
        --maxDeltaT) maxDeltaT="$2"; shift 2 ;;
        --nprocs) nprocs="$2"; shift 2 ;;
        --tag) tag="$2"; shift 2 ;;
        *) scale="$1"; shift ;;
    esac
done

[ -n "$scale" ] || { echo "usage: $0 <scale> [--alphaCo X] [--maxDeltaT Y] [--nprocs N] [--tag NAME]" >&2; exit 2; }
[ -n "$tag" ] || tag="scale${scale}"

mkdir -p "$root"
stamp=$(date +%Y%m%d-%H%M%S)
work="$root/${tag}-${stamp}"
out="$root/${tag}-${stamp}.out"

cp -r "$caseSrc" "$work"
rm -rf "$work"/processor* "$work"/postProcessing "$work"/constant/polyMesh \
    "$work"/log.* "$work"/0.[0-9]* "$work"/[1-9]*

python3 - "$work/system/blockMeshDict" "$scale" <<'PY'
import re, sys
path, scale = sys.argv[1], float(sys.argv[2])
src = open(path).read()

m = re.search(r'\bblocks\b\s*\(\s*(.*?)\n\s*\)\s*;', src, re.S)
if not m:
    raise SystemExit("blocks section not found in blockMeshDict")

def rescale(mo):
    nx, ny, nz = (int(mo.group(i)) for i in (2, 3, 4))
    sx = max(1, round(nx*scale))
    sy = max(1, round(ny*scale))
    return mo.group(1) + f"({sx} {sy} {nz})"

# Only the resolution triple that follows the hex vertex list is scaled:
# a bare 3-tuple pattern would also hit simpleGrading (1 1 1)
new = re.sub(r'(\)\s*)\(\s*(\d+)\s+(\d+)\s+(\d+)\s*\)', rescale, m.group(1))
if new == m.group(1):
    raise SystemExit("no block resolution changed (bad scale?)")
src = src[:m.start(1)] + new + src[m.end(1):]
open(path, 'w').write(src)

counts = [tuple(int(x) for x in c) for c in re.findall(r'\)\s*\(\s*(\d+)\s+(\d+)\s+(\d+)\s*\)', new)]
nx = sum(c[0] for c in counts)
ny = counts[0][1]
nz = counts[0][2]
print(f"grid: {nx} x {ny} x {nz} = {nx*ny*nz} cells")
PY

python3 - "$work/system/controlDict" "${alphaCo:-}" "${maxDeltaT:-}" <<'PY'
import re, sys
path, alphaCo, maxDeltaT = sys.argv[1], sys.argv[2], sys.argv[3]
src = open(path).read()
for key, value in (("maxAlphaCo", alphaCo), ("maxDeltaT", maxDeltaT)):
    if value:
        new = re.sub(rf'(?m)^({key}\s+)[^\s;]+;', lambda mo: mo.group(1) + value + ';', src, count=1)
        if new == src:
            raise SystemExit(f"{key} not found in controlDict")
        src = new
open(path, 'w').write(src)
PY

echo "=== grid study tag=$tag scale=$scale alphaCo=${alphaCo:-contract} maxDeltaT=${maxDeltaT:-contract} nprocs=$nprocs"
echo "    work=$work"
start=$(date +%s)
bash "$scriptDir/run-case.sh" "$work" "$nprocs" > "$out" 2>&1
rc=$?
wall=$(( $(date +%s) - start ))

steps=$(grep -c "^Time = " "$work/log.foamRun" 2>/dev/null || echo 0)
exec_time=$(grep "ExecutionTime" "$work/log.foamRun" 2>/dev/null | tail -1 | sed 's/.*ExecutionTime = \([0-9.]*\) s.*/\1/')
cons=$(grep -E "Mass conservation relative error" "$out" | tail -1 | sed 's/.*error = //')
switch=$(grep -E "V/P switch" "$work/log.foamRun" 2>/dev/null | head -1 | sed 's/.*filled fraction = \([0-9.e+-]*\).*at t = \([0-9.e+-]*\) s/fill=\1 t=\2/')
accepted=$(grep -c "All acceptance checks passed" "$out")

echo "RESULT tag=$tag scale=$scale alphaCo=${alphaCo:-contract} maxDeltaT=${maxDeltaT:-contract} nprocs=$nprocs rc=$rc wall=${wall}s steps=$steps execTime=${exec_time:-?}s masscon=${cons:-?} ${switch:-no-switch} accepted=$accepted" \
    | tee -a "$root/results.txt"
