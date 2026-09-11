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
# Weak-scaling benchmark: run a case with 1..N subdomains and report the
# wall-clock time and the speed-up. A scotch decomposition is written for
# each subdomain count (any existing decomposeParDict is restored at the
# end).
#
# Usage: perf-scaling.sh <caseDir> <nprocs> [<nprocs> ...]
#
# Must be invoked inside the OpenFOAM etc/bashrc environment.
#******************************************************************************

set -euo pipefail

caseDir=$(cd "$1" && pwd)
shift
[ $# -ge 1 ] || { echo "usage: perf-scaling.sh <caseDir> <nprocs...>" >&2; exit 2; }

cd "$caseDir"

: "${WM_PROJECT_DIR:?perf-scaling.sh must be invoked inside the OpenFOAM environment}"

. "$WM_PROJECT_DIR/bin/tools/RunFunctions"

# Preserve an existing decomposition dictionary
restoreDict=0
if [ -f system/decomposeParDict ]; then
    cp system/decomposeParDict system/decomposeParDict.perf-bak
    restoreDict=1
fi

cleanup() {
    if [ "$restoreDict" -eq 1 ]; then
        mv system/decomposeParDict.perf-bak system/decomposeParDict
    else
        rm -f system/decomposeParDict
    fi
}
trap cleanup EXIT

base=""
for np in "$@"
do
    rm -rf processor* postProcessing constant/polyMesh log.* 0.[0-9]* [1-9]*

    blockMesh > log.blockMesh 2>&1

    if [ "$np" -gt 1 ]; then
        cat > system/decomposeParDict <<DICT
FoamFile
{
    format      ascii;
    class       dictionary;
    object      decomposeParDict;
}
numberOfSubdomains $np;
method scotch;
DICT
        decomposePar -force > log.decomposePar 2>&1
        start=$(date +%s.%N)
        mpirun --allow-run-as-root -np "$np" foamRun -parallel \
            > log.foamRun 2>&1
        end=$(date +%s.%N)
    else
        start=$(date +%s.%N)
        foamRun > log.foamRun 2>&1
        end=$(date +%s.%N)
    fi

    wall=$(echo "$end $start" | awk '{printf "%.1f", $1 - $2}')
    execTime=$(grep "ExecutionTime" log.foamRun | tail -1 | \
        sed -n 's/.*ExecutionTime = \([0-9.eE+-]*\) s.*/\1/p')

    if [ -z "$base" ]; then
        base="$wall"
    fi

    speedup=$(echo "$base $wall" | awk '{printf "%.2f", $1/$2}')

    echo "np=$np wall=${wall}s executionTime=${execTime}s speedup=${speedup}"
done
