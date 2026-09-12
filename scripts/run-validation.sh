#!/usr/bin/env bash
#******************************************************************************
# moldingFoam
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Run an analytic validation case and its verifier.
#
# The case's system/verifier file names the python verifier to run after
# the solve; the verifier receives the case directory as its argument.
#
# Usage: run-validation.sh <caseDir>
#
# Must be invoked inside the OpenFOAM etc/bashrc environment.
#******************************************************************************

set -euo pipefail

caseDir=$(cd "$1" && pwd)
cd "$caseDir"

: "${WM_PROJECT_DIR:?run-validation.sh must be invoked inside the OpenFOAM environment}"

. "$WM_PROJECT_DIR/bin/tools/RunFunctions"

[ -f system/verifier ] || {
    echo "error: $caseDir/system/verifier is missing" >&2
    exit 1
}

verifier=$(sed -n 's/^[[:space:]]*\([^[:space:]]*\)[[:space:]]*$/\1/p' system/verifier | head -1)

[ -n "$verifier" ] || {
    echo "error: $caseDir/system/verifier is empty" >&2
    exit 1
}

rm -rf postProcessing constant/polyMesh log.* 0.[0-9]* [1-9]*

blockMesh > log.blockMesh 2>&1

foamRun 2>&1 | tee log.foamRun

echo "=============================================================="
echo "moldingFoam validation case $(basename "$caseDir"), verifying ..."
echo "=============================================================="

if grep -qi "Duplicate entry" log.foamRun; then
    echo "error: duplicate runtime-selection entries (multiple module " \
         "copies loaded)" >&2
    exit 1
fi
# Heap-integrity guard: glibc reports at exit must fail the run even when
# the solver printed its End marker
if grep -qiE "malloc_consolidate|corrupted (fastbin|size)|free\(\): invalid" \
        log.foamRun; then
    echo "error: heap corruption reported at exit" >&2
    exit 1
fi

python3 "$(dirname "$0")/$verifier" "$caseDir"
