#!/usr/bin/env python3
#******************************************************************************
# verify-crystallinity-viscosity.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the CrossWlf crystallinity-viscosity coupling
# (task 043, gap G5): with the `crystallinity` sub-dictionary present the
# viscosity is multiplied by (1 - chi/chiInfinity)^(-exponent), so the
# crystallised melt must become far more viscous than the uncoupled case.
# Ablation calibration (crystallizationAdvection geometry, chi -> 1):
#
#   crystallinity {chiInfinity 1; exponent 2;} -> max(eta) = 5.6e10 Pa s
#   uncoupled                                  -> max(eta) = 6.36 Pa s
#   max|U|: 2.7e-3 m/s coupled vs 2.7e-2 m/s uncoupled
#
# Thresholds sit between the two with several orders of magnitude (eta) or
# ~4x (velocity) of margin.
#
# Usage: verify-crystallinity-viscosity.py <caseDir>   (exits non-zero)
#******************************************************************************

import os
import re
import sys

ETA_MIN = 1e3
U_MAX = 1e-2


def read_scalar(case_dir, time, name):
    with open(os.path.join(case_dir, time, name), errors="replace") as f:
        txt = f.read()
    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n\s*\d+\s*\n"
        r"\((.*?)\)\s*;", txt, re.S)
    if m is not None:
        return [float(x) for x in m.group(1).split()]
    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", txt)
    return [float(m.group(1))] if m is not None else None


def read_vector_max(case_dir, time, name):
    with open(os.path.join(case_dir, time, name), errors="replace") as f:
        txt = f.read()
    m = re.search(
        r"internalField\s+nonuniform\s+List<vector>\s*\n\s*\d+\s*\n"
        r"\((.*?)\)\s*;", txt, re.S)
    if m is None:
        return None
    return max((float(a)**2 + float(b)**2 + float(c)**2)**0.5
               for a, b, c in re.findall(
                   r"\(([-+0-9.eE]+) ([-+0-9.eE]+) ([-+0-9.eE]+)\)",
                   m.group(1)))


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)
    if re.search(r"\bnan\b", log, re.I):
        print("FAIL: the log contains NaN values")
        sys.exit(1)

    times = sorted((d for d in os.listdir(case_dir)
                    if re.match(r"^[0-9]+(\.[0-9]+)?$", d)
                    and float(d) > 0), key=float)
    if not times:
        print("FAIL: no written time directories")
        sys.exit(1)
    t = times[-1]

    if not os.path.isfile(os.path.join(case_dir, "constant", "momentumTransport")):
        print("FAIL: missing constant/momentumTransport")
        sys.exit(1)

    with open(os.path.join(case_dir, "constant", "momentumTransport"),
              errors="replace") as f:
        mt = f.read()
    if "crystallinity" not in mt:
        print("FAIL: the case must enable the crystallinity coupling")
        sys.exit(1)

    chi = read_scalar(case_dir, t, "chi")
    if chi:
        print("  chi: max = {:.4f}".format(max(chi)))

    eta = read_scalar(case_dir, t, "strainRateViscosityModel:nu")
    if eta is None:
        print("FAIL: cannot read the viscosity field "
              "(strainRateViscosityModel:nu)")
        sys.exit(1)
    print("  viscosity: max = {:.4e} Pa s (coupling threshold {:.0e})"
          .format(max(eta), ETA_MIN))
    if max(eta) < ETA_MIN:
        print("FAIL: the crystallinity-coupled viscosity did not rise")
        sys.exit(1)

    umax = read_vector_max(case_dir, t, "U")
    if umax is not None:
        print("  max|U| = {:.4e} m/s (coupled ceiling {:g})"
              .format(umax, U_MAX))
        if umax > U_MAX:
            print("FAIL: the flow was not slowed by the coupled viscosity")
            sys.exit(1)

    print("PASS: the crystallinity coupling raises the viscosity and slows "
          "the flow")


if __name__ == "__main__":
    main()
