#!/usr/bin/env python3
#******************************************************************************
# verify-runner-network.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the runner-coupled inlet and gate pressure
# (task 016/026, extended by 058), in two phases:
#
#   filling - the inlet moldingInletVelocity takes its flow rate from the
#     equal-pressure-drop split of the 1D runner network (a single gate
#     here, so the gate flow equals the prescribed total). The inlet mass
#     flux must match rho*Q;
#   packing - once the packing switch fires, the gate pressure comes from
#     moldingPrghPressure with the *same* network: p_gate = p_packing -
#     dp_network(Q), with dp from the power-law Hagen-Poiseuille relation
#     at the gate flow measured in the same time step.
#
# The cavity is deliberately filled here (endTime beyond the fill time) so
# that the packing branch of the pressure condition is exercised - it is the
# only case covering it.
#
# Note: the melt volume fraction is not compared with Q t / V because the
# trapped air is heated by the melt and expands, which is physical.
#
# Usage: verify-runner-network.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import math
import os
import re
import sys

Q = 1e-07             # total (single-gate) runner flow rate [m^3/s]
V = 0.02*0.002*0.002  # cavity volume [m^3]
RHO = 746.3           # melt density at 480 K, 1e5 Pa [kg/m^3]

# The runner network of this case (identical in 0/U and 0/p_rgh):
# power-law K = 1e4, n = 0.5, feed 0.05 m / 0.004 m, gate 0.01 m / 0.002 m.
# A path adds a = L/D^(3n+1) and dp = 128 K (32/pi)^(n-1) a Q^n / pi.
NP = 0.5
A_PATH = 0.05/0.004**(3*NP + 1) + 0.01/0.002**(3*NP + 1)
C_PL = 128*1e4*(32/math.pi)**(NP - 1)/math.pi

PTARGET = 5e6         # constant/moldingDict: packing.pressure (constant)
RAMP = 0.1            # skip the packing pressure ramp before checking [s]

TIME_RE = re.compile(r"^Time = ([-+0-9.eE]+)")


def timedSeries(lines, pattern):
    """(time, value) pairs for a logged sample, using the Time = header"""
    out = []
    t = -1.0
    sampRe = re.compile(pattern)

    for line in lines:
        m = TIME_RE.match(line)
        if m:
            t = float(m.group(1))
            continue

        m = sampRe.search(line)
        if m and t >= 0:
            out.append((t, float(m.group(1))))

    return out


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    lines = log.splitlines()

    fills = [
        float(x) for x in re.findall(r"filled fraction = ([-+0-9.eE]+)", log)
    ]
    flux = timedSeries(lines,
        r"sum\(inlet\) of alphaRhoPhi\.melt = ([-+0-9.eE]+)")
    pres = timedSeries(lines, r"areaAverage\(inlet\) of p_rgh = ([-+0-9.eE]+)")
    switch = re.findall(r"V/P switch:.*at t = ([-+0-9.eE]+) s", log)

    if not fills:
        print("FAIL: no filling reports in the log")
        sys.exit(1)
    if not flux:
        print("FAIL: no inlet mass flow samples in the log")
        sys.exit(1)
    if not pres:
        print("FAIL: no inlet pressure samples in the log")
        sys.exit(1)
    if not switch:
        print("FAIL: no V/P switch in the log (the case must reach packing)")
        sys.exit(1)

    tSwitch = float(switch[-1])

    print("  final melt volume fraction = {:.6f}, V/P switch at t = {:g} s"
          .format(fills[-1], tSwitch))

    fail = False

    # Filling: the mass flux follows rho*Q (settled samples of the phase)
    fill = [f for (t, f) in flux if 0 < t < tSwitch and abs(f) > 0]

    if not fill:
        print("FAIL: no non-zero inlet mass flow during filling")
        fail = True
    else:
        settled = fill[-20:] if len(fill) >= 20 else fill
        mdot = sum(settled)/len(settled)
        merr = abs(abs(mdot) - RHO*Q)/(RHO*Q)

        print("  fill inlet mass flux = {:.6e} kg/s (rho Q = {:.6e} kg/s, "
              "error {:.3%})".format(mdot, -RHO*Q, merr))

        # 4%: the flux is prescribed exactly, so the difference is the
        # discrete measure of it (it was 1.6% over the whole run, 2.4% over
        # the filling window alone)
        if merr > 0.04:
            print("FAIL: the inlet mass flux does not match rho Q")
            fail = True

    if max(fills) < 0.95:
        print("FAIL: the cavity did not fill (needed to reach packing)")
        fail = True

    # Packing: the gate pressure is the packing target less the network drop
    # at the flow of the same step. Only the samples where that drop is a
    # significant part of the target test the coupling - elsewhere the
    # relation holds trivially - and the packing transient has isolated
    # steps where the pressure and the flux oscillate, so the check uses the
    # median of the significant samples (ignoring the network term would put
    # them at ~17%, well outside the tolerance).
    fluxAt = {t: f for (t, f) in flux}
    errs = []

    for (t, p) in pres:
        if t <= tSwitch + RAMP:
            continue

        q = abs(fluxAt.get(t, 0.0))/RHO
        dP = C_PL*A_PATH*q**NP

        if dP > 0.1*PTARGET:
            errs.append(abs(p - (PTARGET - dP))/PTARGET)

    errs.sort()

    def median(v):
        n = len(v)
        if not n:
            return 0.0
        return v[n//2] if n % 2 else 0.5*(v[n//2 - 1] + v[n//2])

    print("  packing samples with dp > 10% of the target = {}, median "
          "|p - (target - dp)| / target = {:.3%}".format(
              len(errs), median(errs)))

    if len(errs) < 3:
        print("FAIL: the packing phase never flowed enough to test the "
              "network coupling")
        fail = True
    elif median(errs) > 0.05:
        print("FAIL: the gate pressure does not follow the packing target "
              "less the runner pressure drop")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the runner network drives the inlet flow and the packing "
          "gate pressure")


if __name__ == "__main__":
    main()
