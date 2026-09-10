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
# Run a two-region conjugate heat transfer validation case: build the
# mesh, split it into the cavity and mould regions, run foamMultiRun and
# hand over to the python verifier named by the case's system/verifier.
#
# Usage: run-moldcht.sh <caseDir>
#
# Must be invoked inside the OpenFOAM etc/bashrc environment.
#******************************************************************************

set -euo pipefail

scriptDir=$(cd "$(dirname "$0")" && pwd)
caseDir=$(cd "$1" && pwd)
cd "$caseDir"

[ -f system/verifier ] || {
    echo "error: $caseDir/system/verifier is missing" >&2
    exit 1
}

verifier=$(sed -n 's/^[[:space:]]*\([^[:space:]]*\)[[:space:]]*$/\1/p' system/verifier | head -1)

[ -n "$verifier" ] || {
    echo "error: $caseDir/system/verifier is empty" >&2
    exit 1
}

: "${WM_PROJECT_DIR:?run-moldcht.sh must be invoked inside the OpenFOAM environment}"

. "$WM_PROJECT_DIR/bin/tools/RunFunctions"

rm -rf postProcessing constant/polyMesh constant/cellToRegion \
       constant/cavity/polyMesh constant/mold/polyMesh log.* \
       0.[0-9]* [1-9]*

blockMesh > log.blockMesh 2>&1

topoSet -dict system/topoSetDict > log.topoSet 2>&1

splitMeshRegions -cellZones all > log.split 2>&1

foamMultiRun 2>&1 | tee log.foamRun

echo "=============================================================="
echo "moldingFoam CHT validation, verifying ..."
echo "=============================================================="

python3 "$scriptDir/$verifier" "$caseDir"
