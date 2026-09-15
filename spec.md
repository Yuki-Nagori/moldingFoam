# moldingFoam CAE Specification

Version: 1.0 (2026-09-15)  
Status: implementation-aligned reference for downstream CAE integrations

This document defines the equations, fields, units, solver stages, input keys,
and verification limits exposed by the moldingFoam module suite. A downstream
CAE adapter must use the names and sign conventions here. Material values are
case inputs; the solver does not claim that a generic material is an
experimentally calibrated grade.

## 1. Scope

The suite is an OpenFOAM-14 `foamRun` solver module for injection molding. Its
primary process model is a compressible, non-isothermal two-phase melt/air
calculation with optional pressure-dependent PVT, Cross-WLF viscosity, latent
heat, crystallization, fiber orientation, runner networks, mold heat transfer,
shrinkage, residual stress, and solid displacement.

The normal process sequence is:

1. filling: melt/air interface transport, momentum, pressure, and energy;
2. packing/holding: pressure or process-profile control while the gate remains
   open;
3. gate freeze and cooling/ejection: the gate closes according to the selected
   criterion, heat transfer and optional mold cycles continue, and no further
   melt discharge is allowed through a frozen gate.

Optional models are activated by their dictionaries. An input key that is not
present uses the case/model default; adapters must preserve the case dictionary
instead of assuming every optional field exists.

## 2. Governing fields and units

| Symbol / OpenFOAM field | Meaning | SI unit |
|---|---|---|
| `U` | mixture velocity | m/s |
| `p_rgh` | pressure relative to hydrostatic head | Pa |
| `p` | absolute/thermodynamic pressure when available | Pa |
| `alpha.melt` | melt volume fraction, 0 to 1 | 1 |
| `T` | temperature | K |
| `rho` | density from the active EOS | kg/m3 |
| `mu` | dynamic viscosity | Pa s |
| `D` | solid displacement | m |
| `sigma` | Cauchy stress tensor | Pa |
| `sigmaEq` | von Mises equivalent stress | Pa |
| `S` | shrinkage/free-strain indicator when enabled | 1 |
| `chi` | crystallinity when enabled | 1 |
| `a` | fiber orientation tensor when enabled | 1 |
| `fillTime` | local filling time | s |
| `airTrap` | trapped-air diagnostic | case-defined dimensionless field |

`alpha.melt` is bounded physically to `[0,1]`. Patch fluxes use the outward
patch-normal convention of OpenFOAM. A positive pressure work contribution is
work done by pressure on the control volume under the solver's reported
orientation; downstream energy reports must retain the sign and patch name.

## 3. Fluid equations

The mixture continuity and momentum equations are solved in finite-volume form.
In continuous notation, the model is:

\[
\frac{\partial \rho}{\partial t}+\nabla\cdot(\rho U)=0,
\]

\[
\frac{\partial(\rho U)}{\partial t}
 +\nabla\cdot(\rho U\otimes U)
 =-\nabla p+\nabla\cdot\tau+\rho g+f_\sigma+f_\text{models},
\]

where `tau` is the constitutive viscous stress, `g` is gravity, and model
sources include the selected runner, viscoelastic, shrinkage, and pressure/PVT
couplings. The phase fraction is transported with the bounded interface
scheme:

\[
\frac{\partial\alpha}{\partial t}+\nabla\cdot(\alpha U)
 +\nabla\cdot\bigl(\alpha(1-\alpha)U_c\bigr)=0,
\]

with the compressive interface velocity and limiter controlled by the case's
alpha settings. The exact discrete flux is the OpenFOAM scheme selected in
`fvSchemes`; integrations must compare the same mesh, time step, and limiter.

## 4. Thermodynamics and material laws

### 4.1 PVT / Tait equation of state

The melt specific volume uses the two-domain modified Tait form. For melt (`m`)
or solid (`s`) domain:

\[
v_0(T)=b_1+b_2(T-T_t),\qquad
B(T)=b_3\exp[-b_4T],
\]

\[
v(T,p)=v_0(T)\left[1-C\ln\left(1+\frac{p}{B(T)}\right)\right],
\qquad \rho=1/v.
\]

The transition temperature is `Tt = b5 + b6 p`. The active coefficient set is
selected by the phase/domain rule in the case material dictionary. The solver
checks the admissible logarithm/domain and applies its configured regularization
for extreme pressure; adapters must report the coefficient set and pressure
range used for a run.

### 4.2 Cross-WLF viscosity

The default shear viscosity is represented by a Cross-WLF law:

\[
\mu(\dot\gamma,T,p)=\frac{\mu_0(T,p)}
 {1+[\mu_0\dot\gamma/\tau^*]^{1-n}},
\]

with WLF zero-shear viscosity

\[
\mu_0=D_1\exp\left[-\frac{A_1(T-D_2)}{A_2+T-D_2}\right],
\]

