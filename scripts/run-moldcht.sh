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
# Run the two-region conjugate heat transfer validation case: build the
# mesh, split it into the cavity and mould regions, run foamMultiRun and
# verify against the one-dimensional two-layer reference.
#
# Usage: run-moldcht.sh <caseDir>
#
# Must be invoked inside the OpenFOAM etc/bashrc environment.
#******************************************************************************

set -euo pipefail

caseDir=$(cd "$1" && pwd)
cd "$caseDir"

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

python3 "$(dirname "$0")/verify-moldcht.py" "$caseDir"
