#!/usr/bin/env python3
#******************************************************************************
# verify-mold-steady.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Verify that the lumped mould temperature approaches a periodic steady
# state over many moulding cycles: the per-cycle temperature increments
# must be positive and decay with the cycle number.
#
# Usage: verify-mold-steady.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import glob
import os
import re
import sys


def series(case_dir, name):
    data = {}
    for path in sorted(glob.glob(
            os.path.join(case_dir, "postProcessing", name, "*", "*.dat"))):
        with open(path) as handle:
            for line in handle:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()
                try:
                    data[float(parts[0])] = float(parts[-1])
                except ValueError:
                    continue
    return data


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as h:
        log = h.read()

    resets = [
        float(m.group(1))
        for m in re.finditer(
            r"cycle \d+ complete; starting cycle \d+/\d+ at t = "
            r"([-+0-9.eE]+) s", log)
    ]
    wall = series(case_dir, "moldTemperature")

    if not wall or len(resets) < 10:
        print("FAIL: missing mould temperature series or too few cycles")
        sys.exit(1)

    times = sorted(wall)

    def at(t):
        for tt in times:
            if tt >= t - 1e-12:
                return wall[tt]
        return wall[times[-1]]

    starts = [wall[times[0]]] + [at(t) for t in resets]
    incr = [starts[i + 1] - starts[i] for i in range(len(starts) - 1)]

    print("  cycles          = {}".format(len(starts) - 1))
    print("  T_mould start   = {:.6f} K".format(starts[0]))
    print("  T_mould end     = {:.6f} K".format(starts[-1]))
    print("  first increment = {:.3e} K".format(incr[0]))
    print("  last increment  = {:.3e} K".format(incr[-1]))

    if min(incr) < -1e-9:
        print("FAIL: mould temperature decreased in some cycle")
        sys.exit(1)

    # Convergence: the increments decay and the last few are small
    if not (incr[-1] < 0.5*incr[0]):
        print("FAIL: per-cycle increments have not decayed")
        sys.exit(1)

    if not (incr[-1] < 0.05*(starts[-1] - starts[0] + 1e-12) + 1e-9):
        print("FAIL: last increment is not small compared to the warm-up")
        sys.exit(1)

    print("PASS: mould temperature converges to a periodic steady state")
    sys.exit(0)


if __name__ == "__main__":
    main()
