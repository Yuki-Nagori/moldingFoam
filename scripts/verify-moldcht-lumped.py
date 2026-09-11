#!/usr/bin/env python3
#******************************************************************************
# verify-moldcht-lumped.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the 002 (lumped mould) limit: in a thin
# two-region plate the melt and the mould equilibrate to the capacity
# weighted mean temperature, exactly as the lumped-parameter
# moldingMoldTemperature model does. The check is the equilibrium energy
# consistency between the multi-region CHT and the 0D lumped model.
#
# Usage: verify-moldcht-lumped.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

# Case parameters (must match validation/moldCHT-lumped)
RHO_F = 800.3          # melt density from the constant-density Tait
CV_F = 2400.0          # melt Cv
VF = 0.02*0.0002*0.02  # cavity volume
RHOM = 7800.0
CVM = 500.0
VM = 0.02*0.0005*0.02  # mould volume
T0F = 480.0
T0M = 353.0

CF = RHO_F*CV_F*VF
CM = RHOM*CVM*VM
TEQ = (CF*T0F + CM*T0M)/(CF + CM)


def read_internal(path):
    with open(path) as handle:
        text = handle.read()
    m = re.search(r"internalField\s+nonuniform\s+List<scalar>\s*\n?\s*"
                  r"(\d+)\s*\((.*?)\)\s*;", text, re.S)
    if not m:
        return None
    return [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    times = [d for d in os.listdir(case_dir)
             if re.match(r"^[0-9.]+$", d) and float(d) > 0
             and os.path.isfile(os.path.join(case_dir, d, "mold", "T"))]
    if not times:
        print("FAIL: no written mould temperature")
        sys.exit(1)

    t_last = max(times, key=float)
    tdir = os.path.join(case_dir, t_last)

    Tf = read_internal(os.path.join(tdir, "cavity", "T"))
    Tm = read_internal(os.path.join(tdir, "mold", "T"))
    if Tf is None or Tm is None:
        print("FAIL: cannot read the temperature fields")
        sys.exit(1)

    TfMean = sum(Tf)/len(Tf)
    TmMean = sum(Tm)/len(Tm)

    err = max(abs(TfMean - TEQ), abs(TmMean - TEQ))/TEQ

    print("  lumped equilibrium T = {:.4f} K".format(TEQ))
    print("  cavity mean = {:.4f} K, mould mean = {:.4f} K "
          "(max relative error {:.3e})".format(TfMean, TmMean, err))

    if err > 1e-3:
        print("FAIL: the multi-region CHT does not match the lumped "
              "equilibrium within 1e-3")
        sys.exit(1)

    print("PASS: the multi-region CHT reaches the 002 lumped equilibrium")


if __name__ == "__main__":
    main()
