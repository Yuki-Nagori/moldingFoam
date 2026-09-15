#!/usr/bin/env python3
"""Fixed through-thickness quadrature contract for Dual Domain (no adaptivity)."""
# SPDX-License-Identifier: GPL-3.0-or-later
import math

# 8-point Gauss-Legendre abscissae and weights on [-1, 1], version 1.
POINTS = (-0.9602898564975363, -0.7966664774136267, -0.5255324099163290,
          -0.1834346424956498, 0.1834346424956498, 0.5255324099163290,
          0.7966664774136267, 0.9602898564975363)
WEIGHTS = (0.1012285362903763, 0.2223810344533745, 0.3137066458778873,
           0.3626837833783620, 0.3626837833783620, 0.3137066458778873,
           0.2223810344533745, 0.1012285362903763)


def points(thickness):
    if type(thickness) not in (int, float) or not math.isfinite(thickness) or thickness <= 0:
        raise ValueError("thickness must be finite and positive")
    return tuple(0.5 * thickness * x for x in POINTS)


def integrate(function, thickness):
    """Integrate f(z) dz, with z=0 at the midsurface and +z outerward."""
    return 0.5 * thickness * sum(w * function(z)
                                  for z, w in zip(points(thickness), WEIGHTS))


def validate_rule():
    if len(POINTS) != 8 or len(WEIGHTS) != 8:
        raise ValueError("quadrature-v1 requires exactly eight points and weights")
    if any(not math.isfinite(x) for x in POINTS + WEIGHTS):
        raise ValueError("quadrature values must be finite")
    if any(x <= -1 or x >= 1 for x in POINTS) or any(w <= 0 for w in WEIGHTS):
        raise ValueError("quadrature points/weights outside contract")
    if abs(sum(WEIGHTS) - 2.0) > 1e-14:
        raise ValueError("quadrature weights must sum to 2")
    if any(abs(POINTS[i] + POINTS[-i - 1]) > 1e-14 for i in range(8)):
        raise ValueError("quadrature points must be symmetric")
    return True
