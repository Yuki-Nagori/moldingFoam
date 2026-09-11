#!/usr/bin/env python3
#******************************************************************************
# verify-runner-temperature.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the runner-coupled melt temperature: the
# gate temperature follows the 1D runner energy balance. With a feed wall
# at 500 K, htc 2000 W/m^2/K, D = 4 mm, L = 50 mm and rho Q cp =
# 800*1e-7*2400, the analytic gate temperature is
#   Twall + (Tin - Twall) exp(-htc pi D L/(mdot cp))
#
# Usage: verify-runner-temperature.py <caseDir>
#******************************************************************************

import math
import os
import re
import sys

TIN = 480.0
TWALL = 500.0
HTC = 2000.0
D = 0.004
L = 0.05
MDOT_CP = 800.0*1e-07*2400.0

TANA = TWALL + (TIN - TWALL)*math.exp(-HTC*math.pi*D*L/MDOT_CP)


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    times = [d for d in os.listdir(case_dir)
             if re.match(r"^[0-9.]+$", d)
             and os.path.isfile(os.path.join(case_dir, d, "T"))]
    if not times:
        print("FAIL: no written T field")
        sys.exit(1)

    t_last = max(times, key=float)
    with open(os.path.join(case_dir, t_last, "T")) as handle:
        text = handle.read()

    m = re.search(r"\binlet\b\s*\{(.*?)\n    \}", text, re.S)
    if not m:
        print("FAIL: no inlet patch in the T field")
        sys.exit(1)

    body = m.group(1)

    m = re.search(r"\bvalue\s+nonuniform\s+List<scalar>\s*\n?\s*(\d+)"
                  r"\s*\((.*?)\)", body, re.S)
    if m:
        vals = [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]
        Tinlet = sum(vals)/len(vals)
    else:
        m = re.search(r"\bvalue\s+uniform\s+([-+0-9.eE]+)", body)
        Tinlet = float(m.group(1))

    err = abs(Tinlet - TANA)/TANA

    print("  inlet melt temperature = {:.4f} K "
          "(analytic {:.4f} K, error {:.4%})".format(Tinlet, TANA, err))

    if err > 0.01:
        print("FAIL: the gate temperature does not follow the runner "
              "energy balance")
        sys.exit(1)

    print("PASS: the runner-coupled inlet sets the analytic gate "
          "temperature")


if __name__ == "__main__":
    main()
