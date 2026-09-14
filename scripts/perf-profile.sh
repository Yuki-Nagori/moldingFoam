#!/usr/bin/env bash
# 050: wall-clock split of a run (communication wait vs compute).
#
# Runs a case under mpirun, samples one rank with perf for a window, and
# reports the sample share of the MPI stack (communication / blocking
# waits) against the OpenFOAM compute libraries. The MPI share is a lower
# bound on the parallel overhead: it counts only the time a rank spends
# inside the MPI library (including its busy-wait progress loop).
#
# Usage (inside the OpenFOAM environment):
#   bash scripts/perf-profile.sh <caseDir> <nprocs> [warmup] [window]
#
# The case is run in place (it is cleaned first); use a scratch copy.

set -uo pipefail

scriptDir=$(cd "$(dirname "$0")" && pwd)
caseDir=$(cd "$1" && pwd)
nprocs="$2"
warmup="${3:-20}"
window="${4:-20}"

cd "$caseDir"

: "${WM_PROJECT_DIR:?perf-profile.sh must be invoked inside the OpenFOAM environment}"

export OMPI_MCA_rmaps_base_oversubscribe=1

rm -rf processor* postProcessing constant/polyMesh log.* 0.[0-9]* [1-9]* \
    perf.data perf.dso.txt perf.symbol.txt perf.split.txt

blockMesh > log.blockMesh 2>&1 || { echo "blockMesh failed"; exit 1; }
decomposePar -force > log.decomposePar 2>&1 || { echo "decomposePar failed"; exit 1; }

mpirun -np "$nprocs" foamRun -parallel > log.foamRun 2>&1 &
mpirunPid=$!

sleep "$warmup"
# -x matches the process name: a -f match would also hit mpirun, whose
# command line contains the foamRun arguments
rank0=$(pgrep -x foamRun | sort -n | head -1)
if [ -z "$rank0" ]
then
    echo "run finished before the sampling window; increase endTime or lower warmup"
    wait "$mpirunPid"
    exit 1
fi

paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 4)
if [ "$paranoid" -gt 1 ]
then
    echo "perf needs kernel.perf_event_paranoid <= 1; run once:" >&2
    echo "  sudo sysctl -w kernel.perf_event_paranoid=1" >&2
    wait "$mpirunPid"
    exit 1
fi

echo "=== sampling rank $rank0 for ${window}s ($(nproc) cores, np=$nprocs) ==="
perf record -F 499 -g -o perf.data -p "$rank0" -- sleep "$window" \
    > perf.record.txt 2>&1
perf report --stdio -i perf.data --no-children --sort dso \
    > perf.dso.txt 2>/dev/null
perf report --stdio -i perf.data --no-children --sort symbol \
    2>/dev/null | head -30 > perf.symbol.txt

wait "$mpirunPid"
steps=$(grep -c "^Time = " log.foamRun)
exec_time=$(grep "ExecutionTime" log.foamRun | tail -1)

python3 "$scriptDir/perf-split.py" perf.dso.txt | tee perf.split.txt

echo "=== steps $steps, $exec_time ==="
