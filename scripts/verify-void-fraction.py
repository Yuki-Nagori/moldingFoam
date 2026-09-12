#!/usr/bin/env python3
#******************************************************************************
# verify-void-fraction.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the sealed-melt void indicator (task 018a):
# a cavity sealed at t ~ 0 and cooled to the mould temperature must
# develop a positive, near-uniform and bounded voidFraction, and the
# final written field must agree with the logged maximum. The case also
# guards the sealed cooling against the rigid (acoustic) blow-up of the
# near-incompressible melt: the void fraction and the temperature must
# stay stable over the whole run.
#
# Usage: verify-void-fraction.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys
import math


def read_scalar_field(path):
    with open(path, errors="replace") as f:
        txt = f.read()

    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n?\s*(\d+)\s*\((.*?)\)\s*;",
        txt,
        re.S,
    )
    if m is not None:
        return [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]

    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", txt)
    if m is not None:
        return [float(m.group(1))]

    return None


def time_dirs(case_dir):
    return sorted(
        (d for d in os.listdir(case_dir)
         if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0),
        key=float,
    )


def read_tait(case_dir):
    path = os.path.join(case_dir, "constant", "physicalProperties.melt")

    with open(path, errors="replace") as f:
        txt = f.read()

    def coeff(name):
        m = re.search(name + r"\s+([-+0-9.eE]+)", txt)
        return float(m.group(1)) if m else None

    return dict(
        b1m=coeff("b1m"),
        b2m=coeff("b2m"),
        b3=coeff(r"b3\s"),
        b4=coeff("b4"),
        b5=coeff("b5"),
        C=coeff(r"C\s"),
    )


def rho_tait_melt(p, T, c):
    B = c["b3"]*math.exp(-c["b4"]*T)
    v0 = c["b1m"] + c["b2m"]*(T - c["b5"])
    return 1.0/(v0*(1 - c["C"]*math.log(1 + p/B)))


