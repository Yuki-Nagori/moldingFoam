#!/bin/bash
# check-lib-duplication.sh - task 056 guard: the solver library must be
# mapped exactly once at run time.
#
# OpenFOAM reaches the solver library by two names: the case's `libs (...)`
# entry (libmoldingFoam.so) and the solver-name lookup (<solver>Solver.so).
# When those two names resolve to two *different files* - a bundle copy in
# $FOAM_LIBBIN plus a hand-built copy in $FOAM_USER_LIBBIN, say - a parallel
# run maps the library twice: every runtime selection table then reports
# "Duplicate entry", the two copies' static objects collide during exit,
# and glibc aborts with "malloc_consolidate(): unaligned fastbin chunk
# detected" (rc=134). Serial runs do not reproduce it, which is why it can
# go unnoticed for a long time.
#
# Usage:
#   scripts/diag/check-lib-duplication.sh                # inspect the env
#   scripts/diag/check-lib-duplication.sh <run log>      # also scan a log
#
# Exit code 1 when two or more distinct library files are on the path, or
# when the log carries duplicate-registration warnings.
#
# Must be run inside the OpenFOAM environment (it reads FOAM_LIBBIN and
# FOAM_USER_LIBBIN).

set -u

if [ -z "${WM_PROJECT_DIR:-}" ]; then
    echo "check-lib-duplication.sh: run inside the OpenFOAM environment" >&2
    exit 2
fi

dirs=()
for d in "${FOAM_LIBBIN:-}" "${FOAM_USER_LIBBIN:-}"; do
    [ -n "$d" ] && [ -d "$d" ] && dirs+=("$d")
done
IFS=: read -r -a ldpath <<< "${LD_LIBRARY_PATH:-}"
for d in "${ldpath[@]}"; do
    [ -n "$d" ] && [ -d "$d" ] && dirs+=("$d")
done

echo "search dirs:"
printf '  %s\n' "${dirs[@]}"
echo
echo "candidates (realpath resolves the <name>Solver.so symlink):"

declare -a reals=()
while IFS= read -r f; do
    real=$(readlink -f "$f" 2>/dev/null || echo "$f")
    printf '  %s\n      -> %s\n' "$f" "$real"
    reals+=("$real")
done < <(find "${dirs[@]}" -maxdepth 1 -name 'libmoldingFoam*.so' 2>/dev/null | sort -u)

if [ "${#reals[@]}" -eq 0 ]; then
    echo "  (none found - is the library built?)"
fi

distinct=$(printf '%s\n' "${reals[@]}" | sort -u | wc -l)

status=0
echo
if [ "$distinct" -gt 1 ]; then
    echo "FAIL: $distinct distinct libmoldingFoam libraries on the loader path"
    echo "      keep exactly one real file plus the <name>Solver.so symlink"
    status=1
elif [ "$distinct" -eq 1 ]; then
    echo "OK: exactly one libmoldingFoam library on the loader path"
fi

if [ "$#" -ge 1 ]; then
    log="$1"
    if [ -f "$log" ]; then
        dup=$(grep -c 'Duplicate entry' "$log")
        corrupt=$(grep -cE 'malloc_consolidate|unaligned fastbin' "$log")
        echo "log $log: duplicate-registration lines = $dup, glibc corruption lines = $corrupt"
        if [ "$dup" -gt 0 ] || [ "$corrupt" -gt 0 ]; then
            echo "FAIL: the run loaded the library more than once"
            grep -o 'Duplicate entry [A-Za-z0-9_]* in runtime selection table [A-Za-z0-9_]*' "$log" \
                | sort -u | head -10 | sed 's/^/      /'
            status=1
        fi
    else
        echo "log not found: $log" >&2
        status=1
    fi
fi

exit "$status"
