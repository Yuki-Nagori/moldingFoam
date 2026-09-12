#!/usr/bin/env python3
#******************************************************************************
# void_cooling.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Zero-dimensional void closure for a sealed cell (task 033).
#
# A sealed cell keeps its mass m and volume V, so its isochoric pressure
# follows from inverting the Tait EOS at the fixed bulk density
# rho = m/V. When that pressure would fall below the cavitation pressure
# pv the melt cannot sustain the tension: the pressure is pinned at pv
# and the volume deficit becomes void,
#
#     p   = max(p_iso(rho, T), pv)
#     phi = max(0, 1 - rho/rho_tait(pv, T))
#
# i.e. the complementarity p >= pv, phi >= 0 and (p - pv) phi = 0. This
# is the constitutive closure the future two-field solver module will
# evaluate; it also cross-checks the voidFraction indicator of the
# delivered 018a treatment.
#
# Usage:
#   void_cooling.py --selftest
#   void_cooling.py --case <caseDir>   (per-cell check against the solver's
#                                       voidFraction field, task 018a)
#******************************************************************************

import math
import os
import re
import sys

# HDPE-like Tait coefficients (see validation/highPressure)
B1M, B2M = 1.2495299e-03, 1.026e-06
B1S, B2S = 1.070046e-03, 2.077e-07
B3M, B4M = 1.042149833644e08, 0.004941
B3S, B4S = 3.3241950281e08, 2.46e-06
B5, C = 390.65, 0.0894


def v0m(T):
    return B1M + B2M*(T - B5)


def v0s(T):
    return B1S + B2S*(T - B5)


def rho_tait_melt(p, T):
    B = B3M*math.exp(-B4M*T)
    return 1.0/(v0m(T)*(1 - C*math.log(1 + p/B)))


def rho_tait(p, T):
    return rho_tait_melt(p, T) if T >= B5 else rho_tait_melt(p, B5)


def p_iso_melt(rho, T):
    B = B3M*math.exp(-B4M*T)
    return B*(math.exp((1 - 1.0/(rho*v0m(T)))/C) - 1)


def void_state(rho, T, pv):
    """Pressure and void fraction of a sealed cell (complementarity)."""
    p = max(p_iso_melt(rho, T), pv)
    phi = max(0.0, 1 - rho/rho_tait_melt(pv, T))
    return p, phi


def selftest():
    ok = True

    # 1. The complementarity over a sealed cooling sweep
    rho0 = rho_tait_melt(1e5, 480)     # sealed at atmospheric, 480 K
    worst_p, worst_phi, worst_c = 1e30, 0.0, 0.0

    for k in range(101):
        T = 480 - k*0.5
        p, phi = void_state(rho0, T, 0.0)

        worst_p = min(worst_p, p)
        worst_phi = max(worst_phi, phi)
        worst_c = max(worst_c, abs(min(p, 0.0))*phi)

    print("  sealed rho0 = %.2f kg/m^3, T 480->430 K:" % rho0)
    print("    p_min = %.3e Pa, phi_max = %.6f, complementarity = %.3e"
          % (worst_p, worst_phi, worst_c))

    if worst_p < -1e-6:
        print("FAIL: the pressure left the floor (p < pv)")
        ok = False
    if worst_phi < 0:
        print("FAIL: the void fraction went negative")
        ok = False
    if worst_c > 1e-12:
        print("FAIL: the complementarity (p - pv) phi = 0 is violated")
        ok = False

    # 2. Void onset consistency: phi = 0 at p_iso = pv and the PVT value
    T = 430.0
    rho_pv = rho_tait_melt(0.0, T)
    phi_pvt = 1 - rho0/rho_pv
    p, phi = void_state(rho0, T, 0.0)
    print("  at T = 430 K: p_iso = %.4e Pa, phi = %.6f (PVT %.6f)"
          % (p_iso_melt(rho0, T), phi, phi_pvt))

    if phi > 0 and abs(phi - phi_pvt) > 1e-12:
        print("FAIL: the void fraction does not match the PVT")
        ok = False

    # 3. Isochoric inversion round trip
    for p in (1e5, 1e7, 4e7):
        T = 470.0
        rho = rho_tait_melt(p, T)
        if abs(p_iso_melt(rho, T) - p)/p > 1e-12:
            print("FAIL: the isochoric inversion is not exact")
            ok = False

    print("PASS: the 0D void closure satisfies the complementarity and the "
          "PVT" if ok else "FAIL")
    return 0 if ok else 1


def check_case(case_dir):
    """Compare the 0D phi with the solver's voidFraction field."""
    times = sorted(
        (d for d in os.listdir(case_dir)
         if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0),
        key=float,
    )
    if not times:
        print("FAIL: no written time directories")
        return 1

    def read(name):
        with open(os.path.join(case_dir, times[-1], name),
                  errors="replace") as f:
            txt = f.read()
        m = re.search(
            r"internalField\s+nonuniform\s+List<scalar>\s*\n\s*\d+\s*"
            r"\n\((.*?)\)\s*;",
            txt,
            re.S,
        )
        return [float(x) for x in m.group(1).split()]

    rho = read("rho")
    T = read("T.melt")
    vf = read("voidFraction")
    alpha = read("alpha.melt")

    dmax = 0.0
    n = 0
    for i in range(len(rho)):
        if alpha[i] <= 0.9999:
            continue
        _, phi = void_state(rho[i], T[i], 0.0)
        dmax = max(dmax, abs(phi - vf[i]))
        n += 1

    print("  {:d} melt cells, max |0D phi - voidFraction| = {:.3e}".format(
        n, dmax))

    if n == 0 or dmax > 1e-4:
        print("FAIL: the 0D closure disagrees with the solver field")
        return 1

    print("PASS: the 0D void closure reproduces the solver indicator")
    return 0


if __name__ == "__main__":
    if "--case" in sys.argv:
        sys.exit(check_case(sys.argv[sys.argv.index("--case") + 1]))
    sys.exit(selftest())
