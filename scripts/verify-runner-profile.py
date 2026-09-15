#!/usr/bin/env python3
#******************************************************************************
# verify-runner-profile.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the multi-stage process profile driving a
# runner network (tasks 030 x 058): the inlet prescribes a two-stage total
# flow rate (2e-07 -> 6e-07 m^3/s at t = 0.4 s) and the runner splits it over
# two gates. Both stages must appear at the boundaries:
#
#   - the split of the two gates stays at the analytic resistance ratio;
#   - the two gate flows sum to the profile value of their stage, i.e. the
#     profile, not the scalar totalFlowRate (absent here), sets the total.
#
# The function objects write every time step, so the samples are read with
# the "Time = ..." lines that precede them.
#
# Usage: verify-runner-profile.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

RHO = 746.3            # melt density at 480 K, 1e5 Pa [kg/m^3]
N = 0.5                # power-law index
RATIO = (0.004/0.002)**(3 + 1/N)   # analytic split Q1/Q2 = 32
SWITCH = 0.4           # profile stage change [s]
Q_EARLY = 2e-07        # stage 1 total [m^3/s]
Q_LATE = 6e-07         # stage 2 total [m^3/s]


def timedSamples(log, patch):
    """(time, value/RHO) pairs for one inlet, using the Time = header"""
    out = []
    t = -1.0
    timeRe = re.compile(r"^Time = ([-+0-9.eE]+)")
    sampRe = re.compile(
        r"sum\(%s\) of alphaRhoPhi\.melt = ([-+0-9.eE]+)" % patch)

    for line in log.splitlines():
        m = timeRe.match(line)
        if m:
            t = float(m.group(1))
            continue

        m = sampRe.search(line)
        if m and t > 0:
            out.append((t, abs(float(m.group(1)))/RHO))

    return out


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    s1 = timedSamples(log, "inlet1")
    s2 = timedSamples(log, "inlet2")

    if not s1 or not s2:
        print("FAIL: no mass flux samples for one of the gates")
        sys.exit(1)

    def mean(series, lo, hi):
        v = [q for (t, q) in series if lo < t < hi]
        return sum(v)/len(v) if v else 0.0

    def ratio(lo, hi):
        return mean(s1, lo, hi)/mean(s2, lo, hi)

    rEarly = ratio(0.0, SWITCH)
    rLate = ratio(SWITCH + 0.05, 10.0)
    qEarly = mean(s1, 0.0, SWITCH) + mean(s2, 0.0, SWITCH)
    qLate = mean(s1, SWITCH + 0.05, 10.0) + mean(s2, SWITCH + 0.05, 10.0)

    print("  split ratio: early {:.2f}, late {:.2f} (analytic {:.1f})".format(
        rEarly, rLate, RATIO))
    print("  total flow: early {:.4e}, late {:.4e} m^3/s (profile {:.1e} -> "
          "{:.1e})".format(qEarly, qLate, Q_EARLY, Q_LATE))

    fail = False
    if abs(rEarly - RATIO)/RATIO > 0.05 or abs(rLate - RATIO)/RATIO > 0.05:
        print("FAIL: the gate split does not match the analytic ratio")
        fail = True
    if abs(qEarly - Q_EARLY)/Q_EARLY > 0.05:
        print("FAIL: the first stage does not follow the profile total")
        fail = True
    if abs(qLate - Q_LATE)/Q_LATE > 0.05:
        print("FAIL: the second stage does not follow the profile total")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the process profile sets the runner total and the network "
          "splits it")


if __name__ == "__main__":
    main()
