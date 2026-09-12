#!/usr/bin/env bash
#******************************************************************************
# smoke-exit.sh
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Exit-path smoke test (task 037): a solver run that completes must exit
# with status 0 and leave no glibc heap-corruption report, and a FOAM
# FATAL path must exit non-zero without corrupting the heap.
#
# Usage: smoke-exit.sh <caseDir>   (run inside the OpenFOAM environment)
#******************************************************************************
set -euo pipefail

caseDir=$(cd "$1" && pwd)
workDir=$(mktemp -d)
trap 'rm -rf "$workDir"' EXIT

cp -r "$caseDir"/. "$workDir"/
cd "$workDir"

rm -rf postProcessing log.* processor* constant/polyMesh 0.[0-9]* [1-9]*
blockMesh > log.blockMesh 2>&1

fail=0

# Success path: zero steps, must exit 0 with no heap report
sed -i "s/^endTime.*/endTime         0;/" system/controlDict
set +e
MALLOC_CHECK_=3 foamRun > log.zero 2>&1
rc=$?
set -e
echo "zero-step exit = $rc"

if [ "$rc" -ne 0 ]; then
    echo "FAIL: the zero-step run exited with $rc" >&2
    fail=1
fi
if ! grep -q "^End$" log.zero; then
    echo "FAIL: the zero-step run did not print the End marker" >&2
    fail=1
fi
if grep -qiE "malloc_consolidate|corrupted (fastbin|size)|free\\(\\): invalid" \
        log.zero; then
    echo "FAIL: heap corruption reported at exit" >&2
    fail=1
fi
if grep -qi "Duplicate entry" log.zero; then
    echo "FAIL: duplicate runtime-selection entries (multiple copies of " \
         "the module loaded)" >&2
    fail=1
fi

# FATAL path: an unknown solver must exit non-zero without heap corruption
sed -i "s/^solver .*/solver          noSuchSolver;/" system/controlDict
set +e
MALLOC_CHECK_=3 foamRun > log.fatal 2>&1
rc=$?
set -e
echo "fatal-path exit = $rc"

if [ "$rc" -eq 0 ]; then
    echo "FAIL: the fatal path exited 0" >&2
    fail=1
fi
if grep -qiE "malloc_consolidate|corrupted (fastbin|size)|free\\(\\): invalid" \
        log.fatal; then
    echo "FAIL: heap corruption reported on the fatal path" >&2
    fail=1
fi

if [ "$fail" -ne 0 ]; then
    exit 1
fi

echo "PASS: the success and fatal exit paths are clean"
