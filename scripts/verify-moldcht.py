#!/usr/bin/env python3
#******************************************************************************
# verify-moldcht.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the two-region conjugate heat transfer
# benchmark: a melt cavity (moldingFoam) on top of a mould block (solid)
# with a shared interface, an adiabatic cavity bottom and a fixed mould
# top. The interface temperature and the cavity average temperature are
# compared with an independent one-dimensional two-layer implicit
# finite-difference reference solution.
#
# Usage: verify-moldcht.py <caseDir>       (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

# --- benchmark parameters (must match the case dictionaries) ---
LF = 0.002          # cavity thickness [m]
LM = 0.005          # mould thickness [m]
NF = 8              # cavity cells
NM = 10             # mould cells
KF = 0.25           # melt conductivity [W/m/K]
KM = 50.0           # mould conductivity [W/m/K]
RMC = 7800.0*500.0  # mould volumetric heat capacity [J/m^3/K]
CP_BASE = 2400.0    # melt base heat capacity [J/kg/K]

# Tait coefficients of the melt (constant/cavity/physicalProperties.melt)
TAIT = {
    "b1m": 1.2495299e-03, "b2m": 1.026e-06,
    "b1s": 1.070046e-03, "b2s": 2.077e-07,
    "b3": 1.042149833644e08, "b4": 0.004941,
    "b5": 390.65, "b6": 1.543e-07, "C": 0.0894,
}
P0 = 1e5
T0F = 480.0         # initial cavity temperature [K]
T0M = 353.0         # initial mould temperature [K]
TTOP = 353.0        # mould top temperature [K]
DT = 0.005          # time step [s] (overridden from the case controlDict)


def boundary_value(path, patch):
    """Return the mean patch value of a written field."""
    with open(path) as handle:
        text = handle.read()

    match = re.search(
        r"\b" + patch + r"\b\s*\{(.*?)\n    \}", text, re.S)
    if not match:
        raise RuntimeError("patch {} not found in {}".format(patch, path))
    body = match.group(1)

    m = re.search(r"value\s+uniform\s+([-+0-9.eE]+)", body)
    if m:
        return float(m.group(1))

    m = re.search(
        r"value\s+nonuniform\s+List<scalar>\s*\n?\s*(\d+)\s*\((.*?)\)",
        body, re.S)
    vals = [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]
    return sum(vals)/len(vals)


def cell_average(path):
    """Return the internal-field mean of a written field."""
    with open(path) as handle:
        text = handle.read()

    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n?\s*(\d+)\s*\((.*?)\)",
        text, re.S)
    if not m:
        m = re.search(
            r"internalField\s+uniform\s+([-+0-9.eE]+)", text)
        return float(m.group(1))

    vals = [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]
    return sum(vals)/len(vals)


def melt_rho_Cv(T):
    """Melt density and Cv from the Tait EOS at p = P0."""
    Tt = TAIT["b5"] + TAIT["b6"]*P0
    if T > Tt:
        v0 = TAIT["b1m"] + TAIT["b2m"]*(T - Tt)
        v0p = TAIT["b2m"]
        b3 = TAIT["b3"]
        b4 = TAIT["b4"]
    else:
        v0 = TAIT["b1s"] + TAIT["b2s"]*(T - Tt)
        v0p = TAIT["b2s"]
        b3 = TAIT["b3"]
        b4 = TAIT["b4"]

    B = b3*__import__("math").exp(-b4*T)
    f = 1.0 - TAIT["C"]*__import__("math").log(max(1.0 + P0/B, 1e-30))
    v = v0*f
    vT = v0p*f - TAIT["C"]*P0*b4*v0/(B + P0)
    vP = -v0p*TAIT["b6"]*f - TAIT["C"]*v0/(B + P0)
    rho = 1.0/v
    CpMCv = -T*vT*vT/vP
    return rho, CP_BASE - CpMCv


