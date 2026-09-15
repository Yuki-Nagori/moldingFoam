#!/usr/bin/env python3
"""Audit the OpenFOAM-14 solidDisplacement corrector contract."""
import json
import os
import re
import sys
from pathlib import Path


def main():
    source = Path(os.environ.get(
        'FOAM_SOLID_DISPLACEMENT_SOURCE',
        '/opt/openfoam14/applications/modules/solidDisplacement/solidDisplacement.C'))
    if not source.is_file():
        print(json.dumps({'status': 'unavailable', 'source': str(source)}))
        return 2
    text = source.read_text(errors='replace')
    solve_count = len(re.findall(r'DEqn\.solve\(\)', text))
    initial_assignments = len(re.findall(r'initialResidual\s*=\s*DEqn\.solve', text))
    loop = bool(re.search(r'while\s*\(\s*initialResidual\s*>\s*convergenceTolerance', text))
    solve_pos = text.find('initialResidual = DEqn.solve')
    nonSolveUpdates = (
        len(re.findall(r'initialResidual\s*=\s*', text[solve_pos + 1:]))
        if solve_pos >= 0 else 0
    )
    result = {
        'status': 'ok', 'source': str(source), 'solve_count': solve_count,
        'initial_residual_assignments': initial_assignments,
        'initial_residual_updates': initial_assignments + nonSolveUpdates,
        'loop_uses_initial_residual': loop,
        'non_solve_residual_updates': nonSolveUpdates,
        'contract_violation': bool(loop and nonSolveUpdates == 0),
    }
    print(json.dumps(result, sort_keys=True))
    return 1 if result['contract_violation'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
