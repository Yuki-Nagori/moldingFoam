#!/usr/bin/env python3
#******************************************************************************
# free_strain.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Map the PVT free shrinkage of the moulded part to an equivalent
# thermal eigenstrain for solidDisplacement (task 013b).
#
# The free linear shrinkage relative to the reference (solid) density is
#
#     eps = 1 - (rhoRef/rho)^(1/3)
#
# solidDisplacement imposes the thermal eigenstrain alphav*T with a zero
# reference temperature, so the equivalent temperature field is
#
#     T_eq = eps/alphav
#
# The frozen equivalent field is written with zero-gradient boundaries
# (the structural case runs with a vanishing conductivity so the field
# stays put and acts as a pure eigenstrain load).
#
# Usage:
#   free_strain.py --selftest
#   free_strain.py --case <caseDir> --time <t> --rho-ref <rho> \
#       --alphav <alpha> [--rho <fieldName>] [--output <file>]
#******************************************************************************

import os
import re
import sys


def free_linear_strain(rho, rho_ref):
    return 1.0 - (rho_ref/rho)**(1.0/3.0)


def equivalent_temperature(eps, alphav):
    return eps/alphav


def read_scalar_field(path):
    with open(path, errors="replace") as f:
        txt = f.read()

    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n\s*\d+\s*\n\((.*?)\)\s*;",
        txt,
        re.S,
    )
    if m is not None:
        return [float(x) for x in m.group(1).split()], txt

    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", txt)
    if m is not None:
        return [float(m.group(1))], txt

    return None, txt


def patch_names(txt):
    """Return the boundary patch names of a field file."""
    m = re.search(r"boundaryField\s*\{(.*)\}\s*$", txt, re.S)
    if m is None:
        return []
    return re.findall(r"^\s*(\w+)\s*$", m.group(1), re.M)


def write_field(path, name, values):
    with open(path, "w") as f:
        f.write("""/*--------------------------------*- C++ -*----------------------------------*\\
| =========                 |                                                 |
| \\\\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox           |
|  \\\\    /   O peration     | Website:  https://openfoam.org                  |
|   \\\\  /    A nd           | Version:  14                                    |
|    \\\\/     M anipulation  |                                                 |
\\*---------------------------------------------------------------------------*/
FoamFile
{
    format      ascii;
    class       volScalarField;
    object      %s;
}
dimensions      [temperature];
internalField   nonuniform List<scalar>
%d
(
%s
)
;
boundaryField
{
    ".*"
    {
        type            zeroGradient;
    }
}
""" % (name, len(values), "\n".join("%.10g" % v for v in values)))


def selftest():
    ok = True

    eps0 = free_linear_strain(950, 950)
    print("  eps(rho = rhoRef) = %.3e (expect 0)" % eps0)
    ok = ok and abs(eps0) < 1e-15

    eps1 = free_linear_strain(2*950, 950)
    print("  eps(2 rhoRef) = %.6f (expect %.6f)"
          % (eps1, 1 - 0.5**(1/3)))
    ok = ok and abs(eps1 - (1 - 0.5**(1/3))) < 1e-12

    # Equivalent temperature round trip
    T = equivalent_temperature(2e-3, 1.0)
    print("  T_eq(eps = 2e-3, alphav = 1) = %.6f" % T)
    ok = ok and abs(1.0*T - 2e-3) < 1e-15

    # Two equal layers with dEps = 1e-3, h = 1 -> Timoshenko kappa
    kappa = 3*1e-3/(2*1.0)
    print("  bimetal kappa(dEps = 1e-3, h = 1) = %.6f (expect 1.5e-3)"
          % kappa)
    ok = ok and abs(kappa - 1.5e-3) < 1e-15

    print("PASS: free-strain mapping self-test" if ok else "FAIL")
    return 0 if ok else 1


def main():
    if "--selftest" in sys.argv:
        sys.exit(selftest())

    def arg(name, default=None):
        if name in sys.argv:
            return sys.argv[sys.argv.index(name) + 1]
        return default

    case = arg("--case")
    if case is None:
        print(__doc__)
        sys.exit(2)

    time = arg("--time")
    rho_ref = float(arg("--rho-ref"))
    alphav = float(arg("--alphav"))
    rho_name = arg("--rho", "rho")
    out = arg("--output", "0/T_eq")

    rho, txt = read_scalar_field(os.path.join(case, time, rho_name))
    if rho is None:
        print("FAIL: cannot read the %s field" % rho_name)
        sys.exit(1)

    eps = [free_linear_strain(r, rho_ref) for r in rho]
    Teq = [equivalent_temperature(e, alphav) for e in eps]

    write_field(os.path.join(case, out), "T_eq", Teq)

    print("  cells              = %d" % len(rho))
    print("  eps range          = %.6e .. %.6e" % (min(eps), max(eps)))
    print("  T_eq range [K]     = %.6e .. %.6e" % (min(Teq), max(Teq)))
    print("  wrote %s" % out)


if __name__ == "__main__":
    main()
