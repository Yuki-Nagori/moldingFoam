#!/usr/bin/env python3
#******************************************************************************
# verify-aniso-conduction.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the anisotropic thermal conductivity of the
# fibre-orientation coupling (tasks 034/060).
#
# The slab is quiescent and fully filled, with a fixed temperature on both
# ends and adiabatic sides. Its initial field is the steady linear profile
# plus a half-sine mode
#
#     T(y, 0) = Tlin(y) + A0 sin(pi y/L),   A0 = 5 K
#
# A uniform conductivity leaves the linear part alone, so the ONLY measurable
# consequence of the anisotropic conductivity is the decay rate of that mode,
#
#     lambda_1 = lambda_yy pi^2/(rho Cv L^2),
#     lambda_yy = kappa (1 + anisotropy (a_yy - 1/3)),
#
# integrated here with the implicit Euler factor of the solver:
# A(t) = A0 (1 + lambda_1 dt)^(-n).
#
# The volume-average probe carries the amplitude as
# mean(t) = C0 + c1 A(t) with geometric constants C0, c1 (computed below from
# the mesh, they only depend on the number of cells across the slab).
#
# Sensitivity: with the coupling off (anisotropy = 0, lambda_yy = kappa) the
# final amplitude is ~7% larger - far outside the 2% tolerance below.
#
# Usage: verify-aniso-conduction.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import math
import os
import re
import sys

TOL = 0.02          # allowed deviation of the measured decay from the
                    # analytic one (the coupling's signature is ~7%)


def read(path):
    with open(path, errors="replace") as f:
        return f.read()


def scalar(text, key, default=None):
    m = re.search(r"(?m)^\s*" + key + r"\s+([-+0-9.eE]+)\s*;", text)
    if m:
        return float(m.group(1))
    if default is not None:
        return default
    sys.exit(1)


def samples(log, pattern):
    """(time, value) pairs, using the Time = line that precedes each sample"""
    out = []
    t = -1.0
    timeRe = re.compile(r"^Time = ([-+0-9.eE]+)")
    sampRe = re.compile(pattern)

    for line in log.splitlines():
        m = timeRe.match(line)
        if m:
            t = float(m.group(1))
            continue

        m = sampRe.search(line)
        if m and t > 0:
            out.append((t, float(m.group(1))))

    return out


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    log = read(os.path.join(case_dir, "log.foamRun"))

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    # Geometry and mesh (the slab is uniform: the block gives the cell counts)
    bm = read(os.path.join(case_dir, "system", "blockMeshDict"))
    verts = [(float(x), float(y)) for x, y in
             re.findall(r"\(\s*([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+[-+0-9.eE]+\s*\)",
                        bm.split("blocks")[0])]
    ymin = min(v[1] for v in verts)
    ymax = max(v[1] for v in verts)
    L = ymax - ymin

    block = re.search(r"hex\s*\([^)]*\)\s*\(\s*(\d+)\s+(\d+)\s+(\d+)\s*\)", bm)
    nY = int(block.group(2))

    # Boundary temperatures by name
    Ttxt = read(os.path.join(case_dir, "0", "T"))
    m = re.search(r"hotWall\s*\{[^}]*value\s+uniform\s+([-+0-9.eE]+)", Ttxt, re.S)
    m2 = re.search(r"coldWall\s*\{[^}]*value\s+uniform\s+([-+0-9.eE]+)", Ttxt, re.S)
    if not (m and m2):
        print("FAIL: cannot read the hotWall/coldWall temperatures from 0/T")
        sys.exit(1)
    Thot, Tcold = float(m.group(1)), float(m2.group(1))

    # Fibre orientation along the slab and the conductivity anisotropy
    atxt = read(os.path.join(case_dir, "0", "a"))
    am = re.search(r"internalField\s+uniform\s*\(\s*([-+0-9.eE]+)\s+([-+0-9.eE]+)"
                   r"\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)", atxt)
    if not am:
        print("FAIL: cannot read the orientation tensor from 0/a")
        sys.exit(1)
    ayy = float(am.group(4))          # symmTensor (xx xy xz yy yz zz)

    mold = read(os.path.join(case_dir, "constant", "moldingDict"))
    aniso = scalar(mold, "conductivityAnisotropy", 0.0)

    props = read(os.path.join(case_dir, "constant", "physicalProperties.melt"))
    kappa = scalar(props, "kappa")
    Cp = scalar(props, "Cp")
    b1m = scalar(props, "b1m")
    rho = 1.0/b1m                     # constant-density benchmark (b2 = 0)

    dt = scalar(read(os.path.join(case_dir, "system", "controlDict")), "deltaT")

    series = samples(log, r"volAverage\(all\) of T = ([-+0-9.eE]+)")
    if len(series) < 10:
        print("FAIL: no volume-average temperature samples in the log")
        sys.exit(1)

    # Geometric constants: the linear part averages to the mid temperature,
    # the half-sine to c1 A, both over the cell centres across the slab
    C0 = 0.5*(Thot + Tcold)
    c1 = sum(math.sin(math.pi*(j + 0.5)/nY) for j in range(nY))/nY

    lam = kappa*(1.0 + aniso*(ayy - 1.0/3.0))
    lam1 = lam*math.pi**2/(rho*Cp*L**2)

    (t0, mean0), (t1, mean1) = series[0], series[-1]
    A_meas = (mean1 - C0)/c1
    n = round((t1 - t0)/dt)
    A_ana = ((mean0 - C0)/c1)*(1.0 + lam1*dt)**(-n)

    err = abs(A_meas - A_ana)/abs(A_ana)

    print("  mesh: {} x {} x {} (L = {:g} m), kappa = {:g} W/m/K, "
          "anisotropy = {:g}, a_yy = {:g}".format(
              nY, nY, 1, L, kappa, aniso, ayy))
    print("  lambda_yy = {:.6f} W/m/K (isotropic would be {:.6f}), "
          "lambda_1 = {:.6f} 1/s".format(lam, kappa, lam1))
    print("  amplitude: {:.5f} K at t = {:g} s -> {:.6f} K at t = {:g} s "
          "({} steps)".format((mean0 - C0)/c1, t0, A_meas, t1, n))
    print("  analytic decay gives {:.6f} K (error {:.3%})".format(A_ana, err))

    if err > TOL:
        print("FAIL: the sine mode does not decay at the anisotropic "
              "conductivity's rate (coupling off or changed?)")
        sys.exit(1)

    print("PASS: the anisotropic conductivity sets the conduction decay rate")


if __name__ == "__main__":
    main()
