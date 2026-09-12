#!/usr/bin/env python3
#******************************************************************************
# verify-shrink-bar.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the free-shrinkage quarter bar (task 034):
# a 30 x 1 bar whose x = 0 and y = 0 planes are rollers (slip) and whose
# other two faces are traction-free, carrying a uniform free shrinkage
# eps = 0.01 (imposed as T = eps with alphav = 1, Tref = 0).
#
# With nu = 0 the exact stress-free solution is the uniform eigenstrain
# field u = eps (x, y): the displacement is linear, the rollers see no
# reaction and the free faces stay flat. This verifier checks
#
#   - the internal displacement field is linear in (x, y), which also
#     implies the no-penetration condition on both roller planes
#     (u_x = 0 at x = 0, u_y = 0 at y = 0 by extrapolation);
#   - the axial displacement over the free end equals eps L and is
#     uniform (zero axial reaction);
#   - the lateral displacement over the free surface equals eps h and is
#     uniform (zero lateral reaction).
#
# Usage: verify-shrink-bar.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

L = 30.0
H = 1.0
EPS = 0.01
NX = 48
NY = 16
TOL = 1e-3


def latest_time(case_dir):
    times = sorted(
        (d for d in os.listdir(case_dir)
         if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0),
        key=float,
    )
    return times[-1] if times else None


def balanced_block(text, name):
    """Return the text between the braces following `name {`."""
    m = re.search(r"(?:^|\s)" + re.escape(name) + r"\s*\{", text)
    if m is None:
        return None

    start = m.end()
    depth = 1
    i = start
    while i < len(text) and depth:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1

    return text[start:i - 1]


def vector_list(text):
    """Parse a nonuniform List<vector> or a uniform vector."""
    m = re.search(
        r"nonuniform\s+List<vector>\s*\n\s*\d+\s*\n\((.*?)\)\s*;",
        text,
        re.S,
    )
    if m is not None:
        return [
            [float(x) for x in g.split()]
            for g in re.findall(r"\(([^()]*)\)", m.group(1))
        ]

    m = re.search(r"uniform\s*\(\s*([^()]*)\)\s*;", text)
    if m is not None:
        return [[float(x) for x in m.group(1).split()]]

    return None


def patch_values(boundary, name):
    """The last parseable vector `value` entry of a patch block."""
    block = balanced_block(boundary, name)
    if block is None:
        return None

    values = None
    for m in re.finditer(r"\bvalue\b", block):
        parsed = vector_list(block[m.end():])
        if parsed is not None:
            values = parsed

    return values


def read_D(case_dir, time):
    with open(os.path.join(case_dir, time, "D"), errors="replace") as f:
        txt = f.read()

    m = re.search(r"internalField\s+uniform\s*\(\s*([^()]*)\)\s*;", txt)
    if m is not None:
        internal = [[float(x) for x in m.group(1).split()]]
    else:
        m = re.search(r"internalField\s+nonuniform\s+List<vector>", txt)
        if m is None:
            return None, {}
        internal = vector_list(txt[m.start():])

    boundary = balanced_block(txt, "boundaryField") or ""

    return internal, {
        name: patch_values(boundary, name)
        for name in ("topSurface", "tractionEnd")
    }


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    log_path = None
    for candidate in ("log.run", "log.foamRun", "log.run-parallel"):
        path = os.path.join(case_dir, candidate)
        if os.path.exists(path):
            log_path = path
            break
    if log_path is None:
        print("FAIL: no solver log found (log.run/log.foamRun)")
        sys.exit(1)

    with open(log_path, errors="replace") as f:
        if "End" not in f.read():
            print("FAIL: the solver did not reach the end time")
            sys.exit(1)

    time = latest_time(case_dir)
    if time is None:
        print("FAIL: no written time directories")
        sys.exit(1)

    internal, patches = read_D(case_dir, time)
    if internal is None or len(internal) != NX*NY:
        print("FAIL: cannot read the internal displacement field")
        sys.exit(1)

    fail = False

    # Internal linearity: u = eps (x, y) on the cell centres. The fields
    # are written x-fastest over the 48 x 16 structured mesh.
    maxErrX = maxErrY = 0.0
    for j in range(NY):
        for i in range(NX):
            ux, uy = internal[i + NX*j][0], internal[i + NX*j][1]
            x = (i + 0.5)*L/NX
            y = (j + 0.5)*H/NY
            maxErrX = max(maxErrX, abs(ux - EPS*x))
            maxErrY = max(maxErrY, abs(uy - EPS*y))

    relX = maxErrX/(EPS*L)
    relY = maxErrY/(EPS*H)
    print("  linearity: max|u_x - eps x| = {:.3e} m ({:.2e} of eps L)".format(
        maxErrX, relX))
    print("  linearity: max|u_y - eps y| = {:.3e} m ({:.2e} of eps h)".format(
        maxErrY, relY))
    if relX > TOL or relY > TOL:
        print("FAIL: the displacement field is not the free eigenstrain")
        fail = True

    # Free end: u_x = eps L uniformly (zero axial reaction)
    te = patches.get("tractionEnd")
    if te is None:
        print("FAIL: cannot read the tractionEnd patch")
        fail = True
    else:
        ux = [v[0] for v in te]
        mean = sum(ux)/len(ux)
        spread = max(abs(v - mean) for v in ux)
        print("  free end u_x = {:.6e} m (expected {:.6e}, spread {:.2e})"
              .format(mean, EPS*L, spread))
        if abs(mean - EPS*L)/(EPS*L) > TOL or spread/(EPS*L) > TOL:
            print("FAIL: the free end axial strain is not free and uniform")
            fail = True

    # Free surface: u_y = eps h uniformly (zero lateral reaction)
    ts = patches.get("topSurface")
    if ts is None:
        print("FAIL: cannot read the topSurface patch")
        fail = True
    else:
        uy = [v[1] for v in ts]
        mean = sum(uy)/len(uy)
        spread = max(abs(v - mean) for v in uy)
        print("  free surface u_y = {:.6e} m (expected {:.6e}, spread {:.2e})"
              .format(mean, EPS*H, spread))
        if abs(mean - EPS*H)/(EPS*H) > TOL or spread/(EPS*H) > TOL:
            print("FAIL: the free surface lateral strain is not free and "
                  "uniform")
            fail = True

    if fail:
        sys.exit(1)

    print("PASS: the uniform free shrinkage is reproduced exactly about "
          "the rollers (stress-free)")


if __name__ == "__main__":
    main()
