#!/usr/bin/env python3
#******************************************************************************
# fit-crystallization.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Fit the isothermal Nakamura/Avrami parameters (n, K) from a relative
# crystallinity history, e.g. a DSC isothermal crystallisation curve.
#
# The Avrami form chi(t) = 1 - exp(-(K t)^n) is linearised as
#   ln(-ln(1 - chi)) = n ln K + n ln t
# and n, K follow from an ordinary least-squares fit.
#
# Usage:
#   fit-crystallization.py <data.csv>   # lines "t chi"
#   fit-crystallization.py --selftest   # synthetic data round trip
#
# The selftest generates chi from known parameters, fits them back and
# exits non-zero if the recovery is worse than 1%.
#******************************************************************************

import math
import sys


def fit(samples):
    """Least-squares Avrami fit; returns (n, K, r2)."""
    pts = []
    for t, chi in samples:
        if t <= 0 or chi <= 0 or chi >= 1:
            continue
        pts.append((math.log(t), math.log(-math.log(1 - chi))))

    m = len(pts)
    if m < 2:
        raise RuntimeError("not enough valid (t, chi) points")

    sx = sum(x for x, _ in pts)
    sy = sum(y for _, y in pts)
    sxx = sum(x*x for x, _ in pts)
    sxy = sum(x*y for x, y in pts)

    b = (m*sxy - sx*sy)/(m*sxx - sx*sx)
    a = (sy - b*sx)/m

    n = b
    K = math.exp(a/b)

    yMean = sy/m
    ssTot = sum((y - yMean)**2 for _, y in pts)
    ssRes = sum((y - (a + b*x))**2 for x, y in pts)
    r2 = 1 - ssRes/max(ssTot, 1e-30)

    return n, K, r2


def avrami(t, n, K):
    return 1 - math.exp(-((K*t)**n))


def selftest():
    n0, K0 = 2.0, 0.1
    samples = [
        (t*0.1, avrami(t*0.1, n0, K0)) for t in range(1, 101)
    ]
    n, K, r2 = fit(samples)

    print("  synthetic (n, K) = ({:g}, {:g}) -> fitted ({:.6f}, "
          "{:.6f}), R2 = {:.8f}".format(n0, K0, n, K, r2))

    if abs(n - n0)/n0 > 0.01 or abs(K - K0)/K0 > 0.01 or r2 < 0.999999:
        print("FAIL: the Avrami fit did not recover the synthetic "
              "parameters within 1%")
        return 1

    print("PASS: the DSC-style Avrami fit recovers the parameters "
          "within 1%")
    return 0


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        sys.exit(selftest())

    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)

    samples = []
    with open(sys.argv[1]) as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.replace(",", " ").split()
            if len(parts) < 2:
                continue
            samples.append((float(parts[0]), float(parts[1])))

    n, K, r2 = fit(samples)

    print("  Avrami fit: n = {:.6f}, K = {:.6f} 1/s, R2 = {:.8f}".format(
        n, K, r2))
    print("  moldingDict:")
    print("      avramiExponent    {:.6f};".format(n))
    print("      rateConstant      {:.6f};".format(K))


if __name__ == "__main__":
    main()
