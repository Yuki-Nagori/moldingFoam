"""Scale supported benchmark copies; preserve the physical initial profile."""
import re
import sys
from pathlib import Path
from validation_metrics import mesh_dimensions

case, name, scale, dt_scale = Path(sys.argv[1]), sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
mesh = case/'system/blockMeshDict'
text = mesh.read_text()
pattern = r'(\bhex\s*\([^()]+\)\s*)\((\d+)\s+(\d+)\s+(\d+)\)'
def resize(match):
    nx, ny, nz = map(int, match.groups()[1:])
    return '{}({} {} {})'.format(match[1], max(2, round(nx*scale)), max(2, round(ny*scale)), nz)
text, count = re.subn(pattern, resize, text)
if count != (2 if name == 'moldCHT' else 1):
    raise ValueError('unexpected explicit hex block count')
mesh.write_text(text)
ctrl = case/'system/controlDict'
text = ctrl.read_text()
if re.search(r'\badjustTimeStep\s+(yes|true|on)\s*;', text):
    raise ValueError('matrix requires fixed time steps')
text, count = re.subn(r'(\bdeltaT\s+)([^;]+);', lambda m: m[1]+format(float(m[2])*dt_scale, '.12g')+';', text)
if count != 1:
    raise ValueError('expected one deltaT')
ctrl.write_text(text)
if name == 'warpagePlate':
    nx, ny = mesh_dimensions(case)
    if ny % 2:
        raise ValueError('equal layers require even ny')
    for name, values in [('T', (0.001, 0.002)), ('e', (0.434, 0.868))]:
        path = case/'0'/name
        text = path.read_text()
        rows = [str(values[j >= ny//2]) for j in range(ny) for i in range(nx)]
        text, count = re.subn(r'internalField\s+nonuniform\s+List<scalar>\s+\d+\s*\([^()]*\)\s*;',
                             'internalField nonuniform List<scalar>\n'+str(nx*ny)+'\n(\n'+'\n'.join(rows)+'\n);', text)
        if count != 1:
            raise ValueError('expected explicit two-layer initial field')
        path.write_text(text)
