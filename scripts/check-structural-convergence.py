#!/usr/bin/env python3
"""Check static displacement history without confusing it with time accuracy."""
import json
import math
import os
import sys
from pathlib import Path

from validation_metrics import field, mesh_dimensions


def history(case, window=3):
    case = Path(case)
    nx, ny = mesh_dimensions(case)
    times = sorted((float(p.name), p) for p in case.iterdir()
                   if p.is_dir() and p.name.replace('.', '', 1).isdigit()
                   and float(p.name) > 0 and (p/'D').is_file())
    if len(times) < window + 1:
        raise ValueError('insufficient displacement history')
    samples = []
    for t, directory in times:
        rows = field(directory/'D', 'vector', 3, nx*ny)
        q = sum(rows[nx - 1 + nx*j][1] for j in range(ny))/ny
        norm = math.sqrt(sum(sum(v*v for v in row) for row in rows)/(nx*ny))
        samples.append((t, q, norm))
    changes = []
    for (_, q0, n0), (_, q1, n1) in zip(samples[-window-1:-1], samples[-window:]):
        scale = max(abs(q1), n1, 1e-30)
        changes.append(max(abs(q1-q0), abs(n1-n0))/scale)
    return samples, changes


def main():
    case = sys.argv[1] if len(sys.argv) > 1 else '.'
    window = int(os.environ.get('STRUCTURAL_CONVERGENCE_WINDOW', '3'))
    tolerance = float(os.environ.get('STRUCTURAL_CONVERGENCE_TOL', '1e-3'))
    samples, changes = history(case, window)
    result = {'window': window, 'tolerance': tolerance,
              'samples': [{'time': t, 'Q': q, 'D_rms': n} for t, q, n in samples],
              'changes': changes, 'converged': all(c <= tolerance for c in changes)}
    print(json.dumps(result, allow_nan=False))
    if not result['converged']:
        print('FAIL: structural displacement has not reached static convergence', file=sys.stderr)
        return 1
    print('PASS: structural displacement static convergence')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print('FAIL:', exc)
        raise SystemExit(1)