def reference(nsteps, sample_steps):
    """1D two-layer implicit FD; returns (t, T_interface, T_cavity_mean)."""
    n = NF + NM
    dzf = LF/NF
    dzm = LM/NM
    # node conductances between adjacent nodes (harmonic mean)
    k = [KF]*NF + [KM]*NM
    dz = [dzf]*NF + [dzm]*NM

    # face conductances per unit area
    gf = []
    for i in range(n - 1):
        g = 2.0/(dz[i]/k[i] + dz[i + 1]/k[i + 1])
        gf.append(g)

    T = [T0F]*NF + [T0M]*NM

    samples = []
    for step in range(1, nsteps + 1):
        # tridiagonal coefficients: a_i T_{i-1} + b_i T_i + c_i T_{i+1} = d_i
        a = [0.0]*n
        b = [0.0]*n
        c = [0.0]*n
        d = [0.0]*n

        for i in range(n):
            if i < NF:
                rho, Cv = melt_rho_Cv(T[i])
                rc = rho*Cv
            else:
                rc = RMC
            b[i] = rc*dz[i]/DT
            d[i] = b[i]*T[i]
            if i > 0:
                a[i] = -gf[i - 1]
                b[i] += gf[i - 1]
            if i < n - 1:
                c[i] = -gf[i]
                b[i] += gf[i]

        # mould top fixed
        b[n - 1] += gf[n - 2]
        d[n - 1] += gf[n - 2]*TTOP
        # cavity bottom adiabatic: a[0]=0, b[0] already without gf[-1]

        # Thomas algorithm
        cp = [0.0]*n
        dp = [0.0]*n
        cp[0] = c[0]/b[0]
        dp[0] = d[0]/b[0]
        for i in range(1, n):
            m = b[i] - a[i]*cp[i - 1]
            cp[i] = c[i]/m if i < n - 1 else 0.0
            dp[i] = (d[i] - a[i]*dp[i - 1])/m
        T[n - 1] = dp[n - 1]
        for i in range(n - 2, -1, -1):
            T[i] = dp[i] - cp[i]*T[i + 1]

        if step in sample_steps:
            samples.append((step*DT, T[NF], sum(T[:NF])/NF))

    return samples


def main():
    global DT
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    # Read the time step from the case so the reference uses the same one
    with open(os.path.join(case_dir, "system", "controlDict")) as handle:
        m = re.search(r"\bdeltaT\s+([-+0-9.eE]+)\s*;", handle.read())
        if m:
            DT = float(m.group(1))

    times = sorted(
        int(d) for d in os.listdir(case_dir)
        if re.match(r"^[0-9]+$", d)
        and os.path.isdir(os.path.join(case_dir, d))
        and int(d) > 0
    )
    if not times:
        print("FAIL: no written time directories")
        sys.exit(1)

    t_end = max(times)
    nsteps = int(round(t_end/DT))
    sample_steps = set(int(round(t/DT)) for t in times)

    ref = reference(nsteps, sample_steps)
    ref_t = {int(round(t)): (ti, tc) for t, ti, tc in ref}

    max_ierr = 0.0
    max_cerr = 0.0
    max_cont = 0.0

    for t in times:
        ci = boundary_value(
            os.path.join(case_dir, str(t), "cavity", "T"), "cavity_to_mold")
        mi = boundary_value(
            os.path.join(case_dir, str(t), "mold", "T"), "mold_to_cavity")
        ca = cell_average(os.path.join(case_dir, str(t), "cavity", "T"))

        max_cont = max(max_cont, abs(ci - mi))

        ti_ref, tc_ref = ref_t.get(t, (None, None))
        if ti_ref is None:
            continue

        ierr = abs(ci - ti_ref)/(T0F - T0M)
        cerr = abs(ca - tc_ref)/(T0F - T0M)
        max_ierr = max(max_ierr, ierr)
        max_cerr = max(max_cerr, cerr)

        print("  t = {:>2d} s: T_int = {:.3f} K (ref {:.3f}), "
              "T_cav = {:.3f} K (ref {:.3f})".format(
                  t, ci, ti_ref, ca, tc_ref))

    print("  max interface continuity error = {:.3e} K".format(max_cont))
    print("  max interface relative error   = {:.3e}".format(max_ierr))
    print("  max cavity average rel. error  = {:.3e}".format(max_cerr))

    if max_cont > 1e-3:
        print("FAIL: interface temperature is not continuous")
        sys.exit(1)

    # Converged thresholds: with the constant-density benchmark properties
    # the interface matches the reference to ~0.2% and the cavity average
    # to ~1.3%; the checks sit just above that
    if max_ierr < 0.01 and max_cerr < 0.03:
        print("PASS: CHT interface and cavity response match the two-layer "
              "reference (interface < 1%, cavity < 3%)")
        sys.exit(0)
    else:
        print("FAIL: CHT solution deviates from the two-layer reference")
        sys.exit(1)


if __name__ == "__main__":
    main()
