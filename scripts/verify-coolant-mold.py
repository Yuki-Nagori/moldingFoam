#!/usr/bin/env python3
#******************************************************************************
# verify-coolant-mold.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the multi-region mould cooling channel
# (task 036, route C): the mould top face is a one-dimensional
# plug-flow channel (moldingChannelCooling); the coolant must pick up
# heat from the mould, i.e. leave warmer than it enters, and the
# pick-up must stay positive and bounded over the run.
#
# Usage: verify-coolant-mold.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    samples = [
        (float(a), float(b), float(c))
        for a, b, c in re.findall(
            r"moldingChannelCooling: Tc in/out = ([-+0-9.eE]+) / "
            r"([-+0-9.eE]+) K, pick-up = ([-+0-9.eE]+) W",
            log,
        )
    ]
    if len(samples) < 3:
        print("FAIL: not enough coolant channel samples in the log")
        sys.exit(1)

    Tin, Tout, q = samples[-1]
    print("  coolant: inlet = {:.3f} K, outlet = {:.3f} K, pick-up = "
          "{:.3f} W".format(Tin, Tout, q))

    fail = False
    if not all(t > i for i, t, _ in samples):
        print("FAIL: the coolant did not heat up along the channel")
        fail = True
    if q <= 0:
        print("FAIL: the coolant pick-up is not positive")
        fail = True
    if q > 1e4:
        print("FAIL: the pick-up left a physical range")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the 1D channel cools the mould and heats the coolant")


if __name__ == "__main__":
    main()
