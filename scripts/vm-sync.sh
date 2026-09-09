#!/usr/bin/env bash
#******************************************************************************
# moldingFoam
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Sync the moldingFoam repository from a (possibly case-insensitive) macOS
# mount into a VM-native, case-sensitive build directory.
#
# OpenFOAM source trees contain file pairs that differ only in letter case
# (e.g. primitives/chars/wchar/wchar.H and the system <wchar.h>; Tensor.H
# vs tensor.H inside the flattened lnInclude directories), so building on
# a case-insensitive filesystem is unsupported by the OpenFOAM Foundation.
# When the repository lives on a macOS mount, this script mirrors it into
# a VM-native directory (Ubuntu ext4) where xmake must be run.
#
# Usage (inside the VM):
#   ~/moldingFoam/scripts/vm-sync.sh [srcDir] [buildDir]
#   # default: ~/moldingFoam -> ~/moldingFoam-build
#
# The OpenFOAM environment lives inside the VM (the apt-installed
# /opt/openfoam14, or a tree passed via --of_src), outside the synced
# tree entirely.
#******************************************************************************

set -euo pipefail

src="${1:-$HOME/moldingFoam}"
dst="${2:-$HOME/moldingFoam-build}"

[ -d "$src" ] || { echo "error: source directory $src does not exist" >&2; exit 1; }

mkdir -p "$dst"

# Refuse to build inside a case-insensitive filesystem (the whole point of
# this script): the repo source file .CASESENSITIVITYPROBE must appear with
# exactly one case variant
probe="$dst/.CaseSensitivityProbe"
rm -f "$probe" "$dst/.casesensitivityprobe"
touch "$probe"
if [ -e "$dst/.casesensitivityprobe" ]; then
    echo "error: $dst is case-insensitive; choose a VM-native destination" >&2
    rm -f "$probe"
    exit 1
fi
rm -f "$probe"

rsync -a --delete "$src/" "$dst/"

echo "vm-sync: $src -> $dst"
echo "vm-sync: now run: cd $dst && xmake"
