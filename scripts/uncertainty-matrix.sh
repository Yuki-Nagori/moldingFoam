#!/usr/bin/env bash
# Emit the task-069 three-grid/three-time-step matrix for nightly runners.
# Local invocation is intentionally dry-run; the full matrix is resource
# bounded by CI and each row must run in a fresh case copy.
set -euo pipefail

mode="${1:---dry-run}"
if [[ "$mode" != "--dry-run" && "$mode" != "--emit" ]]; then
    echo "usage: $0 [--dry-run|--emit]" >&2
    exit 2
fi

printf 'case\tgridScale\tdtScale\tquantity\n'
for row in \
    "validation/thermoelastic\t0.5\t0.5\tfree-end-deflection" \
    "validation/thermoelastic\t1\t1\tfree-end-deflection" \
    "validation/thermoelastic\t2\t2\tfree-end-deflection" \
    "validation/moldCHT\t0.5\t0.5\tboundary-heat-flux" \
    "validation/moldCHT\t1\t1\tboundary-heat-flux" \
    "validation/moldCHT\t2\t2\tboundary-heat-flux" \
    "validation/warpagePlate\t0.5\t0.5\tfree-end-deflection" \
    "validation/warpagePlate\t1\t1\tfree-end-deflection" \
    "validation/warpagePlate\t2\t2\tfree-end-deflection"
do
    printf '%b\n' "$row"
done

if [[ "$mode" == "--dry-run" ]]; then
    echo "uncertainty-matrix: dry-run only; execute rows in nightly CI" >&2
fi
