#!/usr/bin/env python3
"""Scale a copy of case-contract up for performance profiles (task 050).

Scales the in-plane block resolution by <scale> and shortens the run
(writes and function objects muted), so a perf window sees the interior
of the fill rather than start-up or I/O.

Usage: perf-large-case.py <caseDir> <scale> <endTime>
"""
import os
import re
import sys

case = sys.argv[1]
scale = float(sys.argv[2])
end_time = sys.argv[3]


def rescale(mo):
    nx, ny, nz = (int(mo.group(i)) for i in (2, 3, 4))
    return mo.group(1) + "({} {} {})".format(
        max(1, round(nx*scale)), max(1, round(ny*scale)), nz)


bm = os.path.join(case, "system", "blockMeshDict")
text = open(bm).read()
m = re.search(r'\bblocks\b\s*\(\s*(.*?)\n\s*\)\s*;', text, re.S)
if not m:
    raise SystemExit("blocks not found in blockMeshDict")

# Only the resolution triple that follows a closing parenthesis (the hex
# vertex list) is scaled; a bare 3-tuple pattern would also hit
# simpleGrading (1 1 1) and silently grade the mesh
new = re.sub(r'(\)\s*)\(\s*(\d+)\s+(\d+)\s+(\d+)\s*\)', rescale, m.group(1))
if new == m.group(1):
    raise SystemExit("no block resolution changed (bad scale?)")
open(bm, "w").write(text[:m.start(1)] + new + text[m.end(1):])

counts = [tuple(int(x) for x in c)
          for c in re.findall(r'\)\s*\(\s*(\d+)\s+(\d+)\s+(\d+)\s*\)', new)]
nx = sum(c[0] for c in counts)
ny = counts[0][1]
nz = counts[0][2]
print("grid: {} x {} x {} = {} cells".format(nx, ny, nz, nx*ny*nz))

cd = os.path.join(case, "system", "controlDict")
text = open(cd).read()
text = re.sub(r'(?m)^(endTime\s+)[^\s;]+;',
              lambda mo: mo.group(1) + end_time + ";", text, count=1)
text = re.sub(r'(?m)^(writeInterval\s+)[^\s;]+;',
              lambda mo: mo.group(1) + "1000000;", text, count=1)
text = text.replace("writeInterval   1;", "writeInterval   1000000;")
open(cd, "w").write(text)
print("endTime {} s, writes and function-object sampling muted".format(end_time))
