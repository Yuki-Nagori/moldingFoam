#!/usr/bin/env python3
#******************************************************************************
# perf-breakdown.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Per-step breakdown of a moldingFoam run from its solver log (task 041,
# step A). Reports where the linear-solver work goes: the number of time
# steps, the solve count and total iterations per solver/field pair, the
# MULES (interface) solves, and the execution time. A true per-term and
# communication breakdown needs the upstream profiling instrumentation
# (OPENFOAM_PROFILE-style regions inside the modules), which this tool
# deliberately does not require.
#
# Usage: perf-breakdown.py <log.foamRun>
#******************************************************************************

import re
import sys
from collections import OrderedDict


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "log.foamRun"

    with open(path, errors="replace") as f:
        log = f.read()

    steps = len(re.findall(r"^Time = ", log, re.M))

    execTimes = [float(x) for x in re.findall(
        r"ExecutionTime = ([-+0-9.eE]+) s", log)]
    total = execTimes[-1] if execTimes else 0.0

    solves = OrderedDict()
    # Two-region runs prefix the line with the region ("cavity DICPCG: ..."),
    # single-region runs do not
    pattern = re.compile(
        r"^\s*(?:(\S+)\s+)?(\S+):\s+Solving for (\S+),\s+"
        r"Initial residual = ([-+0-9.eE]+),\s+"
        r"Final residual = ([-+0-9.eE]+),\s+No Iterations (\d+)",
        re.M)

    for m in pattern.finditer(log):
        region, solver, name, init, final, iters = m.groups()
        key = "%s (%s)" % (name, solver)
        n, tot = solves.get(key, (0, 0))
        solves[key] = (n + 1, tot + int(iters))

    mules = len(re.findall(r"MULES: Solving for ", log))

    print("per-step breakdown: %s" % path)
    print("  time steps            : %d" % steps)
    print("  execution time        : %.1f s%s" % (
        total, " (%.3f s/step)" % (total/steps) if steps else ""))

    print("  solves (count, total iterations, iterations/step):")
    totSolves = 0
    totIters = 0
    for key, (n, iters) in solves.items():
        totSolves += n
        totIters += iters
        print("    %-38s %7d  %9d  %8.2f" % (
            key, n, iters, iters/steps if steps else 0.0))

    print("    %-38s %7d  %9d  %8.2f" % (
        "TOTAL", totSolves, totIters, totIters/steps if steps else 0.0))
    print("  MULES (interface) solves: %d (%.2f/step)"
          % (mules, mules/steps if steps else 0.0))
    print("  note: the per-term/communication split needs the upstream "
          "profiling regions (see ai-docs/tasks/041)")


if __name__ == "__main__":
    main()
