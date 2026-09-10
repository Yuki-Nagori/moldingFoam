#!/usr/bin/env python3
#******************************************************************************
# verify-coolant-channel.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the 1D coolant channel boundary: the case is
# isothermal at 300 K and the coldWall moldingMoldTemperature lump is
# cooled by a channel whose inlet is at 350 K, so the mould temperature
# must rise from 300 K towards the coolant. The mould temperature history
# is logged by the surfaceFieldValue function object in log.foamRun.
#
# Usage: verify-coolant-channel.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    log_path = os.path.join(case_dir, "log.foamRun")
    with open(log_path) as handle:
        text = handle.read()

    if "\nEnd\n" not in text and "End\n" not in text:
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    values = [
        float(x) for x in re.findall(
            r"areaAverage\(coldWall\)[^=\n]*=\s*([-+0-9.eE]+)", text)
    ]
    if not values:
        print("FAIL: no coldWall temperature history in the log")
        sys.exit(1)

    T0 = values[0]
    T1 = values[-1]

    print("  coldWall T: start = {:.3f} K, end = {:.3f} K".format(T0, T1))

    fail = False
    if not (T0 > 299.0 and T0 < 301.0):
        print("FAIL: the mould did not start at its initial temperature")
        fail = True
    if T1 < T0 + 5.0:
        print("FAIL: the 1D coolant channel did not heat the mould")
        fail = True
    if T1 > 355.0:
        print("FAIL: the mould exceeded the coolant inlet temperature")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the 1D coolant channel drives the mould towards the "
          "coolant temperature")


if __name__ == "__main__":
    main()
