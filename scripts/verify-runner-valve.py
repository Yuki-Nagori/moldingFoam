#!/usr/bin/env python3
#******************************************************************************
# verify-runner-valve.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the per-gate valve timing (task 058): gate2 of
# a two-gate runner network closes at t = 0.6 s of a 2 s run, so
#
#   - while open, gate2 carries the analytic share of the flat split
#     (QTOT/(1 + 32));
#   - its flow must then stop once and stay stopped: the flowing samples
#     form a contiguous prefix of the log, with nothing at the end;
#   - gate1, whose network is the same one, must take the whole flow after
#     the closing time (it is the only branch still open).
#
# The function objects write every time step, so the check works on the
# sample series rather than on a single sample.
#
# Usage: verify-runner-valve.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

RHO = 746.3            # melt density at 480 K, 1e5 Pa [kg/m^3]
QTOT = 2e-07           # runner flow rate prescribed at each inlet [m^3/s]
N = 0.5                # power-law index
RATIO = (0.004/0.002)**(3 + 1/N)   # open-gate split Q1/Q2 = 32
Q2OPEN = QTOT/(1 + RATIO)          # gate2 share while its valve is open
FLOWING = 0.01*QTOT                # a sample above 1% of QTOT carries flow


def samples(log, patch):
    return [float(x) for x in re.findall(
        r"sum\(%s\) of alphaRhoPhi\.melt = ([-+0-9.eE]+)" % patch, log)]


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    s1 = samples(log, "inlet1")
    s2 = samples(log, "inlet2")

    if not s1 or not s2:
        print("FAIL: no mass flux samples for one of the gates")
        sys.exit(1)

    flow = [i for i, x in enumerate(s2) if abs(x)/RHO > FLOWING]
    q2 = [abs(s2[i])/RHO for i in flow]

    print("  gate2: {} of {} samples carry flow, mean {:.4e} m^3/s "
          "(analytic open share {:.4e})".format(
              len(flow), len(s2), (sum(q2)/len(q2)) if q2 else 0.0, Q2OPEN))
    print("  gate2 last sample = {:.2e} m^3/s, gate1 last sample = {:.4e} "
          "m^3/s".format(abs(s2[-1])/RHO, abs(s1[-1])/RHO))

    fail = False

    if not flow:
        print("FAIL: gate2 never injected")
        fail = True
    elif flow != list(range(flow[0], flow[-1] + 1)):
        print("FAIL: the gate2 flow is not one contiguous open interval")
        fail = True
    elif flow[-1] == len(s2) - 1:
        print("FAIL: gate2 never stops")
        fail = True
    elif abs(sum(q2)/len(q2) - Q2OPEN)/Q2OPEN > 0.05:
        print("FAIL: the open-gate share does not match the analytic split")
        fail = True

    # Once its sibling is shut gate1 is the only open branch of the network,
    # so it takes the whole prescribed flow
    if abs(abs(s1[-1])/RHO - QTOT)/QTOT > 0.05:
        print("FAIL: gate1 does not take the whole flow after the closing "
              "time")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the valve closes gate2 only, and the open branch takes over")


if __name__ == "__main__":
    main()
