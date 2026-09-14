#!/usr/bin/env python3
"""Bucket a `perf report --sort dso` dump into MPI / compute shares (task 050)."""
import re
import sys

path = sys.argv[1]
share = {}
total = 0.0
with open(path) as fh:
    for line in fh:
        line = line.rstrip()
        if not line or line.startswith("#") or line.startswith("|"):
            continue
        m = re.match(r"\s*([\d.]+)%\s+(\S+)\s*$", line)
        if not m:
            continue
        pct = float(m.group(1))
        if pct <= 0.0:
            continue
        dso = m.group(2)
        share[dso] = share.get(dso, 0.0) + pct
        total += pct


def bucket(dso):
    low = dso.lower()
    if any(k in low for k in ("libmpi", "open-pal", "open-rte", "mca_",
                              "libopenmpi", "libmpi_", "ucx", "libfabric")):
        return "MPI stack (communication / blocking waits)"
    if "molding" in low:
        return "moldingFoam"
    if dso.startswith("lib") or "solver" in low:
        return "OpenFOAM compute libraries"
    return "other (libc, kernel, python, ...)"


agg = {}
for dso, pct in share.items():
    agg[bucket(dso)] = agg.get(bucket(dso), 0.0) + pct

print("total sample weight: {:.1f}%".format(total))
for name, pct in sorted(agg.items(), key=lambda kv: -kv[1]):
    print("  {:<44s} {:5.1f}%".format(name, pct))
print("\ntop libraries:")
for dso, pct in sorted(share.items(), key=lambda kv: -kv[1])[:10]:
    print("  {:<44s} {:5.1f}%".format(dso, pct))
