#!/usr/bin/env python3
#******************************************************************************
# verify-fill-time.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the fill-time field: a single gate fills a
# 40 x 4 x 2 mm channel at a constant volumetric flow rate, so the
# centreline fill time must follow t = x/u with u = Q/(h w). The wall
# cells fill later (parabolic front), which is why the centreline is used.
#
# Usage: verify-fill-time.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

Q = 1e-07              # inlet flow rate [m^3/s]
H = 0.004              # channel height [m]
W = 0.002              # channel thickness [m]
U = Q/(H*W)            # mean front speed [m/s]
DX = 0.0005


def cell_x(index):
    if index < 304:
        i = index % 38
    elif index < 336:
        i = 38 + (index - 304) % 4
    else:
        i = 42 + (index - 336) % 38
    return (i + 0.5)*DX


def centreline_indices():
    # j = 4 in each block (y = 0.002)
    idx = [i + 38*4 for i in range(38)]
    idx += [304 + i + 4*4 for i in range(4)]
    idx += [336 + i + 38*4 for i in range(38)]
    return idx


def read_fill_time(case_dir, time_name):
    path = os.path.join(case_dir, time_name, "fillTime")
    with open(path) as handle:
        text = handle.read()
    m = re.search(r"internalField\s+nonuniform\s+List<scalar>\s*\n?\s*"
                  r"(\d+)\s*\((.*?)\)", text, re.S)
    if not m:
        return None
    return [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    times = [d for d in os.listdir(case_dir)
             if re.match(r"^[0-9.]+$", d)
             and os.path.isfile(os.path.join(case_dir, d, "fillTime"))]
    if not times:
        print("FAIL: no written fillTime field")
        sys.exit(1)

    t_last = max(times, key=float)
    ft = read_fill_time(case_dir, t_last)
    if ft is None or len(ft) != 640:
        print("FAIL: cannot read the fillTime field")
        sys.exit(1)

    maxErr = 0.0
    for idx in centreline_indices():
        x = cell_x(idx)
        tAna = x/U
        if ft[idx] > 1e6:
            continue
        err = abs(ft[idx] - tAna)/tAna
        maxErr = max(maxErr, err)

    print("  centreline fill time vs x/u: max relative error = {:.3%}".format(
        maxErr))

    if maxErr > 0.05:
        print("FAIL: the fill time does not follow x/u")
        sys.exit(1)

    print("PASS: the fill-time field follows the analytic centreline "
          "solution")


if __name__ == "__main__":
    main()
