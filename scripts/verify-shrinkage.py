#!/usr/bin/env python3
#******************************************************************************
# verify-shrinkage.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the shrinkage indicator solver case: as the
# melt cools and densifies, the free volumetric shrinkage S = 1 -
# rhoRef/rho must grow towards zero (the melt starts less dense than the
# solid reference) and stay physically bounded.
#
# Usage: verify-shrinkage.py <caseDir>   (exits non-zero on failure)
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

    S = [float(x) for x in re.findall(r"max\(S\) = ([-+0-9.eE]+)", log)]
    if len(S) < 3:
        print("FAIL: not enough shrinkage samples in the log")
        sys.exit(1)

    print("  max(S): first = {:.6f}, last = {:.6f}".format(S[0], S[-1]))

    fail = False
    if S[0] >= 0:
        print("FAIL: the melt did not start less dense than the reference")
        fail = True
    if S[-1] < S[0] + 0.003:
        print("FAIL: the shrinkage did not grow as the melt cooled")
        fail = True
    if min(S) < -0.5 or max(S) > 0.5:
        print("FAIL: the shrinkage left a physical range")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the shrinkage indicator grows as the melt cools and "
          "densifies")


if __name__ == "__main__":
    main()
