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

scriptDir=$(cd "$(dirname "$0")" && pwd)
root="${1:-tests/cases}"
root=$(cd "$root" && pwd)

: "${WM_PROJECT_DIR:?run-solver-tests.sh must be invoked inside the OpenFOAM environment}"

. "$WM_PROJECT_DIR/bin/tools/RunFunctions"

# Allow more MPI ranks than detected cores (same reason as run-case.sh: CI
# runners and small VMs expose fewer slots than the case asks for)
export OMPI_MCA_rmaps_base_oversubscribe=1

nFailed=0
nCases=0

# The assertions applied to a run log: non-zero exit, glibc heap
# corruption, duplicated runtime selections and the expected patterns
# (system/expectedPatterns) must all hold for the serial run and for the
# optional parallel run of a case
checkLog()
{
    local name="$1"
    local stage="$2"

    if grep -qiE "malloc_consolidate|corrupted (fastbin|size)|free\(\): invalid" \
            log.foamRun
    then
        echo "FAIL: $name ($stage): heap corruption reported at exit"
        return 1
    fi

    if grep -qi "Duplicate entry" log.foamRun
    then
        echo "FAIL: $name ($stage): duplicate runtime-selection entries " \
             "(multiple module copies loaded)"
        return 1
    fi

    local pattern
    while IFS= read -r pattern
    do
        case "$pattern" in
            ''|'#'*) continue ;;
        esac
        if grep -Eq -- "$pattern" log.foamRun
        then
            echo "PASS: $name: $pattern"
        else
            echo "FAIL: $name ($stage): missing pattern: $pattern"
            return 1
        fi
    done < system/expectedPatterns

    if [ -f system/verifyScript ]
    then
        local verifier
        verifier=$(sed -n 's/^[[:space:]]*\([^[:space:]]*\)[[:space:]]*$/\1/p' \
            system/verifyScript | head -1)

        if [ -n "$verifier" ] \
           && ! python3 "$scriptDir/$verifier" "$caseDir"
        then
            echo "FAIL: $name ($stage): $verifier"
            return 1
        fi
    fi

    return 0
}

for caseDir in "$root"/*/
do
    caseDir="${caseDir%/}"
    [ -f "$caseDir/system/expectedPatterns" ] || continue
    nCases=$((nCases + 1))
    name=$(basename "$caseDir")

    echo "== solver case: $name =="
    cd "$caseDir"

    rm -rf postProcessing processor* constant/polyMesh log.* 0.[0-9]* [1-9]*
    blockMesh > log.blockMesh 2>&1

    # A case may invert the expectation: system/expectFailure names a
    # message that must appear when foamRun fails (used for the rejected
    # dictionary entries, task 044)
    if [ -f system/expectFailure ]
    then
        caseFailed=0
        expected=$(sed -n '1p' system/expectFailure)
        if foamRun > log.foamRun 2>&1
        then
            echo "FAIL: $name: foamRun succeeded but a fatal error was ""expected"
            caseFailed=1
        elif ! grep -qF -- "$expected" log.foamRun
        then
            echo "FAIL: $name: expected error text not found: $expected"
            caseFailed=1
        else
            echo "PASS: $name: expected failure reproduced"
        fi

        if [ "$caseFailed" -ne 0 ]
        then
            nFailed=$((nFailed + 1))
            tail -20 log.foamRun || true
        fi
        continue
    fi


    caseFailed=0

    if ! foamRun > log.foamRun 2>&1; then
        echo "FAIL: $name: foamRun exited non-zero (see log.foamRun)"
        caseFailed=1
    fi

    if ! checkLog "$name" "serial"; then
        caseFailed=1
    fi

    # Optional parallel pass: system/nProcs holds the number of subdomains
    # and system/decomposeParDict the decomposition. The case is then run
    # again under mpirun, which is what catches rank-dependent collective
    # counts: a reduction inside a loop over boundary patches runs a
    # different number of times on each rank whenever the decomposition
    # leaves them with different neighbour counts, and the run deadlocks
    # (issue #7). The timeout turns a deadlock into a failure of this
    # suite instead of a hanging job
    if [ -f system/nProcs ]
    then
        nProcs=$(head -1 system/nProcs)
        timeoutS="${MOLDINGFOAM_PARALLEL_TIMEOUT:-300}"

        rm -rf processor* log.decomposePar
        foamDictionary -entry numberOfSubdomains -set "$nProcs" \
            system/decomposeParDict > /dev/null

        if ! decomposePar -force > log.decomposePar 2>&1; then
            echo "FAIL: $name (parallel $nProcs): decomposePar failed"
            caseFailed=1
        elif ! timeout "$timeoutS" mpirun -np "$nProcs" foamRun -parallel \
                > log.foamRun 2>&1
        then
            echo "FAIL: $name (parallel $nProcs): mpirun failed or timed " \
                 "out after ${timeoutS}s (a timeout points at a parallel " \
                 "deadlock, e.g. a rank-dependent collective count)"
            caseFailed=1
        elif ! checkLog "$name" "parallel $nProcs"; then
            caseFailed=1
        fi
    fi

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
