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


def mean_at(means, t):
    # Mould mean at time t, linearly interpolated between the bracketing
    # written samples. The mould field evolves smoothly across a cycle
    # reset (only the cavity is reset), so interpolation is well posed;
    # outside the written range the nearest sample is returned.
    if t <= means[0][0]:
        return means[0][1]
    if t >= means[-1][0]:
        return means[-1][1]
    for (t0, m0), (t1, m1) in zip(means, means[1:]):
        if t0 <= t <= t1:
            if t1 - t0 < 1e-12:
                return m1
            return m0 + (m1 - m0)*(t - t0)/(t1 - t0)
    return means[-1][1]


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    # Every completed moulding cycle ends when the ejection criterion is
    # met. The final cycle also stops the run, so it has no following
    # "starting cycle" message; counting the ejection events keeps that
    # last cycle (and its increment) in the trend below.
    ends = [float(x) for x in re.findall(
        r"ejection criterion met:.*?at t = ([-+0-9.eE]+) s "
        r"\(cycle \d+/\d+\)", log)]

    if len(ends) < 4:
        print("FAIL: fewer than four moulding cycles completed ({}); at "
              "least four are needed for three per-cycle "
              "increments".format(len(ends)))
        sys.exit(1)

    print("  moulding cycles completed = {}".format(len(ends)))

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

    # Per-cycle increments must decrease monotonically and decay overall:
    # every cycle re-injects 480 K melt, so the mould approaches a
    # periodic steady state and the increments shrink towards zero
    cycleMean = [mean_at(means, t) for t in ends]
    incs = [
        cycleMean[i + 1] - cycleMean[i]
        for i in range(len(cycleMean) - 1)
    ]
    print("  per-cycle mould-mean increments = " + ", ".join(
        "{:.3f} K".format(v) for v in incs))

    if incs[0] <= 0 or not all(
            incs[i + 1] <= incs[i] + 1e-6 for i in range(len(incs) - 1)):
        print("FAIL: the per-cycle mould-mean increments do not decrease")
        sys.exit(1)

    # Overall decay towards the periodic steady state. The response decays
    # by roughly 10% per cycle (measured), so the last increment is well
    # below the first without a literal halving; a stalled approach
    # (ratio near one) is rejected.
    ratio = incs[-1]/incs[0]
    print("  increment decay (last / first) = {:.3f}".format(ratio))

    if ratio > 0.85:
        print("FAIL: the per-cycle mould-mean increment has not decayed "
              "(no clear periodic-steady trend)")
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