def p_tait_melt_iso(rho, T, c):
    # Invert the melt-branch Tait EOS for the isochoric pressure
    B = c["b3"]*math.exp(-c["b4"]*T)
    v0 = c["b1m"] + c["b2m"]*(T - c["b5"])
    return B*(math.exp((1 - 1/(rho*v0))/c["C"]) - 1)


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    m = re.search(r"gate sealed at t = ([-+0-9.eE]+) s", log)
    if m is None:
        print("FAIL: the gate never sealed; the melt is not a fixed mass")
        sys.exit(1)

    seal_time = float(m.group(1))
    if seal_time > 1e-3:
        print("FAIL: the gate did not seal at the start of the run "
              "(t = {:.3e} s)".format(seal_time))
        sys.exit(1)

    V = [float(x) for x in re.findall(r"void fraction: max = ([-+0-9.eE]+)", log)]
    if len(V) < 2:
        print("FAIL: not enough void-fraction samples in the log")
        sys.exit(1)

    print("  sealed at t = {:.3e} s".format(seal_time))
    print("  max(voidFraction): first = {:.6f}, last = {:.6f}".format(
        V[0], V[-1]))

    fail = False
    if V[-1] < 0.01:
        print("FAIL: no voids formed after the gate seal")
        fail = True
    if min(V) < 0 or max(V) >= 1:
        print("FAIL: the void fraction left the physical range [0, 1)")
        fail = True
    if max(V) - min(V) > 0.05:
        print("FAIL: the void fraction drifted; the sealed cooling is unstable")
        fail = True

    # Discrete melt-mass budget (task 031/033 DoD): the sealed cooling
    # must conserve mass to < 1e-3 without any mass fixer
    with open(os.path.join(case_dir, "constant", "moldingDict"),
              errors="replace") as f:
        mdict = f.read()

    if re.search(r"massFix\s+true|massFixGlobal\s+true", mdict):
        print("FAIL: the case enables a mass fixer; the budget check "
              "requires the un-fixed solver")
        fail = True

    budgets = [
        (float(a), float(b), float(c))
        for a, b, c in re.findall(
            r"mass budget: m = ([-+0-9.eE]+) kg, accumulated boundary "
            r"flux = ([-+0-9.eE]+) kg, residual = ([-+0-9.eE]+) kg",
            log,
        )
    ]
    if not budgets:
        print("FAIL: no mass-budget samples in the log")
        fail = True
    else:
        res = max(abs(b[2]) for b in budgets)
        print("  sealed mass budget: {} samples, max |residual| = {:.3e} kg"
              .format(len(budgets), res))
        if res > 1e-3:
            print("FAIL: the sealed melt mass is not conserved (< 1e-3)")
            fail = True

    vbudgets = [
        (int(a), float(b), float(c), float(d))
        for a, b, c, d in re.findall(
            r"void budget: cells = (\d+), V_void = ([-+0-9.eE]+) m\^3, "
            r"m_melt = ([-+0-9.eE]+) kg, tension = ([-+0-9.eE]+) Pa",
            log,
        )
    ]
    if not vbudgets:
        print("FAIL: no void-budget samples in the log (tensionLimit)")
        fail = True
    else:
        nv, vv, mm, tn = vbudgets[-1]
        print("  void budget: cells = {}, V_void = {:.3e} m^3, "
              "m_melt = {:.6e} kg, tension = {:.3e} Pa"
              .format(nv, vv, mm, tn))
        if vv <= 0 or mm <= 0:
            print("FAIL: the void budget is empty while voids have formed")
            fail = True
        elif abs(mm - vbudgets[0][2]) > 1e-3*vbudgets[0][2]:
            print("FAIL: the budgeted melt mass drifted by more than 1e-3")
            fail = True

    times = time_dirs(case_dir)
    if not times:
        print("FAIL: no written time directories")
        sys.exit(1)

    field_path = os.path.join(case_dir, times[-1], "voidFraction")
    if not os.path.exists(field_path):
        print("FAIL: no written voidFraction field")
        sys.exit(1)

    vals = read_scalar_field(field_path)
    if not vals:
        print("FAIL: cannot read the voidFraction field")
        sys.exit(1)

    fmax = max(vals)
    fmin = min(vals)
    print("  written field: max = {:.6f}, min = {:.6f}".format(fmax, fmin))

    if fmin < 0 or fmax >= 1:
        print("FAIL: the written field left the physical range")
        fail = True
    if fmin < 0.5*fmax:
        print("FAIL: the void fraction is not near-uniform over the part")
        fail = True
    if abs(fmax - max(V)) > 0.02 + 0.1*abs(max(V)):
        print("FAIL: the written field disagrees with the log")
        fail = True

    # Exact PVT consistency in the melt cells: the sealed density is the
    # current melt density (fixed mass and volume) and the indicator must
    # equal 1 - rho/rho_tait(pv, T) to round-off
    try:
        with open(os.path.join(case_dir, "constant", "moldingDict"),
                  errors="replace") as f:
            mdict = f.read()
        m = re.search(r"voidPressure\s+([-+0-9.eE]+)", mdict)
        pv = float(m.group(1)) if m else 0.0

        tdir = os.path.join(case_dir, times[-1])
        alpha = read_scalar_field(os.path.join(tdir, "alpha.melt"))
        Tmelt = read_scalar_field(os.path.join(tdir, "T.melt"))
        rho = read_scalar_field(os.path.join(tdir, "rho"))
        vfield = read_scalar_field(os.path.join(tdir, "voidFraction"))
        tait = read_tait(case_dir)

        if None in tait.values() or not all((alpha, Tmelt, rho, vfield)):
            raise ValueError("missing Tait coefficients or fields")

        if min(Tmelt) <= tait["b5"] + 1:
            raise ValueError("a melt cell is inside the Tait transition band")

        n = 0
        dmax = 0.0
        idx = []
        for i in range(len(alpha)):
            if alpha[i] <= 0.9999:
                continue

            expected = max(0.0, 1 - rho[i]/rho_tait_melt(pv, Tmelt[i], tait))
            dmax = max(dmax, abs(expected - vfield[i]))
            idx.append(i)
            n += 1

        if n == 0:
            raise ValueError("no melt cells to check")

        print("  PVT check: {} melt cells, max |voidFraction - 1 + rho/rho_tait|"
              " = {:.3e}".format(n, dmax))

        if dmax > 1e-4:
            print("FAIL: the void fraction is not PVT-consistent")
            fail = True

        # The sealed melt pressure must have fallen to/below the
        # cavitation pressure (otherwise no void opens) and must follow
        # the isochoric PVT path
        pfield = read_scalar_field(os.path.join(tdir, "p"))
        if not pfield:
            raise ValueError("cannot read the pressure field")

        pm = [pfield[i] for i in idx]
        piso = [p_tait_melt_iso(rho[i], Tmelt[i], tait) for i in idx]
        dp = max(abs(piso[k] - pm[k])/max(abs(piso[k]), 1e5)
                 for k in range(len(idx)))

        print("  pressure check: solver {:.4e}..{:.4e} Pa, PVT {:.4e}.."
              "{:.4e} Pa, max rel. difference = {:.2%}".format(
                  min(pm), max(pm), min(piso), max(piso), dp))

        if max(pm) > pv:
            print("FAIL: the sealed melt pressure stayed above the "
                  "cavitation pressure")
            fail = True
        if dp > 0.1:
            print("FAIL: the pressure does not follow the isochoric PVT path")
            fail = True
    except Exception as e:
        print("FAIL: the PVT consistency check could not run: {}".format(e))
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the sealed melt develops a stable, near-uniform void fraction")


if __name__ == "__main__":
    main()
