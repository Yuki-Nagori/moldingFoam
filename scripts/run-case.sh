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
# Run the moldingFoam contract case (mesh, optional decomposition, solve,
# reconstruction and acceptance verification).
#
# Usage: run-case.sh <caseDir> [nSubdomains]
#
# Must be invoked inside the OpenFOAM etc/bashrc environment.
#******************************************************************************

set -euo pipefail

nprocs="${2:-1}"

# Resolve to an absolute path before changing directory
caseDir=$(cd "$1" && pwd)
cd "$caseDir"

: "${WM_PROJECT_DIR:?run-case.sh must be invoked inside the OpenFOAM environment}"

. "$WM_PROJECT_DIR/bin/tools/RunFunctions"

# Clean a previous run
rm -rf processor* postProcessing constant/polyMesh log.* 0.[0-9]* [1-9]*

blockMesh > log.blockMesh 2>&1

if [ "$nprocs" -gt 1 ]; then
    subdomains=$(sed -n 's/^[[:space:]]*numberOfSubdomains[[:space:]]*\([0-9][0-9]*\).*$/\1/p' \
        system/decomposeParDict | head -1)
    if [ -n "$subdomains" ] && [ "$subdomains" != "$nprocs" ]; then
        echo "error: $nprocs subdomains requested but" \
             "system/decomposeParDict sets numberOfSubdomains $subdomains" >&2
        exit 1
    fi
    decomposePar -force > log.decomposePar 2>&1
    runParallel -n "$nprocs" foamRun || {
        tail -30 log.foamRun
        exit 1
    }
    reconstructPar -latestTime > log.reconstructPar 2>&1
else
    foamRun 2>&1 | tee log.foamRun
fi

echo "=============================================================="
echo "moldingFoam contract case finished, verifying acceptance ..."
echo "=============================================================="

python3 "$(dirname "$0")/verify-case.py" "$caseDir"
