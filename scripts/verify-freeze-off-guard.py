#!/usr/bin/env python3
#******************************************************************************
# verify-freeze-off-guard.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the melt freeze-off (short-shot) guard
# (task 038 T-follow-up): the sample material freezes almost immediately
# in the cold mould. Without the guard the imposed flow rate forces
# frozen material through closing channels and the temperature equation
# diverges; with it the solver must detect the short shot, seal the gate
# and finish the run with a stable, early-stopped fill.
#
# Usage: verify-freeze-off-guard.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys


def latest_time(case_dir):
    times = sorted(
        (d for d in os.listdir(case_dir)
         if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0),
        key=float,
    )
    return times[-1] if times else None


def mean_alpha(case_dir, time):
    with open(os.path.join(case_dir, time, "alpha.melt"),
              errors="replace") as f:
        txt = f.read()

    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n\s*\d+\s*\n"
        r"\((.*?)\)\s*;",
        txt,
        re.S,
    )
    if m is None:
        return None

    vals = [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(1))]
    return sum(vals)/len(vals)


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    fail = False

    if re.search(r"\bnan\b", log, re.I):
        print("FAIL: the log contains NaN values")
        fail = True

    m = re.search(
        r"melt freeze-off detected:.*filled fraction ([-+0-9.eE]+)", log
    )
    if m is None:
        print("FAIL: the freeze-off guard never fired")
        fail = True
    else:
        fillAtFreezeOff = float(m.group(1))
        print("  freeze-off detected at filled fraction = {:.4f}"
              .format(fillAtFreezeOff))
        if not 0.0 < fillAtFreezeOff < 0.50:
            print("FAIL: the freeze-off was not detected early")
            fail = True

    if "gate sealed" not in log and "sealing the gate" not in log:
        print("FAIL: the gate was not sealed on freeze-off")
        fail = True

    if "V/P switch" not in log:
        print("FAIL: the solver did not switch to pressure control")
        fail = True

    time = latest_time(case_dir)
    a = mean_alpha(case_dir, time) if time else None
    if a is None:
        print("FAIL: cannot read the final fill fraction")
        fail = True
    else:
        print("  final fill fraction (short shot) = {:.4f}".format(a))
        if a > 0.50:
            print("FAIL: the frozen part kept filling")
            fail = True

    if fail:
        sys.exit(1)

    print("PASS: the freeze-off guard stops the short shot and keeps the "
          "run stable")


if __name__ == "__main__":
    main()