using the pressure shift and coefficient keys supplied by the selected material
model. Exact coefficient names and units must be preserved from the dictionary;
do not substitute a generic PP curve for a grade-specific material.

### 4.3 Energy equation

The thermal equation is written for the selected sensible-energy field `e` or
`h` and includes conduction, convection, pressure work where enabled, viscous
dissipation, and optional latent heat:

\[
\frac{\partial(\rho e)}{\partial t}+\nabla\cdot(\rho Ue)
 =-\nabla\cdot q-p\nabla\cdot U+\Phi+S_\text{latent}+S_\text{models},
\qquad q=-k\nabla T.
\]

`Phi` is viscous dissipation. Latent heat is represented through the active
apparent heat-capacity/enthalpy coupling; it must not be counted a second time
by a downstream energy post-processor. When `energyBudget` is enabled, logs may
report matrix flux and pressure-work diagnostics. A complete global closure
requires storage, boundary heat, pressure work, viscous dissipation, and latent
terms to be integrated with the same time interval and sign convention.

### 4.4 Crystallization and orientation (optional)

Crystallinity `chi` is transported with the melt and advanced by the selected
kinetics model (Nakamura/Avrami parameters). Fiber orientation uses a symmetric
second-order tensor `a`; the active Folgar–Tucker/Jeffery terms and diffusion
coefficient are case-controlled. Valid orientation output must be finite,
approximately positive semidefinite, and have the configured trace convention.

## 5. Mold and runner models

The CHT option solves separate cavity and mold regions with conjugate interface
temperature/flux conditions. A CHT energy report must distinguish interface heat
flux (W/m2), integrated heat rate (W), and accumulated heat (J).

Runner networks solve one-dimensional branch flow with resistance-weighted flow
partition, gate timing, and optional process-profile forcing. A prescribed gate
flow is a branch-local constraint; a process profile on the network total is not
equivalent to prescribing each gate.

## 6. Solid displacement and shrinkage

The optional `solidDisplacement` module solves small-strain linear elasticity:

\[
\nabla\cdot\sigma+f=\rho\,\frac{\partial^2D}{\partial t^2},
\]

\[
\sigma=2\mu\,\operatorname{sym}(\nabla D)
 +\lambda I\,\operatorname{tr}(\nabla D)-3K\alpha_v T I,
\]

with the plane-stress or three-dimensional coefficients selected by
`planeStress`. Anisotropic eigenstrain replaces the isotropic thermal term when
the corresponding model is active. `tractionDisplacement` uses outward normal
traction and pressure; `fixedValue` fixes displacement.

The current structural validation has a known limitation: the OpenFOAM-14
upstream corrector loop reuses its first displacement residual for the loop
condition. Refined 96x160 validation meshes can therefore show insufficient
linear correction and a late displacement jump. The structural 8% acceptance
threshold is not waived; until task 071 is resolved, fine-grid structural output
must be reported as `not_converged` when the static-history check fails.

## 7. Input and output contract

Every case must provide `system/controlDict`, `system/fvSchemes`,
`system/fvSolution`, mesh dictionaries, field dimensions, and the active material
properties. A verifier must check:

- solver `End` marker and output at the declared final time;
- declared nonuniform field length and component count;
- finite values for every reported component;
- units, patch names, outward-normal signs, and physical sampling time;
- static convergence before accepting a structural accuracy number.

For reproducible comparisons, record source commit, OpenFOAM version/architecture,
mesh dimensions, material dictionary hash, time step, solver tolerances, actual
iteration count, wall time, peak RSS when available, and the raw log/artifact
path. Missing values are `unavailable`, never zero.

## 8. Accuracy and calibration status

The suite has passing analytic and process regressions for most fluid, thermal,
pressure, CHT, material, orientation, and cycle features. These are numerical
verification results, not universal material validation. External calibration
requires a grade-specific data source or experiment.

Current structural evidence at the fixed `x=29.5 m` section is:

| Case | Coarse | Medium | Fine |
|---|---:|---:|---:|
| thermoelastic relative error | 10.914% | 5.453% | 57.713% |
| warpagePlate relative error | 3.216% | 2.142% | 11.136% |

The medium thermoelastic and coarse/medium warpage values are within the 8%
criterion. The coarse thermoelastic and fine structural values are not accepted;
the fine values also fail static convergence in the current solver path. These
figures are the present implementation evidence, not a promise for arbitrary
materials, meshes, or process settings.

## 9. Compatibility rule

Downstream CAE integrations should treat unknown dictionary keys as unsupported,
preserve SI units and field names, and reject a run whose verifier returns
`not_converged`, `unavailable`, or non-finite data. This specification is updated
with the task/commit that changes an equation, field, sign convention, or
acceptance criterion.
