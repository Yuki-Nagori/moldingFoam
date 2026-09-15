#!/usr/bin/env python3
#******************************************************************************
# verify-restart-continuity.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for restart continuation (task 061).
#
# Every other case in this repository starts from startTime, so nothing
# checked that a run picked up with startFrom latestTime continues the same
# trajectory - even though the README claims the lumped mould temperature and
# the stage/seal states are "written with the fields and read on restart".
#
# The harness has already run this case through to the end time; that run is
# the continuous reference. The verifier copies the case inputs to a scratch
# directory, runs them to half the end time, restarts from there and compares
# the final fields with the reference:
#
#   - T, p, p_rgh, alpha.melt must agree (relative tolerance TOL) - i.e. the
#     restart kept every piece of state (the lumped mould temperature lives
#     in the boundary data of T, the stage/seal state in its own file);
#   - the compared state must not be trivial: the fields at the end must
#     differ from the initial ones, otherwise the check would be vacuous.
#
# Note (measured, not asserted): the derived phase-temperature fields
# (T.melt, T.air) are rewritten as "uniform" after a restart where the
# continuous run writes an explicit per-face list, with identical values.
# Byte-comparing time directories across a restart therefore fails even
# though the physics is identical.
#
# Usage: verify-restart-continuity.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

TOL = 1e-06             # relative tolerance between the two runs
FIELDS = ("T", "p", "p_rgh", "alpha.melt")
NONTRIVIAL = 1e-04      # the final state must differ from the initial by
                        # at least this relative amount


def read(path):
    with open(path, errors="replace") as f:
        return f.read()


def fieldValues(path):
    """internalField of a scalar field, uniform or nonuniform"""
    if not os.path.exists(path):
        return None
    s = read(path)
    m = re.search(r"internalField\s+nonuniform\s+List<[^>]+>\s*(\d+)\s*"
                  r"\((.*?)\)\s*;", s, re.S)
    if m:
        return [float(x) for x in m.group(2).split()]
    m = re.search(r"internalField\s+uniform\s+\(([^)]*)\)", s)
    if m:
        return [float(x) for x in m.group(1).split()]
    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", s)
    if m:
        return [float(m.group(1))]
    return None


def timeDirs(case_dir):
    out = []
    for name in os.listdir(case_dir):
        p = os.path.join(case_dir, name)
        if os.path.isdir(p) and re.match(r"^[0-9]+(\.[0-9]+)?$", name):
            out.append((float(name), name))
    return sorted(out)


def run(cmd, cwd):
    return subprocess.run(cmd, shell=True, cwd=cwd,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def patch(path, key, value):
    # [ \t] rather than \s: with MULTILINE a leading \s* can swallow the
    # newlines of the preceding entries and delete them
    s = read(path)
    s2 = re.sub(r"(?m)^([ \t]*" + key + r"[ \t]+)[^;]*;",
                r"\g<1>" + value + ";", s, count=1)
    assert s2 != s, "cannot set %s in %s" % (key, path)
    with open(path, "w") as f:
        f.write(s2)


def tail(path, n=2000):
    return read(path)[-n:] if os.path.exists(path) else ""


def main():
    case_dir = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")

    cd = read(os.path.join(case_dir, "system", "controlDict"))
    end = float(re.search(r"(?m)^endTime\s+([-+0-9.eE]+)\s*;", cd).group(1))
    split = 0.5*end

    times = timeDirs(case_dir)
    if len(times) < 2:
        print("FAIL: the reference run wrote fewer than two time directories")
        sys.exit(1)
    tRef = times[-1][1]

    # Restarted run in a scratch copy of the inputs
    work = tempfile.mkdtemp(prefix="moldingFoam-restart-")
    for item in ("0", "constant", "system"):
        shutil.copytree(os.path.join(case_dir, item),
                        os.path.join(work, item))

    ctrl = os.path.join(work, "system", "controlDict")
    patch(ctrl, "endTime", repr(split))

    r = run("blockMesh > log.blockMesh 2>&1 && foamRun > log.stage1 2>&1", work)
    if r.returncode != 0:
        print("FAIL: the first stage of the restarted run failed (rc = {})"
              .format(r.returncode))
        print(tail(os.path.join(work, "log.blockMesh")) or
              tail(os.path.join(work, "log.stage1")))
        sys.exit(1)

    patch(ctrl, "startFrom", "latestTime")
    patch(ctrl, "endTime", repr(end))

    r = run("foamRun > log.stage2 2>&1", work)
    if r.returncode != 0:
        print("FAIL: the restart stage failed (rc = {})".format(r.returncode))
        print(tail(os.path.join(work, "log.stage2")))
        sys.exit(1)

    stage2 = read(os.path.join(work, "log.stage2"))
    if "\nEnd\n" not in stage2 and not stage2.rstrip().endswith("End"):
        print("FAIL: the restarted run did not reach the end time")
        sys.exit(1)

    times = timeDirs(work)
    tNew = times[-1][1]

    print("  continuous run: {} time directories, final {}"
          .format(len(timeDirs(case_dir)), tRef))
    print("  restarted run : split at {:g}, final {}".format(split, tNew))

    if abs(float(tNew) - float(tRef)) > 1e-12:
        print("FAIL: the restarted run ended at a different time")
        sys.exit(1)

    t0 = timeDirs(case_dir)[0][1]
    fail = False
    moved = 0

    for name in FIELDS:
        a = fieldValues(os.path.join(case_dir, tRef, name))
        b = fieldValues(os.path.join(work, tNew, name))
        c = fieldValues(os.path.join(case_dir, t0, name))

        if a is None or b is None or c is None:
            print("FAIL: cannot read {} from a time directory".format(name))
            fail = True
            continue

        if len(a) != len(b):
            print("FAIL: {} has {} vs {} values".format(name, len(a), len(b)))
            fail = True
            continue

        scale = max(max(abs(x) for x in a), 1e-30)
        dmax = max(abs(x - y) for x, y in zip(a, b))
        evolved = max(abs(x - z) for x, z in zip(a, c))/scale

        print("  {:>10s}: max|continuous - restarted| = {:.3e} "
              "({:.2e} relative), state moved {:.2%} from the initial field"
              .format(name, dmax, dmax/scale, evolved))

        if dmax/scale > TOL:
            print("FAIL: {} differs between the continuous and the restarted "
                  "run (state lost on restart?)".format(name))
            fail = True
        if evolved >= NONTRIVIAL:
            moved += 1

    # Non-vacuity: at least one compared field must have evolved, otherwise
    # the agreement would be trivial (alpha.melt, for one, stays 1 here)
    if moved == 0:
        print("FAIL: no compared field moved from its initial state, so the "
              "comparison is vacuous")
        fail = True

    if fail:
        print("(the restarted run is kept for inspection: {})".format(work))
        sys.exit(1)

    shutil.rmtree(work, ignore_errors=True)

    print("PASS: a restarted run continues the same trajectory and keeps the "
          "state written with the fields")


if __name__ == "__main__":
    main()
