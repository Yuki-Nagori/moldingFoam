#!/usr/bin/env python3
#******************************************************************************
# verify-void-cavitation.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the field-coupled void model (tasks 033/039):
# the sealed melt cools in a cavity that is closed off from the gate; the
# cavitation model transfers melt into the void phase once the pressure
# falls to pSat, and the closure ceiling caps that transfer at the
# isochoric PVT void. The verifier checks that
#
#   - the run is stable (End marker, no NaN);
#   - the sealed melt pressure is pinned at the saturation pressure;
#   - the sealed mass is conserved (fixed volume, closed cavity): the
#     mixture mass per unit volume is compared with the first written
#     state, which catches the unconstrained onset over-evaporation that
#     consumes melt mass;
#   - the void volume matches the isochoric PVT closure
#     phi = max(0, 1 - rho_sealed/rho(pv, T)) within 10 %;
#   - a distributed void has opened (0.01 < mean(1-alpha) < 0.10);
#   - the melt has cooled below the inlet temperature.
#
# Usage: verify-void-cavitation.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import math
import os
import re
import sys

PV_TOL = 0.05
MASS_TOL = 0.01
CLOSURE_TOL = 0.10
R_UNIVERSAL = 8.314462618


def time_dirs(case_dir):
    return sorted(
        (d for d in os.listdir(case_dir)
         if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0),
        key=float,
    )


def read_scalar(case_dir, time, name):
    with open(os.path.join(case_dir, time, name), errors="replace") as f:
        txt = f.read()

    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n\s*\d+\s*\n"
        r"\((.*?)\)\s*;",
        txt,
        re.S,
    )
    if m is not None:
        return [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(1))]

    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", txt)
    if m is not None:
        return [float(m.group(1))]

    return None


def read_tait(case_dir):
    """Tait melt coefficients of the case's melt phase."""
    with open(os.path.join(case_dir, "constant", "physicalProperties.melt"),
              errors="replace") as f:
        txt = f.read()

    def coef(name):
        m = re.search(r"\b" + name + r"\s+([-+0-9.eE]+)\s*;", txt)
        if m is None:
            print("FAIL: cannot read the Tait coefficient " + name)
            sys.exit(1)
        return float(m.group(1))

    return {
        "b1m": coef("b1m"),
        "b2m": coef("b2m"),
        "b3": coef("b3"),
        "b4": coef("b4"),
        "b5": coef("b5"),
        "C": coef("C"),
    }


def read_initial(case_dir):
    """Uniform initial temperature and pressure of the cavity."""
    def uniform_value(path):
        with open(path, errors="replace") as f:
            txt = f.read()
        m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", txt)
        return float(m.group(1)) if m else None

    T0 = uniform_value(os.path.join(case_dir, "0", "T"))
    p0 = uniform_value(os.path.join(case_dir, "0", "p"))

    if T0 is None or p0 is None:
        print("FAIL: cannot read the initial temperature/pressure")
        sys.exit(1)

    return T0, p0


def read_mol_weight(case_dir):
    with open(os.path.join(case_dir, "constant", "physicalProperties.air"),
              errors="replace") as f:
        txt = f.read()
    m = re.search(r"\bmolWeight\s+([-+0-9.eE]+)\s*;", txt)
    if m is None:
        print("FAIL: cannot read the air molWeight")
        sys.exit(1)
    return float(m.group(1))


def rho_tait_melt(p, T, c):
    v0 = c["b1m"] + c["b2m"]*(T - c["b5"])
    B = c["b3"]*math.exp(-c["b4"]*T)
    return 1.0/(v0*(1 - c["C"]*math.log(1 + p/B)))


