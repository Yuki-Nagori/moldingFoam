#!/usr/bin/env python3
#******************************************************************************
# verify-moldcht-cycle.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the multi-cycle multi-region CHT case: the
# cavity is reset between cycles while the mould solid keeps its
# temperature, so the mould must warm across the cycles and the interface
# must stay continuous throughout.
#
# Usage: verify-moldcht-cycle.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys


def time_dir(t):
    return str(int(t)) if abs(t - round(t)) < 1e-9 else repr(t)


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

    cycles = len(re.findall(r"cycle \d+ complete", log))
    if cycles < 2:
        print("FAIL: fewer than two moulding cycles completed "
              "({})".format(cycles))
        sys.exit(1)

    print("  moulding cycles completed = {}".format(cycles))

    # Mould mean temperature across the cycles (the solid T field)
    times = sorted(
        float(d) for d in os.listdir(case_dir)
        if re.match(r"^[0-9.]+$", d) and float(d) > 0
        and os.path.isfile(os.path.join(case_dir, d, "mold", "T")))
    means = []
    for t in times:
        T = read_internal(os.path.join(case_dir, time_dir(t), "mold", "T"))
        if T:
            means.append((t, sum(T)/len(T)))

    if len(means) < 2:
        print("FAIL: not enough written mould temperatures")
        sys.exit(1)

    print("  mould mean T: first = {:.3f} K at t = {:g} s, "
          "last = {:.3f} K at t = {:g} s".format(
              means[0][1], means[0][0], means[-1][1], means[-1][0]))

    if means[-1][1] <= means[0][1]:
        print("FAIL: the mould did not warm across the cycles")
        sys.exit(1)

    # Per-cycle increments must decrease towards a periodic steady state
    ends = [float(x) for x in re.findall(
        r"cycle \d+ complete; starting cycle \d+/\d+ at t = "
        r"([-+0-9.eE]+)", log)]
    if len(ends) >= 3:
        cycleMean = []
        for t in ends:
            best = min(means, key=lambda m: abs(m[0] - t))
            cycleMean.append(best[1])

        incs = [
            cycleMean[i + 1] - cycleMean[i]
            for i in range(len(cycleMean) - 1)
        ]
        print("  per-cycle mould-mean increments = " + ", ".join(
            "{:.3f} K".format(v) for v in incs))

        decreasing = all(
            incs[i + 1] <= incs[i] + 1e-6 for i in range(len(incs) - 1))

        if not decreasing:
            print("FAIL: the per-cycle mould-mean increments do not "
                  "decrease")
            sys.exit(1)

        if len(incs) >= 2 and incs[-1] > 0.6*incs[0]:
            print("FAIL: the per-cycle mould-mean increment has not "
                  "halved (no clear periodic-steady trend)")
            sys.exit(1)

    # Interface continuity at the final time
    tlast = max(times)
    tdir = os.path.join(case_dir, time_dir(tlast))

    def patch_value(path, patch):
        with open(path) as handle:
            text = handle.read()
        m = re.search(r"\b" + patch + r"\b\s*\{(.*?)\n    \}", text, re.S)
        m = re.search(r"\bvalue\s+nonuniform\s+List<scalar>\s*\n?\s*"
                      r"(\d+)\s*\((.*?)\)\s*;", m.group(1), re.S)
        vals = [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]
        return sum(vals)/len(vals)

    ci = patch_value(os.path.join(tdir, "cavity", "T"), "cavity_to_mold")
    mi = patch_value(os.path.join(tdir, "mold", "T"), "mold_to_cavity")
    cont = abs(ci - mi)

    print("  interface continuity at t = {:g} s: {:.3e} K".format(
        tlast, cont))

    if cont > 1e-3:
        print("FAIL: interface temperature is not continuous")
        sys.exit(1)

    print("PASS: the mould warms across the cycles with a continuous "
          "interface")


if __name__ == "__main__":
    main()
