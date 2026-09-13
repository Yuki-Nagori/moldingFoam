#!/usr/bin/env python3
#******************************************************************************
# cavitation-closure-matrix.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Cavitation calibration matrix for the closure-constrained void model
# (task 039). Copies the voidCavitation case per (Cv, Cc) pair, swaps the
# fvModels entry for moldingVoidClosure, runs it and reports the stability,
# the final void fraction, the sealed-mass drift against the initial EOS
# state and the pressure pin. With the closure ceiling the void must be
# insensitive to the coefficients; the matrix documents that, and the
# upstream (unconstrained) model destroys melt mass at the same settings.
#
# Usage (inside the OpenFOAM environment):
#   cavitation-closure-matrix.py <caseDir> <workDir> [Cv...] [-- Cc...]
#   e.g. cavitation-closure-matrix.py tests/cases/voidCavitation /tmp/voidm
#******************************************************************************

import math
import os
import re
import shutil
import subprocess
import sys

R_UNIVERSAL = 8.314462618


def coef(txt, name):
    return float(re.search(r"\b" + name + r"\s+([-+0-9.eE]+)\s*;", txt).group(1))


def read_field(case, time, name):
    with open(os.path.join(case, time, name), errors="replace") as f:
        txt = f.read()
    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n\s*\d+\s*\n"
        r"\((.*?)\)\s*;", txt, re.S)
    if m:
        return [float(x) for x in m.group(1).split()]
    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", txt)
    return [float(m.group(1))] if m else None


def run_case(base, work, Cv, Cc, band):
    case = os.path.join(work, "void_%s_%s" % (Cv, Cc))
    shutil.rmtree(case, ignore_errors=True)
    shutil.copytree(base, case)

    for d in os.listdir(case):
        if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0:
            shutil.rmtree(os.path.join(case, d))

    fvm = os.path.join(case, "constant", "fvModels")
    txt = open(fvm).read()
    txt = re.sub(r"Cv\s+[-+0-9.eE]+;", "Cv              %g;" % Cv, txt)
    txt = re.sub(r"Cc\s+[-+0-9.eE]+;", "Cc              %g;" % Cc, txt)
    txt = re.sub(r"band\s+[-+0-9.eE]+;", "band            %g;" % band, txt)
    open(fvm, "w").write(txt)

    subprocess.run(["blockMesh"], cwd=case, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    with open(os.path.join(case, "log.foamRun"), "w") as lg:
        subprocess.run(["foamRun"], cwd=case, stdout=lg,
                       stderr=subprocess.STDOUT)

    with open(os.path.join(case, "log.foamRun"), errors="replace") as f:
        log = f.read()
    stable = ("\nEnd" in log or log.rstrip().endswith("End")) \
        and not re.search(r"\bnan\b", log, re.I)

    times = sorted(
        (d for d in os.listdir(case)
         if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0), key=float)
    if not times:
        return None
    t0, t1 = times[0], times[-1]

    melt = open(os.path.join(case, "constant", "physicalProperties.melt"),
                errors="ignore").read()
    b1m, b2m, b3, b4, b5, C = (coef(melt, k)
                               for k in ("b1m", "b2m", "b3", "b4", "b5", "C"))
    air = open(os.path.join(case, "constant", "physicalProperties.air"),
               errors="ignore").read()
    molWeight = coef(air, "molWeight")

    def rhoL(p, T):
        return 1.0/((b1m + b2m*(T - b5))
                    * (1 - C*math.log(1 + p/(b3*math.exp(-b4*T)))))

    def mass_per_volume(t):
        a = read_field(case, t, "alpha.melt")
        p = read_field(case, t, "p")
        T = read_field(case, t, "T.melt")
        n = min(len(a), len(p), len(T))
        return sum(a[i]*rhoL(max(p[i], 1.0), T[i])
                   + (1 - a[i])*p[i]*molWeight/(R_UNIVERSAL*T[i])
                   for i in range(n))/n

    T0 = read_field(case, "0", "T")[0]
    p0 = read_field(case, "0", "p")[0]
    mRef = rhoL(max(p0, 1.0), T0)

    a1 = read_field(case, t1, "alpha.melt")
    p1 = read_field(case, t1, "p")
    void = 1 - sum(a1)/len(a1)

    return {
        "Cv": Cv, "Cc": Cc, "stable": stable, "void": void,
        "onset": abs(mass_per_volume(t0) - mRef)/mRef,
        "drift": abs(mass_per_volume(t1) - mass_per_volume(t0))
                 / mass_per_volume(t0),
        "pMin": min(p1),
    }


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    base, work = args[0], args[1]
    Cvs = [float(x) for x in args[2:]] or [0.05, 0.1, 0.2, 0.3]
    base = os.path.abspath(base)
    os.makedirs(work, exist_ok=True)

    print("Cv\tCc\tstable\tvoid\tonset_mass\tfinal_drift\tp_min")
    for Cv in Cvs:
        for Cc in (1, 10, 100):
            r = run_case(base, work, Cv, Cc, 0.2)
            if r is None:
                print("%g\t%g\tNO-FIELDS" % (Cv, Cc))
                continue
            print("%g\t%g\t%s\t%.5f\t%.3f%%\t%.3f%%\t%.1f" % (
                r["Cv"], r["Cc"], "yes" if r["stable"] else "NO",
                r["void"], 100*r["onset"], 100*r["drift"], r["pMin"]))


if __name__ == "__main__":
    main()
