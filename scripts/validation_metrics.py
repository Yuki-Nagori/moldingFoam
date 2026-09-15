"""Strict ASCII readers and explicit measurement output for validation cases."""
import json
import math
import os
from pathlib import Path
import re


def clean(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)


def mesh_dimensions(case):
    text = clean((Path(case)/'system/blockMeshDict').read_text())
    blocks = re.findall(r'\bhex\s*\([^()]+\)\s*\((\d+)\s+(\d+)\s+(\d+)\)', text)
    if len(blocks) != 1:
        raise ValueError('expected one explicit hex block')
    nx, ny, nz = map(int, blocks[0])
    if nx < 2 or ny < 2 or nz != 1:
        raise ValueError('expected a 2D benchmark mesh with nx, ny >= 2')
    return nx, ny


def field(path, kind, components, count):
    text = clean(Path(path).read_text()).split('boundaryField', 1)[0]
    m = re.search(r'\binternalField\s+(.*?)\s*;', text, re.S)
    if not m:
        raise ValueError(f'{path}: missing internalField')
    value = m[1]
    uniform = re.fullmatch(r'uniform\s*\(([^()]*)\)', value, re.S)
    if uniform:
        rows = [uniform[1].split()]
    else:
        match = re.fullmatch(r'nonuniform\s+List<'+kind+r'>\s+(\d+)\s*\((.*)\)', value, re.S)
        if not match or int(match[1]) != count:
            raise ValueError(f'{path}: declared length differs from mesh')
        body = match[2]
        rows = [r.split() for r in re.findall(r'\(([^()]*)\)', body)]
        if len(rows) != count or re.sub(r'\([^()]*\)', '', body).strip():
            raise ValueError(f'{path}: truncated or malformed field')
    if any(len(row) != components for row in rows):
        raise ValueError(f'{path}: incorrect component count')
    rows = [[float(v) for v in row] for row in rows]
    if not all(math.isfinite(v) for row in rows for v in row):
        raise ValueError(f'{path}: non-finite component')
    return rows*count if uniform else rows


def completed_time(case):
    case = Path(case)
    log = (case/'log.foamRun').read_text()
    if not re.search(r'^[ \t]*End[ \t]*$', log, re.M):
        raise ValueError('solver End marker missing')
    ctrl = clean((case/'system/controlDict').read_text())
    end = float(re.search(r'\bendTime\s+([^;]+);', ctrl)[1])
    times = []
    for path in case.iterdir():
        try:
            t = float(path.name)
        except ValueError:
            continue
        if path.is_dir() and math.isfinite(t) and t > 0:
            times.append((t, path.name))
    if not times or not math.isclose(max(times)[0], end, rel_tol=1e-9, abs_tol=1e-12):
        raise ValueError('final output does not match endTime')
    return max(times)[1]


def section_rows(rows, nx, ny, x, length=30.0):
    """Interpolate cell-centre rows at one fixed physical x section."""
    if not (0.0 < x < length) or nx < 2 or len(rows) != nx*ny:
        raise ValueError('section lies outside the mesh or field size is invalid')
    centres = [(i + 0.5)*length/nx for i in range(nx)]
    if x <= centres[0]:
        left = right = 0
    elif x >= centres[-1]:
        left = right = nx - 1
    else:
        right = next(i for i, value in enumerate(centres) if value >= x)
        left = right - 1
    weight = 0.0 if left == right else (x-centres[left])/(centres[right]-centres[left])
    return [[u + weight*(v-u) for u, v in zip(rows[left+nx*j], rows[right+nx*j])]
            for j in range(ny)]


def section_average(rows, nx, ny, x, length=30.0):
    values = section_rows(rows, nx, ny, x, length)
    return [sum(row[k] for row in values)/ny for k in range(len(values[0]))]


def measure(quantity, value, reference, error, threshold, time):
    if not all(math.isfinite(v) for v in (value, reference, error, threshold, float(time))):
        raise ValueError('non-finite metric')
    within = error <= threshold
    result = dict(quantity=quantity, value=value, reference=reference,
                  relative_error=error, threshold=threshold,
                  within_threshold=within, time=float(time))
    output = os.environ.get('UNCERTAINTY_METRICS_FILE')
    if output:
        Path(output).write_text(json.dumps(result, allow_nan=False)+'\n')
    matrix = os.environ.get('UNCERTAINTY_MATRIX_MODE') == '1'
    if matrix:
        if not output:
            raise ValueError('measurement mode requires UNCERTAINTY_METRICS_FILE')
        print('MEASURED: relative error {:.3%}; standard threshold {:.1%}: {}'.format(
            error, threshold, 'within' if within else 'exceeded'))
    return within or matrix
