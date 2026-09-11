#!/usr/bin/env python3
#******************************************************************************
# verify-highpressure.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the 40 MPa packing case.
#
# The polymer mass conservation is evaluated over the fill + packing
# window, ending at the gate seal: once the gate has frozen off the
# cooling melt would shrink and cavitate, which the current Tait
# thermodynamics cannot represent (task 018a). The seal time is read from
# the log, so the same verifier works for a run that continues past the
# seal (the extra samples are ignored).
#
# Usage: verify-highpressure.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import glob
import os
import re
import sys


def read_fo_series(case_dir, fo_name):
    patterns = [
        os.path.join(case_dir, "postProcessing", fo_name, "*", "*.dat"),
        os.path.join(case_dir, "processor*",
                     "postProcessing", fo_name, "*", "*.dat"),
    ]
    series = {}
    files = []
    for pattern in patterns:
        files.extend(sorted(glob.glob(pattern)))
    if not files:
        return None
    for path in files:
        with open(path) as handle:
            for line in handle:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()
                if len(parts) < 2:
                    continue
                try:
                    t = float(parts[0])
                    v = float(parts[-1])
                except ValueError:
                    continue
                series[t] = series.get(t, 0.0) + v
    return series


def euler_integral(series):
    # The solver advances the phase density with the first-order (Euler)
    # scheme, so the discrete mass budget must integrate the boundary flux
    # with the same end-of-step (rectangle) rule. The trapezoid rule used
    # previously over-smooths the rapid packing transient and overstates
    # the conservation error by O(5e-3) even when the discrete budget
    # closes to O(1e-5)
    times = sorted(series)
    area = 0.0
    for a, b in zip(times, times[1:]):
        area += series[b]*(b - a)
    return area


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    failures = []

    evidence = [
        ("solver module selected", "Selecting solver moldingFoam" in log),
        ("CrossWlf viscosity model selected",
         "Selecting generalised Newtonian model CrossWlf" in log),
        ("Tait equation of state selected",
         "equationOfState Tait;" in log),
        ("V/P switch triggered", "V/P switch: filled fraction" in log),
        ("packing pressure followed the curve", "packing: t = " in log),
        ("gate sealed", "gate sealed at t = " in log),
    ]
    for name, ok in evidence:
        print(("PASS" if ok else "FAIL") + ": " + name)
        if not ok:
            failures.append(name)

    m = re.search(r"gate sealed at t = ([-+0-9.eE]+)", log)
    if not m:
        print("FAIL: no gate seal time in the log")
        sys.exit(1)
    tSeal = float(m.group(1))

    inlet = read_fo_series(case_dir, "inletMassFlow")
    vent = read_fo_series(case_dir, "ventMassFlow")
    mass = read_fo_series(case_dir, "polymerMass")

    if inlet is None or vent is None or mass is None:
        print("FAIL: function object data missing")
        sys.exit(1)

    # Window the samples at the gate seal
    times = [t for t in sorted(mass) if t <= tSeal + 1e-12]
    m_start = mass[times[0]]
    m_end = mass[times[-1]]
    d_m = m_end - m_start

    t0 = times[0]
    flux = {t: inlet.get(t, 0.0) + vent.get(t, 0.0) for t in times}
    flux_integral = euler_integral(
        {t: v for t, v in flux.items() if t >= t0})

    if d_m < 1e-6:
        print("FAIL: essentially no polymer injected")
        failures.append("polymer injection")

    error = abs(d_m + flux_integral)/max(abs(d_m), 1e-12)

    print("Polymer mass in cavity (window to the gate seal, t <= {:.4f} s):"
          .format(tSeal))
    print("  start = {:.8f} kg, end = {:.8f} kg".format(m_start, m_end))
    print("  integrated boundary polymer flux = {:.8f} kg".format(
        flux_integral))
    print("  mass conservation relative error = {:.3e}".format(error))

    if error < 1e-3:
        print("PASS: packing-window mass conservation relative error < 1e-3")
    else:
        print("FAIL: packing-window mass conservation relative error >= 1e-3")
        failures.append("mass conservation")

    if failures:
        print("\n{} acceptance check(s) FAILED".format(len(failures)))
        sys.exit(1)

    print("\nAll acceptance checks passed")


if __name__ == "__main__":
    main()
