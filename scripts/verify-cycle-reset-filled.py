#!/usr/bin/env python3
#******************************************************************************
# verify-cycle-reset-filled.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Regression test for the cycle reset of a filled cavity (task 038
# follow-up): a cavity that starts empty fills with melt, then the
# ejection criterion releases the packing pressure and triggers the
# reset. At the reset BOTH phase fractions must return to their initial
# state - restoring only the melt fraction leaves a stale air fraction
# (~0 where the part was melt-filled) and the upstream mixture divides
# by rho = alpha1*rho1 + alpha2*rho2, which trips the floating-point
# exception trap on x86 builds (the local arm64 build does not trap, so
# this case asserts the logged min(rho) instead).
#
# Usage: verify-cycle-reset-filled.py <caseDir>   (exits non-zero on
# failure)
#******************************************************************************

import os
import re
import sys

RHO_MIN = 0.5


def time_dirs(case_dir):
    return sorted(
        (d for d in os.listdir(case_dir)
         if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0),
        key=float,
    )


def mean_alpha(case_dir, time):
    with open(os.path.join(case_dir, time, "alpha.melt"),
              errors="replace") as f:
        txt = f.read()

    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n\s*\d+\s*\n"
        r"\((.*?)\)\s*;",
        txt,
        re.S,
    )
    if m is None:
        return None

    vals = [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(1))]
    return sum(vals)/len(vals)


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

    m = re.search(
        r"cycle 1 complete; starting cycle 2/2 at t = ([-+0-9.eE]+) s", log
    )
    if m is None:
        print("FAIL: the cycle reset never happened")
        sys.exit(1)

    resetTime = float(m.group(1))

    if not re.search(r"ejection criterion met:.*\(cycle 2/2\)", log):
        print("FAIL: the second cycle never met the ejection criterion")
        fail = True

    # The consistency guard must report a physical mixture density at
    # every reset (this is the platform-independent proxy for the x86
    # division-by-zero trap)
    rhos = [
        float(x)
        for x in re.findall(r"cycle reset: min\(rho\) = ([-+0-9.eE]+) kg/m3",
                            log)
    ]
    if not rhos:
        print("FAIL: no cycle-reset density report in the log")
        fail = True
    else:
        print("  cycle reset min(rho) = " + ", ".join(
            "{:.6g}".format(r) for r in rhos) + " kg/m3")
        if min(rhos) <= RHO_MIN:
            print("FAIL: the reset left a non-physical mixture density "
                  "(stale phase fraction)")
            fail = True

    # The cavity must be cleared: the first write after the reset shows
    # the next cycle only starting to fill again
    times = [float(t) for t in time_dirs(case_dir) if float(t) > resetTime]
    if not times:
        print("FAIL: no written time after the reset")
        fail = True
    else:
        a = mean_alpha(case_dir, "{:g}".format(times[0]))
        if a is None:
            print("FAIL: cannot read the first post-reset alpha field")
            fail = True
        else:
            print("  first post-reset fill fraction = {:.4f}".format(a))
            if a > 0.20:
                print("FAIL: the cavity was not cleared by the reset")
                fail = True

    if fail:
        sys.exit(1)

    print("PASS: the filled-cavity cycle reset restores a physical state")


if __name__ == "__main__":
    main()
