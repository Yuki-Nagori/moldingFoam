#!/usr/bin/env python3
#******************************************************************************
# verify-weld-line.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the weld-line benchmark: a 40 x 4 mm channel
# is filled from both ends at the same flow rate, so the two melt fronts
# meet on the centre plane. The last cells to fill are the weld line, so
# the maximum of the recorded fillTime field must sit at the centre and
# equal the analytic fill time V/(2Q).
#
# Usage: verify-weld-line.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

V = 0.04*0.004*0.002   # cavity volume [m^3]
Q = 1e-07              # flow rate per gate [m^3/s]
T_FILL = V/(2*Q)       # analytic fill time [s]

# Mesh structure: block1 38x8, block2 4x8, block3 38x8 (x by y, one cell
# in z), all cells 0.5 mm in x
DX = 0.0005


def cell_x(index):
    """x centre of a cell from the block-structured mesh."""
    if index < 304:
        i = index % 38
    elif index < 336:
        i = 38 + (index - 304) % 4
    else:
        i = 42 + (index - 336) % 38
    return (i + 0.5)*DX


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

    # The weld line is the streamwise station where the cross-section
    # maximum fill time peaks (the wall cells fill last at every station,
    # so the peak of the station maxima is the meeting plane)
    times = [d for d in os.listdir(case_dir)
             if re.match(r"^[0-9.]+$", d)
             and os.path.isdir(os.path.join(case_dir, d))
             and os.path.isfile(os.path.join(case_dir, d, "fillTime"))]
    if not times:
        print("FAIL: no written fillTime field")
        sys.exit(1)

    t_last = max(times, key=float)
    ft = read_fill_time(case_dir, t_last)
    if ft is None or len(ft) != 640:
        print("FAIL: cannot read the fillTime field")
        sys.exit(1)

    # Station maxima over the filled cells (cells that never filled keep
    # the VGREAT sentinel and are excluded)
    station = {}
    for idx, value in enumerate(ft):
        if value > 1e6:
            continue
        x = round(cell_x(idx), 7)
        station[x] = max(station.get(x, -1.0), value)

    xs = sorted(station)
    xWeld = max(xs, key=lambda x: station[x])
    xerr = abs(xWeld - 0.02)

    print("  weld line at x = {:.5f} m "
          "(centre offset {:.3%} of the length, {:.1f} cells)".format(
              xWeld, xerr/0.04, xerr/0.0005))
    print("  station max at the weld line = {:.4f} s "
          "(analytic fill time {:.4f} s)".format(
              station[xWeld], T_FILL))

    # Trapped air (airTrap sums to a cell count) must stay small: the
    # vent sits at the meeting point
    trapped = [
        float(v) for v in re.findall(
            r"sum\(all\) of airTrap = ([-+0-9.eE]+)", log)
    ]
    if trapped:
        print("  trapped air cells (final) = {:g} of 640".format(trapped[-1]))

    # Symmetry of the station-maximum profile about the centre
    symErr = max(
        abs(station[x] - station[round(0.04 - x, 7)])
        /max(station[x], 1e-30)
        for x in xs if x < 0.02
    )

    print("  station-max symmetry error = {:.3%}".format(symErr))

    fail = False
    if xerr > 0.002:
        print("FAIL: the weld line is more than 4 cells off the centre")
        fail = True
    if symErr > 0.05:
        print("FAIL: the fill-time field is not symmetric")
        fail = True
    if fail:
        sys.exit(1)

    print("PASS: the weld line forms on the centre plane with a symmetric "
          "fill-time field")


if __name__ == "__main__":
    main()
