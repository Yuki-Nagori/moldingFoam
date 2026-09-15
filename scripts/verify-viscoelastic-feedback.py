#!/usr/bin/env python3
#******************************************************************************
# verify-viscoelastic-feedback.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Manufactured stress-gradient momentum impulse, one step on 80 cells.
# Usage: verify-viscoelastic-feedback.py <caseDir>
#******************************************************************************

import math
import pathlib
import re
import sys


def main():
    case = pathlib.Path(sys.argv[1])
    times = sorted((p for p in case.iterdir() if p.is_dir()
                    and re.fullmatch(r"[0-9.eE+-]+", p.name)), key=lambda p: float(p.name))
    final = times[-1]
    text = (final / "U").read_text()
    m = re.search(r"internalField\s+nonuniform\s+List<vector>\s+80\s*\((.*?)\)\s*;", text, re.S)
    if not m:
        raise ValueError("missing 80-cell velocity field")
    values = [tuple(map(float, row.split())) for row in re.findall(r"\(([^()]*)\)", m[1])]
    # Manufactured initial shear stress tau_xy=1e8*y. In the interior,
    # rho*dU_x/dt=d(tau_xy)/dy, independent of the local stress ODE.
    # Fixed case coefficients; never infer the expected force from output tau.
    temperature, pressure = 480, 1e5
    v0 = 1.2495299e-3 + 1.026e-6*(temperature-390.65-1.543e-7*pressure)
    bulk = 1.042149833644e8*math.exp(-.004941*temperature)
    rho = 1/(v0*(1-.0894*math.log1p(pressure/bulk)))
    dt = 1e-8
    expected = dt*1e8/rho
    if abs(float(final.name)-dt) > 1e-15 or len(values) != 80:
        raise ValueError("wrong time or mesh")
    if any(len(row) != 3 or not all(map(math.isfinite, row)) for row in values):
        raise ValueError("nonfinite or malformed velocity field")
    error = max(abs(row[0]/expected-1) for row in values[12:-12])
    if not math.isfinite(error) or error > .03:
        raise ValueError(f"momentum feedback relative error {error:.6g} > 3%")
    print(f"PASS: stress gradient drives momentum, relative error {error:.3g}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, IndexError) as exc:
        print(f"FAIL: {exc}")
        sys.exit(1)
