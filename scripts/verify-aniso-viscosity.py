#!/usr/bin/env python3
#******************************************************************************
# verify-aniso-viscosity.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the Lipscomb anisotropic viscosity of the
# fibre-orientation coupling (tasks 034/060).
#
# The shear is imposed at the moving wall, so the uncorrected wall shear
# stress is exactly eta(p, T, gammaDot) gammaDot, with eta from the CrossWlf
# model evaluation shared with verify-couette.py. With the Lipscomb
# correction on the apparent viscosity is that value multiplied by
#
#     f = 1 + (lipscombRatio - 1) 3/2 (a:D)^2/(D:D),   f <= lipscombRatio
#
# which the verifier pins as a LOWER BOUND rather than an exact value: the
# shear rotates the orientation tensor through the Folgar-Tucker equation, so
# the factor that acts on the momentum equation is history dependent (the
# measured increase here is ~18% at lipscombRatio 4, against the 225% the
# initial 45-degree tensor would give if it stayed put).
#
# Sensitivity: with the correction disabled in the implementation (while
# lipscombRatio still says 4) the wall force is the uncorrected one, the
# increase collapses to 0 and the check fails - a lower bound is enough for
# that, and it also catches a sign error or an over-weak implementation.
#
# Usage: verify-aniso-viscosity.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import glob
import math
import os
import re
import sys

MIN_INCREASE = 0.08     # the coupling must raise the wall force by at least
                        # this much (measured ~0.18 at lipscombRatio 4)

# CrossWlf coefficients (constant/momentumTransport) - shared with
# verify-couette.py
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

# Tait coefficients (constant/physicalProperties.melt) - shared
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


def eta(p, T, gamma_dot):
    g = max(gamma_dot, CW["gammaDotMin"])
    Tstar = CW["D2"] + CW["D3"]*p
    x = T - Tstar
    denom = max(CW["A2"] + x, 1e-15)
    e = min(-CW["A1"]*x/denom, math.log(CW["etaMax"]/CW["D1"]))
    eta0 = CW["D1"]*math.exp(e)
    value = eta0/(1.0 + (eta0*g/CW["tauStar"])**(1.0 - CW["n"]))
    return min(max(value, CW["etaMin"]), CW["etaMax"])


def series(case_dir, name):
    """Return {time: value} for a function object (as in verify-couette.py)"""
    data = {}
    for path in sorted(glob.glob(
            os.path.join(case_dir, "postProcessing", name, "*", "*.dat"))):
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


def wallForce(log):
    """(time, x-force) samples of the integrated wall shear stress"""
    out = []
    t = -1.0
    timeRe = re.compile(r"^Time = ([-+0-9.eE]+)")
    forceRe = re.compile(
        r"areaIntegrate\(top\) of wallShearStress = \(\s*([-+0-9.eE]+)")

    for line in log.splitlines():
        m = timeRe.match(line)
        if m:
            t = float(m.group(1))
            continue

        m = forceRe.search(line)
        if m and t > 0:
            out.append((t, float(m.group(1))))

    return out


def scalar(text, key, default=None):
    m = re.search(r"(?m)^\s*" + key + r"\s+([-+0-9.eE]+)\s*;", text)
    if m:
        return float(m.group(1))
    if default is not None:
        return default
    sys.exit(1)


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    forces = wallForce(log)
    if len(forces) < 10:
        print("FAIL: no wall-force samples in the log "
              "(is the wallForce function object present?)")
        sys.exit(1)

    # Geometry: imposed shear rate and the patch area of the force probe
    U = re.search(r"top\s*\{[^}]*value\s+uniform\s+\(\s*([-+0-9.eE]+)",
                  open(os.path.join(case_dir, "0", "U")).read(), re.S)
    if not U:
        print("FAIL: cannot read the moving-wall speed from 0/U")
        sys.exit(1)
    Uwall = float(U.group(1))

    bm = open(os.path.join(case_dir, "system", "blockMeshDict")).read()
    verts = [float(v) for v in
             re.findall(r"\(\s*[-+0-9.eE]+\s+([-+0-9.eE]+)\s+[-+0-9.eE]+\s*\)",
                        bm.split("blocks")[0])]
    gap = max(verts) - min(verts)
    gdot = Uwall/gap

    area = re.search(r"area = ([-+0-9.eE]+)", log)
    if not area:
        print("FAIL: cannot read the force-probe area from the log")
        sys.exit(1)
    A = float(area.group(1))

    ratio = scalar(open(os.path.join(case_dir, "constant", "momentumTransport")
                        ).read(), "lipscombRatio", 1.0)

    mass = series(case_dir, "meltMass")
    integ = series(case_dir, "meltTemperatureIntegral")
    if not mass or not integ:
        print("FAIL: function object data missing (meltMass, "
              "meltTemperatureIntegral)")
        sys.exit(1)

    times = sorted(t for t in mass if t in integ)

    def avg_t(t):
        return integ[t]/mass[t]

    tEnd = forces[-1][0]
    tRef = min(times, key=lambda t: abs(t - tEnd))
    T = avg_t(tRef)

    Fmeas = abs(forces[-1][1])
    Fbase = eta(P0, T, gdot)*gdot*A
    increase = Fmeas/Fbase - 1.0

    print("  gammaDot = {:.1f} 1/s, patch area = {:g} m^2, "
          "lipscombRatio = {:g}".format(gdot, A, ratio))
    print("  last sample: T = {:.4f} K, wall force = {:.6f} N, "
          "uncorrected baseline = {:.6f} N".format(T, Fmeas, Fbase))
    print("  coupling increase = {:+.2%} (lower bound required: {:+.0%})"
          .format(increase, MIN_INCREASE))

    if ratio <= 1.0:
        print("FAIL: this case must run with the Lipscomb correction enabled "
              "(lipscombRatio > 1)")
        sys.exit(1)

    if increase < MIN_INCREASE:
        print("FAIL: the Lipscomb correction does not raise the wall shear "
              "force (coupling off or changed?)")
        sys.exit(1)

    print("PASS: the Lipscomb correction raises the apparent viscosity of "
          "the sheared melt")


if __name__ == "__main__":
    main()
