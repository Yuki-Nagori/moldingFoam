#!/bin/bash
# repro-037.sh - fast reproducer + variant matrix for the 037 exit crash
# (task 056): "malloc_consolidate(): unaligned fastbin chunk detected"
# from UPstream::exit <- argList::~argList() printed after "End".
#
# Key finding this script encodes: the crash does NOT need the full mould
# cycle, nor even a long run. Any run that reaches "End" crashes on exit,
# and the corruption is already in place after the first steps: endTime
# 0.002 (19 steps, ~20 s) crashes exactly like the full 5069-step cycle.
#
# Every variant runs in a FRESH copy of the case: the crash is heap-layout
# sensitive, and reused case dirs also carry the polluted 0/ state fixed in
# task 055.
#
# Use (inside the OpenFOAM environment, e.g. after
#   source ~/moldingfoam-env/openfoam14/etc/bashrc):
#
#   scripts/diag/repro-037.sh              # whole matrix
#   scripts/diag/repro-037.sh C-btl-tcp    # only variants matching the name
#
# Env knobs: CASE (default ~/cli10), WORK (/tmp/m037), ENDTIME (0.05),
# CANARY_SO (the LD_PRELOAD probe built from malloc-canary.c; variants that
# need it are skipped when it is missing).

set -u

: "${WM_PROJECT_DIR:?repro-037.sh must be invoked inside the OpenFOAM environment}"

caseDir="${CASE:-$HOME/cli10}"
work="${WORK:-/tmp/m037}"
endTime="${ENDTIME:-0.005}"
canary="${CANARY_SO:-/tmp/malloc-canary.so}"

filter="${1:-}"

if [ ! -d "$caseDir" ]; then
    echo "repro-037.sh: case not found: $caseDir" >&2
    exit 2
fi

mkdir -p "$work"

# name|needs-canary|command (run with cwd = the fresh case copy)
variants=(
"A-baseline-repeat|0|mpirun -np 4 foamRun -parallel"
"B-arena1|0|MALLOC_ARENA_MAX=1 mpirun -np 4 foamRun -parallel"
"C-perturb|0|MALLOC_PERTURB_=165 mpirun -np 4 foamRun -parallel"
"D-btl-tcp|0|mpirun --mca btl self,tcp -np 4 foamRun -parallel"
"E-canary-pstream|1|LD_PRELOAD=$canary CANARY_ONLY=libPstream.so mpirun -np 4 foamRun -parallel"
"F-canary-openfoam|1|LD_PRELOAD=$canary CANARY_ONLY=libOpenFOAM.so mpirun -np 4 foamRun -parallel"
"G-canary-openpal|1|LD_PRELOAD=$canary CANARY_ONLY=libopen-pal.so mpirun -np 4 foamRun -parallel"
"H-canary-mpi|1|LD_PRELOAD=$canary CANARY_ONLY=libmpi.so mpirun -np 4 foamRun -parallel"
"I-canary-libc|1|LD_PRELOAD=$canary CANARY_ONLY=libc.so.6 mpirun -np 4 foamRun -parallel"
"J-canary-all-libs|1|LD_PRELOAD=$canary CANARY_ONLY=lib mpirun -np 4 foamRun -parallel"
)

printf '%-22s %-6s %-4s %-9s %-6s %-9s\n' variant rc End corrupt hits tablefull

for v in "${variants[@]}"; do
    name="${v%%|*}"
    rest="${v#*|}"
    needs="${rest%%|*}"
    cmd="${rest#*|}"

    if [ -n "$filter" ] && [ "${name#*"$filter"}" = "$name" ]; then
        continue
    fi
    if [ "$needs" = 1 ] && [ ! -f "$canary" ]; then
        printf '%-22s %s\n' "$name" "skipped (no $canary)"
        continue
    fi

    d="$work/$name"
    rm -rf "$d"
    cp -r "$caseDir" "$d" || { echo "$name: copy failed" >&2; continue; }
    sed -i "s/^endTime .*/endTime         $endTime;/" "$d/system/controlDict"

    (
        cd "$d" || exit 3
        eval "$cmd" > log.foamRun 2>&1
        echo $? > rc.txt
    )

    rc=$(cat "$d/rc.txt" 2>/dev/null || echo '?')
    end=$(grep -c '^End' "$d/log.foamRun")
    corrupt=$(grep -cE 'malloc_consolidate|unaligned fastbin' "$d/log.foamRun")
    hits=$(grep -c 'CANARY-HIT' "$d/log.foamRun")
    tablefull=$(grep -c 'table-full-stopping' "$d/log.foamRun")

    printf '%-22s %-6s %-4s %-9s %-6s %-9s\n' \
        "$name" "$rc" "$end" "$corrupt" "$hits" "$tablefull"

    # Hits carry "alloc=<module>+0x<offset>": print and symbolise them
    if [ "$hits" -gt 0 ]; then
        grep 'CANARY-HIT' "$d/log.foamRun" | sort | uniq -c | head -20
    fi
done

echo
echo "logs under $work/<variant>/log.foamRun  (a crashing run leaves rc.txt != 0)"
