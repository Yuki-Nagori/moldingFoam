#!/usr/bin/env bash
# 048 experiment: cost-lever combination on the contract case.
#
# Variants (all other settings identical to case-contract):
#   A  baseline        nSubCycles 16, energy tolerance 1e-6, maxAlphaCo 0.03
#   B  nSubCycles 8
#   C  energy tolerance 1e-5
#   D  both (8 + 1e-5)
#   E  both + maxAlphaCo 0.015 (dt halved: buys back the conservation margin)
#      E is the v1.28 contract default: use it as the baseline for F/G/H
#   F  E with alpha nCorrectors 1 (halve the explicit alpha correction passes)
#   G  E with MULESCorr yes (limiter-only corrections instead of explicit MULES)
#   H  F + G
#
# Each variant runs on its own scratch copy of case-contract (the contract
# case itself is never touched). Runs are interleaved and repeated so the
# wall clock comparison stays inside one VM session (cross-session spread
# is 20-25%, see ai-docs/tasks/032).
#
# Usage (inside the OpenFOAM environment, from the repo root):
#   bash scripts/lever-experiment.sh [runs...]      # default: A D A D

set -uo pipefail

scriptDir=$(cd "$(dirname "$0")" && pwd)
repoDir=$(cd "$scriptDir/.." && pwd)
caseSrc="$repoDir/case-contract"
root="${LEVER_EXP_DIR:-$HOME/lever-exp}"

export OMPI_MCA_rmaps_base_oversubscribe=1

variants=("$@")
[ "${#variants[@]}" -gt 0 ] || variants=(A D A D)

mkdir -p "$root"

for v in "${variants[@]}"
do
    nsub=16
    tol="1e-6"
    alphaCo=""
    alphaCorr=""
    mulesCorr=""
    case "$v" in
        A) ;;
        B) nsub=8 ;;
        C) tol="1e-5" ;;
        D) nsub=8; tol="1e-5" ;;
        E) nsub=8; tol="1e-5"; alphaCo=0.015 ;;
        F) nsub=8; tol="1e-5"; alphaCo=0.015; alphaCorr=1 ;;
        G) nsub=8; tol="1e-5"; alphaCo=0.015; mulesCorr=yes ;;
        H) nsub=8; tol="1e-5"; alphaCo=0.015; alphaCorr=1; mulesCorr=yes ;;
        *) echo "unknown variant '$v'" >&2; exit 1 ;;
    esac

    stamp=$(date +%Y%m%d-%H%M%S)
    work="$root/${v}-${stamp}"
    out="$root/${v}-${stamp}.out"

    cp -r "$caseSrc" "$work"
    rm -rf "$work"/processor* "$work"/postProcessing "$work"/constant/polyMesh \
        "$work"/log.* "$work"/0.[0-9]* "$work"/[1-9]*

    python3 - "$work/system/fvSolution" "$nsub" "$tol" "$alphaCorr" "$mulesCorr" <<'PY'
import re, sys
path, nsub, tol, alphaCorr, mulesCorr = (
    sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
src = open(path).read()

def patch_block(text, header_re, key, value):
    m = re.search(header_re, text)
    if not m:
        raise SystemExit(f"block {header_re!r} not found")
    start = m.end()
    depth = 1
    i = start
    while depth:
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
        i += 1
    block = text[start:i]
    if not re.search(rf'\b{key}\s+', block):
        raise SystemExit(f"{key} not found in block")
    new = re.sub(rf'(\b{key}\s+)[^\s;]+;', lambda mo: mo.group(1) + value + ';', block, count=1)
    return text[:start] + new + text[i:]

src = patch_block(src, r'"alpha\.melt\.\*"\s*\{', 'nSubCycles', nsub)
if alphaCorr:
    src = patch_block(src, r'"alpha\.melt\.\*"\s*\{', 'nCorrectors', alphaCorr)
if mulesCorr:
    src = patch_block(src, r'"alpha\.melt\.\*"\s*\{', 'MULESCorr', mulesCorr)
src = patch_block(src, r'"\(U\|e\|T\)\.\*"\s*\{', 'tolerance', tol)
open(path, 'w').write(src)
PY

    if [ -n "$alphaCo" ]
    then
        python3 - "$work/system/controlDict" "$alphaCo" <<'PATCH'
import re, sys
path, value = sys.argv[1], sys.argv[2]
src = open(path).read()
if not re.search(r'(?m)^maxAlphaCo\s+', src):
    raise SystemExit("maxAlphaCo not found in controlDict")
new = re.sub(r'(?m)^(maxAlphaCo\s+)[^\s;]+;', lambda mo: mo.group(1) + value + ';', src, count=1)
open(path, 'w').write(new)
PATCH
    fi

    echo "=== variant $v (nSubCycles $nsub, tol $tol, maxAlphaCo ${alphaCo:-contract}) -> $work"
    start=$(date +%s)
    bash "$scriptDir/run-case.sh" "$work" 4 > "$out" 2>&1
    rc=$?
    wall=$(( $(date +%s) - start ))

    steps=$(grep -c "^Time = " "$work/log.foamRun" 2>/dev/null || echo 0)
    exec_time=$(grep "ExecutionTime" "$work/log.foamRun" 2>/dev/null | tail -1 | sed 's/.*ExecutionTime = \([0-9.]*\) s.*/\1/')
    cons=$(grep -E "Mass conservation relative error" "$out" | tail -1 | sed 's/.*error = //')
    accept=$(grep -c "All acceptance checks passed" "$out")

    echo "RESULT variant=$v alphaCo=${alphaCo:-contract} rc=$rc wall=${wall}s steps=$steps execTime=${exec_time:-?}s masscon=${cons:-?} accepted=$accept" | tee -a "$root/results.txt"
done

echo "=== results ($root/results.txt) ==="
cat "$root/results.txt"