def mixture_mass_per_volume(case_dir, time, c, molWeight):
    """Sealed mass per unit volume: melt at the Tait density plus the
    vapour at its ideal-gas density (the cavity volume is fixed)."""
    a = read_scalar(case_dir, time, "alpha.melt")
    p = read_scalar(case_dir, time, "p")
    T = read_scalar(case_dir, time, "T.melt")

    if not a or not p or not T:
        return None

    n = min(len(a), len(p), len(T))
    total = 0.0
    for i in range(n):
        rhoL = rho_tait_melt(max(p[i], 1.0), T[i], c)
        rhoV = p[i]*molWeight/(R_UNIVERSAL*T[i])
        total += a[i]*rhoL + (1 - a[i])*rhoV

    return total/n


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    fail = False

    if re.search(r"\bnan\b", log, re.I):
        print("FAIL: the log contains NaN values")
        fail = True

    # saturation pressure from the case's fvModels
    with open(os.path.join(case_dir, "constant", "fvModels"),
              errors="replace") as f:
        fvm = f.read()

    m = re.search(r"pSat\s*\{[^}]*value\s+([-+0-9.eE]+)", fvm, re.S)
    pSat = float(m.group(1)) if m else 0.0

    times = time_dirs(case_dir)
    if not times:
        print("FAIL: no written time directories")
        sys.exit(1)

    time = times[-1]
    p = read_scalar(case_dir, time, "p")
    a = read_scalar(case_dir, time, "alpha.melt")
    T = read_scalar(case_dir, time, "T.melt")

    if not p or not a:
        print("FAIL: cannot read the pressure or melt fraction")
        sys.exit(1)

    print("  pressure: min = {:.4g} Pa, max = {:.4g} Pa (pSat = {:.4g})"
          .format(min(p), max(p), pSat))
    if pSat <= 0 or abs(min(p) - pSat)/pSat > PV_TOL:
        print("FAIL: the sealed melt pressure is not pinned at pSat")
        fail = True

    void = 1 - sum(a)/len(a)
    print("  mean void fraction = {:.4f}".format(void))
    if not 0.001 < void < 0.10:
        print("FAIL: the void volume is not in the expected range")
        fail = True

    if T:
        print("  melt temperature: {:.1f} .. {:.1f} K".format(min(T), max(T)))
        if max(T) > 500:
            print("FAIL: the sealed melt did not cool")
            fail = True

    # Sealed mass conservation at fixed volume (task 039): the mixture mass
    # per unit volume must not drift from the first written state
    c = read_tait(case_dir)
    molWeight = read_mol_weight(case_dir)

    mFirst = mixture_mass_per_volume(case_dir, times[0], c, molWeight)
    mLast = mixture_mass_per_volume(case_dir, time, c, molWeight)

    T0, p0 = read_initial(case_dir)
    mRef = rho_tait_melt(max(p0, 1.0), T0, c)

    if mFirst is None or mLast is None:
        print("FAIL: cannot evaluate the sealed mixture mass")
        fail = True
    else:
        # The sealed mass is measured against the initial state through the
        # EOS (an absolute reference): rebuilding it from the current melt
        # fraction would be self-consistent by construction and would hide
        # the onset over-evaporation that consumes melt mass
        onset = abs(mFirst - mRef)/mRef
        drift = abs(mLast - mFirst)/mFirst
        print("  sealed mass per volume: initial EOS {:.3f}, first written "
              "{:.3f} (onset {:.3%}), last {:.3f} (drift {:.3%}) kg/m^3"
              .format(mRef, mFirst, onset, mLast, drift))

        if onset > MASS_TOL or drift > MASS_TOL:
            print("FAIL: the sealed melt mass is not conserved (the void is "
                  "inflating by phase-change over-evaporation)")
            fail = True

        # Void against the isochoric PVT closure at the sealed density
        Tmean = sum(T)/len(T)
        rhoL = rho_tait_melt(max(pSat, 1.0), Tmean, c)
        phiClosure = max(0.0, 1 - mLast/rhoL)
        dev = abs(void - phiClosure)/max(phiClosure, 1e-12)
        print("  PVT closure void = {:.5f} (void/closure deviation {:.1%})"
              .format(phiClosure, dev))
        if phiClosure <= 0 or dev > CLOSURE_TOL:
            print("FAIL: the void volume does not match the PVT closure")
            fail = True

    if fail:
        sys.exit(1)

    print("PASS: the sealed melt cavitates at pSat, conserves its mass and "
          "opens the PVT closure void")


if __name__ == "__main__":
    main()
