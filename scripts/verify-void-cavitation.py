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
# Acceptance verification for the field-coupled void model (task 033):
# the sealed melt cools in a cavity that is closed off from the gate; the
# upstream compressible cavitation model (VoFCavitation, SchnerrSauer)
# transfers melt into the void phase once the pressure falls to pSat,
# pinning the pressure and opening the void volume. The verifier checks
# that
#
#   - the run is stable (End marker, no NaN);
#   - the sealed melt pressure is pinned at the saturation pressure;
#   - a distributed void has opened (0.01 < mean(1-alpha) < 0.10);
#   - the melt has cooled below the inlet temperature.
#
# Usage: verify-void-cavitation.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

PV_TOL = 0.05


def latest_time(case_dir):
    times = sorted(
        (d for d in os.listdir(case_dir)
         if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0),
        key=float,
    )
    return times[-1] if times else None


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

    time = latest_time(case_dir)
    if time is None:
        print("FAIL: no written time directories")
        sys.exit(1)

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
    if not 0.01 < void < 0.10:
        print("FAIL: the void volume is not in the expected range")
        fail = True

    if T:
        print("  melt temperature: {:.1f} .. {:.1f} K".format(min(T), max(T)))
        if max(T) > 500:
            print("FAIL: the sealed melt did not cool")
            fail = True

    if fail:
        sys.exit(1)

    print("PASS: the sealed melt cavitates at pSat and opens the void "
          "volume")


if __name__ == "__main__":
    main()
