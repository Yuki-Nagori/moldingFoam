#!/usr/bin/env python3
#******************************************************************************
# verify-mold-cycles.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Verify the multi-cycle mould thermal state: the lumped mould temperature
# must be carried across the cycle resets (no reset to its initial value)
# and must warm up from cycle to cycle as the casting heat accumulates.
#
# Usage: verify-mold-cycles.py <caseDir>   (exits non-zero on failure)
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

    if not wall or len(resets) < 2:
        print("FAIL: missing mould temperature series or cycle resets")
        sys.exit(1)

    times = sorted(wall)

    def at(t):
        for tt in times:
            if tt >= t - 1e-12:
                return wall[tt]
        return wall[times[-1]]

    starts = [wall[times[0]]] + [at(t) for t in resets]
    print("  mould wall temperature at cycle starts: {}".format(
        ["{:.6g}".format(v) for v in starts]))

    # The state must be carried across each reset (no jump to the initial
    # value); allow the normal per-step evolution
    for t in resets:
        before = max((tt for tt in times if tt <= t), default=times[0])
        if abs(wall[before] - at(t)) > 1.0e-3:
            print("FAIL: mould temperature jumped across the reset at "
                  "t = {}".format(t))
            sys.exit(1)

    # Warm-up: each cycle starts no cooler than the previous one, and the
    # mould ends warmer than it started
    for i in range(1, len(starts)):
        if starts[i] < starts[i - 1] - 1e-9:
            print("FAIL: mould cooled down from cycle {} to {}".format(
                i, i + 1))
            sys.exit(1)

    if starts[-1] <= starts[0] + 1e-6:
        print("FAIL: mould temperature did not increase across cycles")
        sys.exit(1)

    print("PASS: mould state retained and warming across {} cycles".format(
        len(starts)))
    sys.exit(0)


if __name__ == "__main__":
    main()
