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
# Run a validation case with the moldingFoam solver: clean, blockMesh,
# foamRun and verification.
#
# Usage: run-couette.sh <caseDir>
#
# Must be invoked inside the OpenFOAM etc/bashrc environment.
#******************************************************************************

set -euo pipefail

caseDir=$(cd "$1" && pwd)
cd "$caseDir"

: "${WM_PROJECT_DIR:?run-couette.sh must be invoked inside the OpenFOAM environment}"

. "$WM_PROJECT_DIR/bin/tools/RunFunctions"

rm -rf postProcessing constant/polyMesh log.* 0.[0-9]* [1-9]*

blockMesh > log.blockMesh 2>&1

foamRun 2>&1 | tee log.foamRun

echo "=============================================================="
echo "moldingFoam Couette shear-heating validation, verifying ..."
echo "=============================================================="

python3 "$(dirname "$0")/verify-couette.py" "$caseDir"
