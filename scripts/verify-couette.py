#!/usr/bin/env python3
#******************************************************************************
# verify-couette.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the Couette shear-heating validation case.
#
# Plane Couette flow between a fixed lower wall and an upper wall moving at
# U, gap h, adiabatic walls, melt initially at T0. Once the linear profile
# is established the viscous dissipation is uniform,
#
#     Phi = eta(p0, T, gammaDot)*gammaDot^2,   gammaDot = U/h
#
# and the energy balance gives
#
#     dT/dt = Phi/(rho(T,p0)*Cv(T,p0))
#
# This script integrates that ODE with the same CrossWlf and Tait models
# (implemented here independently) and compares the predicted mass-weighted
# average temperature rise with the simulation.
#
# Usage: verify-couette.py <caseDir>       (exits non-zero on failure)
#******************************************************************************

import glob
import math
import os
import re
import sys

# CrossWlf coefficients (constant/momentumTransport)
CW = {
    "n": 0.393539,
    "tauStar": 64568.9,
    "D1": 3.76174e15,
    "D2": 153.15,
    "D3": 0.0,
    "A1": 33.21,
    "A2": 51.6,
    "etaMin": 5.0,
    "etaMax": 1e6,
    "gammaDotMin": 1e-6,
}

# Tait coefficients (constant/physicalProperties.melt)
TAIT = {
    "b1m": 1.2495299e-03,
    "b2m": 1.026e-06,
    "b1s": 1.070046e-03,
    "b2s": 2.077e-07,
    "b3": 1.042149833644e08,
    "b4": 0.004941,
    "b5": 390.65,
    "b6": 1.543e-07,
    "C": 0.0894,
}

CP_BASE = 2400.0        # J/kg/K (hMelt base Cp)
P0 = 1e5                # Pa
T0 = 480.0              # K initial temperature
U_WALL = 1.0            # m/s
GAP = 1.0e-3            # m


def eta(p, T, gamma_dot):
    g = max(gamma_dot, CW["gammaDotMin"])
    Tstar = CW["D2"] + CW["D3"]*p
    x = T - Tstar
    denom = max(CW["A2"] + x, 1e-15)
    e = min(-CW["A1"]*x/denom, math.log(CW["etaMax"]/CW["D1"]))
    eta0 = CW["D1"]*math.exp(e)
    value = eta0/(1.0 + (eta0*g/CW["tauStar"])**(1.0 - CW["n"]))
    return min(max(value, CW["etaMin"]), CW["etaMax"])


def tait(p, T):
    """Return (rho, CpMCv) for the smooth melt branch (T > Tt)."""
    Tt = TAIT["b5"] + TAIT["b6"]*p
    v0 = TAIT["b1m"] + TAIT["b2m"]*(T - Tt)
    B = TAIT["b3"]*math.exp(-TAIT["b4"]*T)
    f = 1.0 - TAIT["C"]*math.log(max(1.0 + p/B, 1e-30))
    v = v0*f
    vT = TAIT["b2m"]*f - TAIT["C"]*p*TAIT["b4"]*v0/(B + p)
    vP = -TAIT["b2m"]*TAIT["b6"]*f - TAIT["C"]*v0/(B + p)
    rho = 1.0/v
    CpMCv = -T*vT*vT/vP
    return rho, CpMCv


def series(case_dir, name):
    """Return {time: value} for a function object."""
    patterns = [
        os.path.join(case_dir, "postProcessing", name, "*", "*.dat"),
    ]
    data = {}
    for pattern in patterns:
        for path in sorted(glob.glob(pattern)):
            with open(path) as handle:
                for line in handle:
                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue
                    parts = line.split()
                    try:
                        data[float(parts[0])] = float(parts[-1])
                    except ValueError:
                        continue
    return data


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    mass = series(case_dir, "meltMass")
    integ = series(case_dir, "meltTemperatureIntegral")

    if not mass or not integ:
        print("FAIL: function object data missing (meltMass, "
              "meltTemperatureIntegral)")
        sys.exit(1)

    times = sorted(t for t in mass if t in integ)
    if len(times) < 2:
        print("FAIL: not enough samples")
        sys.exit(1)

    def avg_t(t):
        return integ[t]/mass[t]

    gdot = U_WALL/GAP

    # Integrate the model ODE independently (explicit, 1e-6 s steps)
    t_start = times[0]
    t_end = times[-1]
    nsteps = max(int((t_end - t_start)/1e-6), 1)
    dt = (t_end - t_start)/nsteps
    T = avg_t(t_start)
    for _ in range(nsteps):
        rho, CpMCv = tait(P0, T)
        Cv = CP_BASE - CpMCv
        phi = eta(P0, T, gdot)*gdot*gdot
        T += dt*phi/(rho*Cv)
    predicted = T

    measured = avg_t(t_end)

    print("Couette shear heating validation")
    print("  gammaDot          = {:.1f} 1/s".format(gdot))
    print("  average T(t=0)    = {:.6f} K".format(avg_t(times[0])))
    print("  average T(end)    = {:.6f} K  (t = {} s)".format(
        measured, t_end))
    print("  model-predicted   = {:.6f} K".format(predicted))
    print("  relative error    = {:.3e}".format(
        abs(measured - predicted)/abs(predicted - avg_t(times[0]))))

    error = abs(measured - predicted)/abs(predicted - avg_t(times[0]))
    if error < 0.02:
        print("PASS: shear heating matches the model prediction (< 2%)")
        sys.exit(0)
    else:
        print("FAIL: shear heating deviates from the model prediction "
              "(>= 2%)")
        sys.exit(1)


if __name__ == "__main__":
    main()
