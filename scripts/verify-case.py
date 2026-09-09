#!/usr/bin/env python3
#******************************************************************************
# verify-case.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************

#******************************************************************************
# Acceptance verification for the moldingFoam contract case.
#
# 1. Mass conservation: the polymer mass accumulated in the cavity,
#    M(t) - M(0), must match the time-integrated polymer fluxes through
#    the boundaries (inlet inflow + vent outflow) to a relative error
#    below 1e-3.
# 2. Model evidence: the run log must show that the CrossWlf
#    generalised Newtonian viscosity model and the Tait equation of state
#    were selected, with their coefficients.
#
# Works for serial runs and for parallel runs (per-processor function
# object data are summed for each time).
#
# Usage: verify-case.py <caseDir>       (exits non-zero on failure)
#******************************************************************************

import glob
import math
import os
import re
import sys


def read_fo_series(case_dir, fo_name):
    """Return {time: value} summed over all matching function-object
    data files (serial and per-processor)."""
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


def trapezoid(series):
    times = sorted(series)
    area = 0.0
    for a, b in zip(times, times[1:]):
        area += 0.5*(b - a)*(series[a] + series[b])
    return area


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    log_path = os.path.join(case_dir, "log.foamRun")

    failures = []

    # ------------------------------------------------------------------
    # Model evidence from the run log
    # ------------------------------------------------------------------
    with open(log_path, errors="replace") as handle:
        log = handle.read()

    evidence = [
        ("solver module selected",
         "Selecting solver moldingFoam" in log),
        ("CrossWlf viscosity model selected",
         "Selecting generalised Newtonian model CrossWlf" in log),
        ("CrossWlf coefficients reported",
         "CrossWlf: n = " in log),
        ("Tait equation of state selected",
         "equationOfState Tait;" in log),
        ("V/P switch triggered",
         "V/P switch: filled fraction" in log),
        ("packing pressure followed the curve",
         "packing: t = " in log),
        ("ejection criterion reached",
         "ejection criterion met" in log),
    ]
    for name, ok in evidence:
        print(("PASS" if ok else "FAIL") + ": " + name)
        if not ok:
            failures.append(name)

    # ------------------------------------------------------------------
    # Mass conservation
    # ------------------------------------------------------------------
    inlet = read_fo_series(case_dir, "inletMassFlow")
    vent = read_fo_series(case_dir, "ventMassFlow")
    mass = read_fo_series(case_dir, "polymerMass")

    if inlet is None or vent is None or mass is None:
        print("FAIL: function object data missing (inletMassFlow, "
              "ventMassFlow or polymerMass)")
        sys.exit(1)

    # Baselines by time, not by value: the earliest sample is the start of
    # the conservation window even if a numerical dip went below it
    m_start = mass[min(mass)]
    m_end = mass[max(mass)]
    d_m = m_end - m_start

    # Polymer fluxes are outward-positive: inflow at the inlet is
    # negative, so the accumulated mass equals minus the integrated
    # total boundary flux
    flux_integral = trapezoid(
        {t: inlet.get(t, 0.0) + vent.get(t, 0.0) for t in mass})

    if d_m < 1e-6:
        # A vacuous pass is not acceptance: essentially no polymer was
        # injected, so the conservation ratio would be meaningless
        print("FAIL: essentially no polymer injected "
              "(accumulated mass {:.3e} kg)".format(d_m))
        failures.append("polymer injection")

    error = abs(d_m + flux_integral)/max(abs(d_m), 1e-12)

    print("Polymer mass in cavity: start = {:.6f} kg, "
          "end = {:.6f} kg".format(m_start, m_end))
    print("Integrated boundary polymer flux = {:.6f} kg".format(
        flux_integral))
    print("Mass conservation relative error = {:.3e}".format(error))

    if error < 1e-3:
        print("PASS: mass conservation relative error < 1e-3")
    else:
        print("FAIL: mass conservation relative error >= 1e-3")
        failures.append("mass conservation")

    if failures:
        print("\n{} acceptance check(s) FAILED".format(len(failures)))
        sys.exit(1)

    print("\nAll acceptance checks passed")
    sys.exit(0)


if __name__ == "__main__":
    main()
