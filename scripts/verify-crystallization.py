#!/usr/bin/env python3
#******************************************************************************
# verify-crystallization.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the crystallisation kinetics solver case:
# the melt cools through the crystallisation window and the relative
# crystallinity field must grow monotonically from 0 towards 1, staying
# bounded, while the released latent heat is fed to the energy equation.
#
# Usage: verify-crystallization.py <caseDir>   (exits non-zero on failure)
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

    chi = [float(x) for x in re.findall(r"max\(chi\) = ([-+0-9.eE]+)", log)]
    if len(chi) < 3:
        print("FAIL: not enough crystallinity samples in the log")
        sys.exit(1)

    print("  chi history: first = {:.6f}, last = {:.6f}, samples = {}".format(
        chi[0], chi[-1], len(chi)))

    fail = False
    if chi[0] > 0.05:
        print("FAIL: the first crystallinity sample is too large "
              "(the log starts after the first 50 steps)")
        fail = True
    if min(chi) < -1e-12 or max(chi) > 1 + 1e-12:
        print("FAIL: chi left the physical range [0, 1]")
        fail = True
    if chi[-1] < 0.9:
        print("FAIL: the melt did not crystallise")
        fail = True

    # Monotonic growth (allowing tiny numerical noise)
    for a, b in zip(chi, chi[1:]):
        if b < a - 1e-9:
            print("FAIL: chi decreased from {:.6f} to {:.6f}".format(a, b))
            fail = True
            break

    if fail:
        sys.exit(1)

    print("PASS: crystallinity grows monotonically and bounded to "
          "near 1")


if __name__ == "__main__":
    main()
