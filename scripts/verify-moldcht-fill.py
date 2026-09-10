#!/usr/bin/env python3
#******************************************************************************
# verify-moldcht-fill.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the filling conjugate heat transfer
# benchmark: a channel cavity is filled by moldingFoam (VoF melt/air) while
# a mould block above it is solved by the solid module. All mould outer
# faces are adiabatic, so the melt + mould domain is closed except for the
# inlet and the vent and the global energy change must equal the enthalpy
# carried in at the gate minus the melt enthalpy advected out of the vent.
#
# Checks:
#   1. interface temperature continuity,
#   2. global melt + mould energy balance (< 2%),
#   3. the injected melt volume matches the cavity fill (front mass balance,
#      < 1%); the benchmark stops just before the front breaks through the
#      vent, so the outlet bookkeeping cannot bias the balance.
#
# Usage: verify-moldcht-fill.py <caseDir>    (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

# --- benchmark parameters (must match the case dictionaries) ---
NCX = 240            # cells along the channel
NCY = 8              # cavity cells across the channel
NMY = 10             # mould cells across the block
DX = 0.25e-3         # [m]
DYC = 0.25e-3        # [m]
DYM = 0.5e-3         # [m]
DZ = 2e-3            # [m]
RHOM = 1.0/1.2495299e-03   # melt density from the constant-density Tait
CVM = 2400.0         # melt Cv [J/kg/K]
RHOMOLD = 7800.0     # mould density [kg/m^3]
CVMOLD = 500.0       # mould Cv [J/kg/K]
Q = 1e-07            # inlet volumetric flow rate [m^3/s]
MDOT = RHOM*Q        # melt mass flow rate [kg/s]
TIN = 480.0          # inlet temperature [K]
TREF = 300.0         # reference (initial air) temperature [K]

VC = DX*DYC*DZ       # cavity cell volume [m^3]
VM = DX*DYM*DZ       # mould cell volume [m^3]

SCALAR_LIST = re.compile(
    r"List<scalar>\s*\n?\s*(\d+)\s*\((.*?)\)", re.S)


def scalars(text):
    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", text)
    if m:
        return None, float(m.group(1))
    m = SCALAR_LIST.search(text)
    if not m:
        raise RuntimeError("no scalar list found")
    vals = [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]
    return vals, None


def internal(text, n=0):
    vals, uni = scalars(text)
    if vals is None:
        return [uni]*n if n else [uni]
    return vals


def patch_value(text, patch):
    m = re.search(r"\b" + patch + r"\b\s*\{(.*?)\n    \}", text, re.S)
    if not m:
        raise RuntimeError("patch {} not found".format(patch))
    body = m.group(1)
    m = re.search(r"\bvalue\s+uniform\s+([-+0-9.eE]+)", body)
    if m:
        return float(m.group(1))
    m = re.search(r"\bvalue\s+nonuniform\s+" + SCALAR_LIST.pattern, body, re.S)
    if m:
        vals = [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]
        return sum(vals)/len(vals)
    raise RuntimeError("no value for patch {}".format(patch))


def read(path):
    with open(path) as handle:
        return handle.read()


def time_dir(time):
    return str(int(time)) if abs(time - round(time)) < 1e-9 else repr(time)


def energy(case_dir, time):
    tdir = os.path.join(case_dir, time_dir(time))
    tcav = read(os.path.join(tdir, "cavity", "T"))
    tmol = read(os.path.join(tdir, "mold", "T"))
    cav = internal(tcav, NCX*NCY)
    mol = internal(tmol, NCX*NMY)
    if time > 0:
        alph = read(os.path.join(tdir, "cavity", "alpha.melt"))
        alpha = internal(alph, NCX*NCY)
        alpha_vent = patch_value(alph, "vent")
        tvent = patch_value(tcav, "vent")
    else:
        alpha = [0.0]*NCX*NCY
        alpha_vent = 0.0
        tvent = TREF

    ecav = RHOM*CVM*sum(
        alpha[i]*(cav[i] - TREF) for i in range(min(len(alpha), len(cav))))*VC
    emold = RHOMOLD*CVMOLD*sum(t - TREF for t in mol)*VM
    return ecav, emold, tvent, alpha_vent, alpha


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    times = [0.0] + sorted(
        float(d) for d in os.listdir(case_dir)
        if re.match(r"^[0-9.]+$", d)
        and os.path.isdir(os.path.join(case_dir, d))
        and float(d) > 0
    )

    e0c, e0m, _, _, _ = energy(case_dir, 0)
    e0 = e0c + e0m

    samples = []
    for t in times:
        ec, em, tvent, avent, alpha = energy(case_dir, t)
        samples.append((t, ec + em, tvent, avent, alpha))

    # Melt mass stored in the cavity; the melt advected out of the vent is
    # the inlet mass minus the stored mass (the air leaves with negligible
    # energy and is excluded from the balance)
    def stored_mass(alpha):
        return RHOM*sum(alpha)*VC

    qout = 0.0
    errors = []
    cont = 0.0
    m_prev = stored_mass(samples[0][4])
    for i in range(1, len(samples)):
        t0, e_prev, tv0, av0, a0 = samples[i - 1]
        t1, e_now, tv1, av1, a1 = samples[i]
        dt = t1 - t0
        m_now = stored_mass(a1)
        dm_out = MDOT*dt - (m_now - m_prev)
        m_prev = m_now
        tavg = 0.5*(tv0 + tv1)
        qout += CVM*max(dm_out, 0.0)*max(tavg - TREF, 0.0)

        qin = MDOT*CVM*(TIN - TREF)*t1
        expected = qin - qout
        de = e_now - e0
        err = abs(de - expected)/max(abs(expected), 1e-30)
        errors.append((t1, de, expected, err))

        if t1 > 0:
            tdir = os.path.join(case_dir, time_dir(t1))
            ci = patch_value(
                read(os.path.join(tdir, "cavity", "T")), "cavity_to_mold")
            mi = patch_value(
                read(os.path.join(tdir, "mold", "T")), "mold_to_cavity")
            cont = max(cont, abs(ci - mi))

        if abs(t1 - 1.0) < 1e-9 or abs(t1 - times[-1]) < 1e-9:
            print("  t = {:g} s: dE = {:8.3f} J, expected = {:8.3f} J, "
                  "imbalance = {:6.3%}, T_vent = {:.2f} K".format(
                      t1, de, expected, err, tv1))

    alpha_final = samples[-1][4]
    fill = sum(alpha_final)/len(alpha_final)
    t_end = samples[-1][0]
    v_in = Q*t_end
    v_store = sum(alpha_final)*VC
    verr = abs(v_store - v_in)/v_in

    print("  max interface continuity error = {:.3e} K".format(cont))
    print("  injected volume   = {:.6e} m^3".format(v_in))
    print("  stored melt volume= {:.6e} m^3 (error {:.3%})".format(
        v_store, verr))

    fail = False
    if cont > 1e-3:
        print("FAIL: interface temperature is not continuous")
        fail = True
    if errors[-1][3] > 0.02:
        print("FAIL: melt + mould energy balance is off by more than 2%")
        fail = True
    if verr > 0.01:
        print("FAIL: the injected melt volume does not match the fill")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: filling CHT conserves energy within 2%, the interface is "
          "continuous and the injected volume matches the fill within 1%")


if __name__ == "__main__":
    main()
