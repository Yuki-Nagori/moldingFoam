#!/usr/bin/env python3
#******************************************************************************
# verify-parallel-budget.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Check that alpha and mass diagnostic fluxes use one global reduction.
# Usage: verify-parallel-budget.py <caseDir>
#******************************************************************************

import math
from pathlib import Path
import re
import sys

number = r"([-+0-9.eE]+)"
patch = re.compile(r"mass budget patch .*: alphaRhoPhi1 = "+number+r" kg/s, alphaPhi1 = "+number)
flux = re.compile(r"mass budget fluxes: vol\*dt = .*?alpha\*dt = "+number+r" kg, alphaRho\*dt = "+number)


def verify(path):
    mass_sum = alpha_sum = 0.0
    checked = 0
    for line in path.read_text().splitlines():
        if line.startswith("Time ="):
            mass_sum = alpha_sum = 0.0
        m = patch.search(line)
        if m:
            mass_sum += float(m[1])
            alpha_sum += float(m[2])
        m = flux.search(line)
        if m and abs(alpha_sum*mass_sum) > 1e-30:
            alpha_dt, mass_dt = map(float, m.groups())
            expected = mass_dt*alpha_sum/mass_sum
            error = abs(alpha_dt-expected)/max(abs(expected), 1e-30)
            if not math.isfinite(error) or error > 1e-4:
                raise ValueError(f"alpha flux has a duplicate/missing reduction: {error=}")
            checked += 1
    if not checked:
        raise ValueError("no nonzero alpha/mass flux pair checked")
    print(f"PASS: {checked} alpha/mass flux pairs share one global sum")


if __name__ == "__main__":
    try:
        verify(Path(sys.argv[1])/"log.foamRun")
    except (OSError, ValueError) as exc:
        print(f"FAIL: {exc}")
        sys.exit(1)
