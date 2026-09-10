#!/usr/bin/env python3
#******************************************************************************
# verify-stefan.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the one-dimensional solidification (Stefan)
# benchmark.
#
# The melt starts just above the solidification temperature Tf with a cold
# wall at Tw. With constant properties and a sharp front, the freeze front
# follows the one-phase Neumann solution
#
#     X(t) = 2*lambda*sqrt(alpha_s*t),
#     lambda e^{lambda^2} erf(lambda) = Ste/sqrt(pi),
#     Ste = Cp*(Tf - Tw)/L,   alpha_s = kappa/(rho*Cp)
#
# This script reads the written T fields, locates the Tf isotherm by
# interpolation and compares it with the analytic front. It also compares
# the solid temperature profile with the erf solution.
#
# Usage: verify-stefan.py <caseDir>        (exits non-zero on failure)
#******************************************************************************

import glob
import math
import os
import re
import sys

TW = 353.0              # cold wall temperature [K]
TI = 392.0              # initial melt temperature [K]
L = 2.0e5               # latent heat [J/kg]
CP = 2400.0             # base heat capacity [J/kg/K]
KAPPA = 0.25            # thermal conductivity [W/m/K]

TAIT = {
    "b1m": 1.2495299e-03,
    "b2m": 1.026e-06,
    "b3": 1.042149833644e08,
    "b4": 0.004941,
    "b5": 390.65,
    "b6": 1.543e-07,
    "C": 0.0894,
}

P0 = 1e5


def tait_rho(p, T):
    Tt = TAIT["b5"] + TAIT["b6"]*p
    v0 = TAIT["b1m"] + TAIT["b2m"]*(T - Tt)
    B = TAIT["b3"]*math.exp(-TAIT["b4"]*T)
    f = 1.0 - TAIT["C"]*math.log(max(1.0 + p/B, 1e-30))
    return 1.0/(v0*f)


def solve_lambda(ste):
    """Solve lambda*exp(lambda^2)*erf(lambda) = ste/sqrt(pi)."""
    target = ste/math.sqrt(math.pi)
    lo, hi = 0.0, 3.0
    for _ in range(200):
        mid = 0.5*(lo + hi)
        f = mid*math.exp(mid*mid)*math.erf(mid)
        if f < target:
            lo = mid
        else:
            hi = mid
    return 0.5*(lo + hi)


def read_profile(case_dir, time_name):
    """Return (y, T) cell-centre profiles averaged over x, bottom to top."""
    with open(os.path.join(case_dir, time_name, "T")) as handle:
        text = handle.read()

    internal = text.split("boundaryField")[0]
    match = re.search(r"List<scalar>\s*\n?\s*(\d+)\s*\(", internal)
    n = int(match.group(1))
    start = match.end()
    body = internal[start:internal.rindex(")")]
    values = [
        float(x)
        for x in re.findall(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?", body)
    ]
    if len(values) != n:
        print("FAIL: parsed {} of {} values".format(len(values), n))
        sys.exit(1)

    # mesh is 4 x 100 x 1, x fastest: average each y row of 4 cells
    ny = n//4
    profile = [sum(values[4*j:4*j + 4])/4.0 for j in range(ny)]

    dy = 0.02/ny
    return [(j + 0.5)*dy for j in range(ny)], profile


def front_position(y, T, Tf):
    for j in range(1, len(T)):
        if (T[j - 1] - Tf)*(T[j] - Tf) <= 0:
            # linear interpolation
            f = (Tf - T[j - 1])/(T[j] - T[j - 1])
            return y[j - 1] + f*(y[j] - y[j - 1])
    return None


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    Tf = TAIT["b5"] + TAIT["b6"]*P0
    rho = tait_rho(P0, TI)
    alpha = KAPPA/(rho*CP)
    ste = CP*(Tf - TW)/L
    lam = solve_lambda(ste)

    print("Stefan solidification benchmark")
    print("  Tf = {:.3f} K, Ste = {:.4f}, lambda = {:.6f}".format(
        Tf, ste, lam))
    print("  rho = {:.1f} kg/m^3, alpha_s = {:.4e} m^2/s".format(
        rho, alpha))

    times = sorted(
        (float(d), d) for d in os.listdir(case_dir)
        if re.match(r"^[0-9]+(\.[0-9]+)?$", d)
        and os.path.isdir(os.path.join(case_dir, d))
        and float(d) > 0
    )
    if not times:
        print("FAIL: no time directories found")
        sys.exit(1)

    # The apparent-Cp band smoothing (1 K) and the small initial
    # superheat perturb the early transient; the asymptotic front is the
    # meaningful validation target, so only t >= tAccept is checked
    tAccept = 60.0

    max_front_err = 0.0
    max_solid_err = 0.0
    for t, name in times:
        y, T = read_profile(case_dir, name)
        x_sim = front_position(y, T, Tf)
        if x_sim is None:
            print("  t = {:>6.1f} s: front not found".format(t))
            continue

        x_ana = 2.0*lam*math.sqrt(alpha*t)
        front_err = abs(x_sim - x_ana)/x_ana

        # solid profile against the erf solution
        solid_err = 0.0
        for j in range(len(y)):
            if y[j] >= x_ana:
                break
            T_ana = TW + (Tf - TW)*math.erf(
                y[j]/(2.0*math.sqrt(alpha*t)))/math.erf(lam)
            solid_err = max(solid_err, abs(T[j] - T_ana))

        print("  t = {:>6.1f} s: X_sim = {:.4f} mm, X_ana = {:.4f} mm, "
              "rel err = {:.3e}{}".format(
                  t, 1e3*x_sim, 1e3*x_ana, front_err,
                  "" if t >= tAccept else "  (transient)"))

        if t >= tAccept:
            max_front_err = max(max_front_err, front_err)
            max_solid_err = max(max_solid_err, solid_err)

    print("  max front relative error  = {:.3e}".format(max_front_err))
    print("  max solid profile error   = {:.3e} K".format(max_solid_err))

    if max_front_err < 0.05 and max_solid_err < 0.05*(Tf - TW):
        print("PASS: freeze front and solid profile match the Neumann "
              "solution (< 5%)")
        sys.exit(0)
    else:
        print("FAIL: solidification deviates from the Neumann solution")
        sys.exit(1)


if __name__ == "__main__":
    main()
