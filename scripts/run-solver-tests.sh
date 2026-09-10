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
# Run the fast solver feature cases under a directory tree. Each case is
# a small, short-running OpenFOAM case; after the run the patterns listed
# in <case>/system/expectedPatterns (one extended regular expression per
# line, '#' comments allowed) must all appear in log.foamRun.
#
# These cases complement the model tests (xmake run test) and the full
# contract regression (xmake run case-contract): they exercise solver
# level features (stage transitions, seals, cycle resets, diagnostics)
# in seconds instead of a full moulding cycle.
#
# Usage: run-solver-tests.sh <casesRoot>
#
# Must be invoked inside the OpenFOAM etc/bashrc environment.
#******************************************************************************

set -euo pipefail

root="${1:-tests/cases}"
root=$(cd "$root" && pwd)

: "${WM_PROJECT_DIR:?run-solver-tests.sh must be invoked inside the OpenFOAM environment}"

. "$WM_PROJECT_DIR/bin/tools/RunFunctions"

nFailed=0
nCases=0

for caseDir in "$root"/*/
do
    caseDir="${caseDir%/}"
    [ -f "$caseDir/system/expectedPatterns" ] || continue
    nCases=$((nCases + 1))
    name=$(basename "$caseDir")

    echo "== solver case: $name =="
    cd "$caseDir"

    rm -rf postProcessing constant/polyMesh log.* 0.[0-9]* [1-9]*
    blockMesh > log.blockMesh 2>&1
    foamRun > log.foamRun 2>&1 || true

    caseFailed=0
    while IFS= read -r pattern
    do
        case "$pattern" in
            ''|'#'*) continue ;;
        esac
        if grep -Eq -- "$pattern" log.foamRun
        then
            echo "PASS: $name: $pattern"
        else
            echo "FAIL: $name: missing pattern: $pattern"
            caseFailed=1
        fi
    done < system/expectedPatterns

    if [ "$caseFailed" -ne 0 ]
    then
        nFailed=$((nFailed + 1))
        tail -20 log.foamRun || true
    fi
done

if [ "$nCases" -eq 0 ]
then
    echo "FAIL: no solver cases found under $root"
    exit 1
fi

if [ "$nFailed" -ne 0 ]
then
    echo "$nFailed of $nCases solver case(s) FAILED"
    exit 1
fi

echo "All $nCases solver case(s) passed"
