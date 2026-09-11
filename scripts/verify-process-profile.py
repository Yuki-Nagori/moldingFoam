#!/usr/bin/env python3
#******************************************************************************
# verify-process-profile.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the multi-stage injection profile: the gate
# flow rate follows a table profile (1e-7 then 3e-7 m^3/s) and the V/P
# switch is triggered by the time criterion at 0.4 s. The inlet mass flux
# must match rho*Q in both stages.
#
# Usage: verify-process-profile.py <caseDir>  (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

RHO = 746.3            # melt density at 480 K, 1e5 Pa [kg/m^3]
Q1 = 1e-07
Q2 = 3e-07


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        lines = f.readlines()

    if not any(line.strip() == "End" for line in lines):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    # Associate each inlet mass flow sample with the current time
    samples = []
    t = 0.0
    for line in lines:
        m = re.match(r"\s*Time = ([-+0-9.eE]+)s", line)
        if m:
            t = float(m.group(1))
            continue
        m = re.search(r"sum\(inlet\) of alphaRhoPhi\.melt = ([-+0-9.eE]+)",
                      line)
        if m:
            samples.append((t, float(m.group(1))))

    def mean_flux(t0, t1):
        vals = [v for (tt, v) in samples if t0 <= tt <= t1 and abs(v) > 0]
        return sum(vals)/len(vals) if vals else None

    f1 = mean_flux(0.05, 0.29)
    f2 = mean_flux(0.31, 0.39)

    if f1 is None or f2 is None:
        print("FAIL: no mass flux samples in one of the stages")
        sys.exit(1)

    e1 = abs(abs(f1) - RHO*Q1)/(RHO*Q1)
    e2 = abs(abs(f2) - RHO*Q2)/(RHO*Q2)

    print("  stage 1 flux = {:.6e} kg/s (rho Q = {:.6e}, error {:.3%})".format(
        f1, -RHO*Q1, e1))
    print("  stage 2 flux = {:.6e} kg/s (rho Q = {:.6e}, error {:.3%})".format(
        f2, -RHO*Q2, e2))

    # Time-based V/P switch at 0.4 s
    sw = re.findall(r"V/P switch:.*at t = ([-+0-9.eE]+) s", "\n".join(lines))
    if not sw:
        print("FAIL: no V/P switch in the log")
        sys.exit(1)
    tSwitch = float(sw[-1])
    esw = abs(tSwitch - 0.4)/0.4

    print("  V/P switch at t = {:g} s (error {:.3%})".format(tSwitch, esw))

    fail = False
    if e1 > 0.05 or e2 > 0.05:
        print("FAIL: the inlet mass flux does not follow the flow profile")
        fail = True
    if esw > 0.02:
        print("FAIL: the time-based V/P switch did not fire at 0.4 s")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the multi-stage flow profile and time-based V/P switch "
          "are honoured")


if __name__ == "__main__":
    main()
