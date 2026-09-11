/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | moldingFoam: injection molding solver modules
   \\      /  Website      | https://openfoam.org
    \\  /    A nd          | Copyright (C) 2026 Yuki Lu
     \\/     M anipulation |
-------------------------------------------------------------------------------
License
    This file is part of moldingFoam, an external solver-module distribution
    for OpenFOAM.

    moldingFoam is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    moldingFoam is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with moldingFoam.  If not, see <http://www.gnu.org/licenses/>.

Application
    modelTests

Description
    Repeatable numerical tests for the moldingFoam physical models; the
    process exits non-zero on failure.

    Tait equation of state:
    - p = 0 reduces to vhat = v0(T) on both branches;
    - the analytic psi and alphav match central finite differences of the
      density (relative tolerance 1e-6) in the melt, solid and blended
      transition regions;
    - the density reproduces PVT table points of an HDPE grade.

    CrossWlf viscosity:
    - the zero strain-rate limit approaches eta0(T);
    - the high strain-rate log-log slope approaches (n - 1);
    - hand-computed reference points including the frozen-region
      exponential cap are reproduced;
    - the clamps [etaMin, etaMax] are respected.

    hMelt thermodynamics (latent heat):
    - the latent-Cp peak vanishes at the band edges (C1 continuity);
    - its integral across the transition band equals latentHeat;
    - dHs/dT matches Cp + latentCp;
    - latentHeat = 0 reproduces constant-Cp behaviour.

    Lumped mould thermal state (moldingMoldTemperature update rule):
    - the discrete energy balance holds to machine precision for every
      update;
    - the steady state equals (hFilm*Tfilm + hA*Twater + Q)/(hFilm + hA);
    - an arbitrarily large time step is bounded by the equilibrium;
    - dt -> 0 returns the previous temperature;
    - with no coupling the temperature does not change.

    Vent orifice (restricted-vent back-pressure):
    - zero flow recovers the ambient pressure;
    - the back-pressure is quadratic in the mass flow and follows its
      direction;
    - a hand-computed point is reproduced;
    - zero effective area disables the resistance.

    Coolant channel (1D plug-flow model, moldingCoolantChannel):
    - the power-law Nusselt correlation reproduces a hand-computed point;
    - faces at one axial position form a single well-mixed cross-section;
    - the discrete march reproduces the hand-computed group temperatures
      and conserves the discrete channel energy balance exactly;
    - with many small cross-sections the march converges to the analytic
      plug-flow exponential within 1e-3.

    Shrinkage indicators (moldingShrinkage):
    - the volumetric shrinkage is zero at the reference density and
      follows 1 - rhoRef/rho;
    - the constrained thermal stress indicator follows
      E/(1 - nu) alpha (Tref - T).

    Warpage (moldingWarpage):
    - a linear through-thickness profile gives the classical bimetal
      curvature alpha dT/h with zero membrane strain;
    - a uniform profile gives zero curvature;
    - a symmetric parabolic profile gives zero curvature (symmetry) and
      the analytical membrane strain alpha c h^2/12;
    - the strip deflection and constrained residual stress reproduce the
      hand values.

    Viscoelastic constitutive (moldingViscoelastic):
    - UCM start-up shear reproduces eta0 gammadot (1 - exp(-t/lambda));
    - relaxation from a steady state decays as exp(-t/lambda);
    - the steady first normal stress difference is 2 eta0 lambda
      gammadot^2;
    - the Giesekus term gives shear thinning and a positive N1.

    Fibre orientation (moldingFiberOrientation):
    - the shape factor reproduces (r^2 - 1)/(r^2 + 1);
    - tr(a) is invariant and the advance keeps tr(a) = 1 and the
      eigenvalues in [0, 1];
    - the isotropic state is an equilibrium;
    - the quadratic closure reproduces the Jeffery orbit of a single
      fibre exactly (compared with an independent RK4 integration of the
      Jeffery angle equation).

    Crystallisation kinetics (moldingCrystallization):
    - the Gaussian rate window reproduces K(Tmax) = Kmax and the half
      width K(Tmax +/- W/2) = Kmax/2, with the pressure shift;
    - the exact Avrami step reproduces the isothermal solution
      1 - exp(-(K t)^n) and composes over piecewise-constant stages;
    - chi stays bounded in [0, 1] for arbitrarily large steps and
      dchi/dt vanishes at chi = 1;
    - the latent source returns rho L dchi/dt.

    Runner network (1D pressure-flow-thermal model, moldingRunnerNetwork):
    - the Hagen-Poiseuille and wall-shear-rate helpers reproduce hand
      values;
    - a single feed + gate chain reproduces the exact Hagen-Poiseuille
      pressure drop;
    - parallel branches split by the analytic resistance ratio for a
      constant viscosity and by (D1/D2)^(3 + 1/n) for a power law;
    - the melt temperature follows the analytic exponential along a
      wall-coupled segment and is unchanged along an adiabatic one.

\*---------------------------------------------------------------------------*/

#include "Tait.H"
#include "hMeltThermo.H"
#include "CrossWlf.H"
#include "moldThermalState.H"
#include "moldingCoolantChannel.H"
#include "moldingRunnerNetwork.H"
#include "moldingCrystallization.H"
#include "moldingFiberOrientation.H"
#include "moldingViscoelastic.H"
#include "moldingWarpage.H"
#include "moldingShrinkage.H"
#include "ventOrifice.H"
#include "dictionary.H"
#include "IFstream.H"
#include "IOstreams.H"

using namespace Foam;

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace
{
    label nFailed(0);

    void checkBool(const word& name, bool ok)
    {
        Info<< (ok ? "PASS: " : "FAIL: ") << name << endl;
        if (!ok)
        {
            ++nFailed;
        }
    }

    //- Relative difference |a - b|/max(|b|, small)
    scalar relDiff(scalar a, scalar b)
    {
        return mag(a - b)/max(mag(b), small);
    }

    //- A finite, non-nan scalar
    bool isValidScalar(scalar x)
    {
        return x == x && mag(x) < great;
    }
}


// * * * * * * * * * * * * * * * * * Test Tait  * * * * * * * * * * * * * * //

// HDPE grade Tait coefficients with the smooth transition disabled
static const char* taitDictString0 =
    "specie { molWeight 1; }"
    "equationOfState {"
    "    b1m 1.2495299e-03;"
    "    b2m 1.026e-06;"
    "    b1s 1.070046e-03;"
    "    b2s 2.077e-07;"
    "    b3 1.042149833644e08;"
    "    b4 0.004941;"
    "    b3s 3.3241950281e08;"
    "    b4s 2.46e-06;"
    "    b5 390.65;"
    "    b6 1.543e-07;"
    "    C 0.0894;"
    "    smoothBand 0;"
    "}";

// HDPE grade Tait coefficients (openInjMoldSim dogbone case)
static const char* taitDictString =
    "specie { molWeight 1; }"
    "equationOfState {"
    "    b1m 1.2495299e-03;"
    "    b2m 1.026e-06;"
    "    b1s 1.070046e-03;"
    "    b2s 2.077e-07;"
    "    b3 1.042149833644e08;"
    "    b4 0.004941;"
    "    b3s 3.3241950281e08;"
    "    b4s 2.46e-06;"
    "    b5 390.65;"
    "    b6 1.543e-07;"
    "    C 0.0894;"
    "    smoothBand 0.5;"
    "}";

void taitTests()
{
    IStringStream is(taitDictString);
    dictionary dict(is);

    Tait<specie> tait("melt", dict);

    // p = 0 must reduce to vhat = v0(T) on both branches
    {
        const scalar Tmelt = 500;
        const scalar Tsolid = 350;
        const scalar v0melt =
            1.2495299e-03 + 1.026e-06*(Tmelt - 390.65);
        const scalar v0solid =
            1.070046e-03 + 2.077e-07*(Tsolid - 390.65);

        checkBool
        (
            "Tait: p=0 melt branch gives vhat = v0(T)",
            relDiff(tait.vhat(0, Tmelt), v0melt) < 1e-12
        );
        checkBool
        (
            "Tait: p=0 solid branch gives vhat = v0(T)",
            relDiff(tait.vhat(0, Tsolid), v0solid) < 1e-12
        );
    }

    // Analytic psi and alphav against central finite differences.
    // Finite-difference stencils must not straddle a blending-band edge
    // (the C1 smoothstep has a third-derivative jump there), so points
    // within Tait::smoothBand*0.1 of an edge are skipped.
    {
        const label nP = 7;
        const scalar ps[] = {1e5, 1e6, 1e7, 2e7, 5e7, 8e7, 1.2e8};
        const label nT = 11;
        const scalar Ts[] =
            {340, 360, 380, 389, 390.4, 390.65, 390.9, 392, 400, 450, 500};

        const scalar dp = 1e3;
        const scalar dT = 1e-4;
        const scalar edgeMargin = 0.05;

        const scalar b5(390.65);
        const scalar b6(1.543e-7);
        const scalar band(0.5);

        scalar maxErrPsi(0);
        scalar maxErrAlpha(0);
        label nEvaluated(0);

        for (label ip = 0; ip < nP; ++ip)
        {
            for (label it = 0; it < nT; ++it)
            {
                const scalar p = ps[ip];
                const scalar T = Ts[it];

                // Skip stencils touching a band edge
                bool nearEdge(false);
                for (label ipd = -1; ipd <= 1 && !nearEdge; ++ipd)
                {
                    const scalar Tt = b5 + b6*(p + ipd*dp);
                    nearEdge =
                        min(mag(T - (Tt - band)), mag(T - (Tt + band)))
                      < edgeMargin;
                }
                if (nearEdge)
                {
                    continue;
                }
                ++nEvaluated;

                const scalar psiFD =
                    (tait.rho(p + dp, T) - tait.rho(p - dp, T))/(2*dp);
                const scalar alphaFD =
                    (tait.rho(p, T + dT) - tait.rho(p, T - dT))/(2*dT);

                const scalar psiAnalytic(tait.psi(p, T));
                const scalar alphaAnalytic(-tait.alphav(p, T)*tait.rho(p, T));

                maxErrPsi = max(maxErrPsi, relDiff(psiAnalytic, psiFD));
                maxErrAlpha =
                    max(maxErrAlpha, relDiff(alphaAnalytic, alphaFD));
            }
        }

        Info<< "    evaluated " << nEvaluated << " stencil points" << nl
            << "    max relative error psi    = " << maxErrPsi << nl
            << "    max relative error drho/dT = " << maxErrAlpha << endl;

        checkBool
        (
            "Tait: analytic psi matches finite differences (rtol 1e-6)",
            nEvaluated > (nP*nT)/2 && maxErrPsi < 1e-6
        );
        checkBool
        (
            "Tait: analytic drho/dT matches finite differences (rtol 1e-6)",
            nEvaluated > (nP*nT)/2 && maxErrAlpha < 1e-6
        );
    }

    // PVT table points of the HDPE grade
    {
        struct PVTRow { scalar p; scalar T; scalar rho; };
        const PVTRow rows[] =
        {
            {0,        500, 734.3637435807},
            {5e7,      480, 895.2965123963},
            {1e8,      500, 958.3664094341},
            {1e5,      350, 941.9999163615},
            {5e7,      370, 951.6442476159},
            {1e5,      300, 951.3059743745},
        };

        bool ok(true);
        const label nPvtRows = label(sizeof(rows)/sizeof(PVTRow));
        for (label i = 0; i < nPvtRows; ++i)
        {
            const scalar r(tait.rho(rows[i].p, rows[i].T));
            Info<< "    rho(" << rows[i].p << ", " << rows[i].T
                << ") = " << r << " (expected " << rows[i].rho << ")"
                << endl;
            ok = ok && relDiff(r, rows[i].rho) < 1e-9;
        }

        checkBool("Tait: HDPE PVT table points reproduced", ok);
    }

    // smoothBand = 0: hard solid/melt switch; the derivatives must stay
    // finite (no inf*0) on both branches
    {
        IStringStream is0
        (
            taitDictString0
        );
        dictionary dict0(is0);

        Tait<specie> hard("melt", dict0);

        const label nP = 5;
        const scalar ps[] = {1e5, 1e6, 1e7, 5e7, 1e8};
        const label nT = 6;
        const scalar Ts[] = {340, 360, 380, 400, 450, 500};

        bool ok(true);
        for (label ip = 0; ip < nP; ++ip)
        {
            for (label it = 0; it < nT; ++it)
            {
                const scalar p = ps[ip];
                const scalar T = Ts[it];

                if
                (
                    !isValidScalar(hard.rho(p, T))
                 || !isValidScalar(hard.psi(p, T))
                 || !isValidScalar(hard.alphav(p, T))
                 || !isValidScalar(hard.CpMCv(p, T))
                )
                {
                    ok = false;
                }

                const scalar dp = 1e3;
                const scalar dT = 1e-4;
                const scalar psiFD =
                    (hard.rho(p + dp, T) - hard.rho(p - dp, T))/(2*dp);
                const scalar alphaFD =
                    (hard.rho(p, T + dT) - hard.rho(p, T - dT))/(2*dT);

                ok = ok
                  && relDiff(hard.psi(p, T), psiFD) < 1e-6
                  && relDiff
                     (
                        -hard.alphav(p, T)*hard.rho(p, T),
                        alphaFD
                     ) < 1e-6;
            }
        }

        checkBool
        (
            "Tait: smoothBand 0 derivatives finite and match FD (rtol 1e-6)",
            ok
        );
    }
}


// * * * * * * * * * * * * * * * * Test CrossWlf  * * * * * * * * * * * * * * //

// HDPE grade Cross-WLF coefficients (openInjMoldSim dogbone case)
static const char* crossWlfDictString =
    "n 0.393539;"
    "tauStar 64568.9;"
    "D1 3.76174e15;"
    "D2 153.15;"
    "D3 0;"
    "A1 33.21;"
    "A2 51.6;"
    "etaMin 5;"
    "etaMax 1e6;"
    "gammaDotMin 1e-06;";

// * * * * * * * * * * * * Test latent heat (hMelt)  * * * * * * * * * * * //

void latentHeatTests()
{
    IStringStream is(taitDictString);
    dictionary dict(is);

    typedef hMeltThermo<Tait<specie>> hMelt;

    const scalar Cp0 = 2400;
    const scalar L = 2e5;

    hMelt thermo(Tait<specie>("melt", dict), Cp0, 0, L, Tstd, 0);

    const scalar p = 1e6;
    const scalar band = 0.5;
    const scalar Tt = thermo.Tt(p);

    // The latent-Cp peak vanishes at the band edges (C1 continuity) and
    // the sensible Cp stays base-only (transport consistency)
    checkBool
    (
        "hMelt: latent-Cp peak vanishes at the band edges",
        thermo.latentCp(p, Tt - band) == 0
     && thermo.latentCp(p, Tt + band) == 0
     && thermo.Cp(p, Tt) == Cp0
    );

    // Peak of dw/dT is 1.5/(2*band) at the band centre; the energy-
    // equation Cv carries the peak on top of the base capacity
    checkBool
    (
        "hMelt: apparent-Cv peak reproduced at the band centre",
        relDiff(thermo.latentCp(p, Tt), L*1.5/(2*band)) < 1e-12
     && relDiff
        (
            thermo.Cv(p, Tt),
            Cp0 + L*1.5/(2*band) - thermo.CpMCv(p, Tt)
        ) < 1e-12
    );

    // The integral of the latent peak across the band is exactly the
    // latent heat: hs(Tt + band) - hs(Tt - band) = Cp0*2*band + L
    {
        const scalar dhs =
            thermo.hs(p, Tt + band) - thermo.hs(p, Tt - band);
        const scalar expected = Cp0*2*band + L;
        checkBool
        (
            "hMelt: enthalpy absorbs the full latent heat across the band",
            relDiff(dhs, expected) < 1e-12
        );
    }

    // dHs/dT matches the apparent Cp inside the band. The apparent Cp is
    // C1 but not C2 across the band edges, so the central difference of
    // hs carries a truncation error proportional to h^2 times the
    // (large) third derivative of the latent peak: the step is kept
    // small enough that the reference stays accurate to ~1e-9 relative
    // (sampling stops 0.05 K short of the edges)
    {
        bool ok = true;
        scalar maxErr(0);
        const scalar h = 1e-5;
        for
        (
            scalar T = Tt - band + 0.05;
            T <= Tt + band - 0.05 + small;
            T += 0.05
        )
        {
            const scalar dhsFD =
                (thermo.hs(p, T + h) - thermo.hs(p, T - h))/(2*h);
            const scalar err
            (
                relDiff(dhsFD, thermo.Cp(p, T) + thermo.latentCp(p, T))
            );
            maxErr = max(maxErr, err);
            ok = ok && err < 1e-8;
        }
        Info<< "    max relative error dHs/dT = " << maxErr << nl;
        checkBool("hMelt: dHs/dT matches Cp + latentCp", ok);
    }

    // latentHeat = 0 reproduces constant-Cp behaviour
    {
        hMelt plain(Tait<specie>("melt", dict), Cp0, 0, 0, Tstd, 0);

        checkBool
        (
            "hMelt: latentHeat = 0 reproduces constant-Cp behaviour",
            plain.Cp(p, 480) == Cp0
         && plain.latentCp(p, 480) == 0
         && plain.hs(p, 480) == Cp0*(480 - Tstd)
        );
    }
}


void moldThermalTests()
{
    // Parameter set representative of the contract case after the V/P
    // switch: a 500 J/K insert with a fast casting-side film
    const scalar C = 500;        // J/K
    const scalar hA = 5.4;       // W/K
    const scalar Tw = 300;       // K
    const scalar T0 = 353;       // K
    const scalar hFilm = 3.8e7;  // W/K
    const scalar Tfilm = 354;    // K
    const scalar dt = 2.6e-4;    // s

    // The update must satisfy the discrete energy balance
    //   C(Tnew - Told) = dt (hFilm(Tfilm - Tnew) + hA(Tw - Tnew) + Q)
    // to machine precision (this is the conservation property that the
    // energy equation's boundary flux relies on)
    {
        const scalar Q = 0;
        const scalar Tnew =
            moldThermalState::Tnew(C, hA, Tw, T0, hFilm, Tfilm, dt, Q);

        const scalar lhs = C*(Tnew - T0);
        const scalar rhs =
            dt*(hFilm*(Tfilm - Tnew) + hA*(Tw - Tnew) + Q);

        Info<< "    C*(Tnew-Told) = " << lhs
            << " J, dt*q = " << rhs << " J" << endl;

        checkBool
        (
            "moldThermalState: discrete energy balance holds",
            relDiff(lhs, rhs) < 1e-12
        );

        const scalar Q2 = 1200;
        const scalar TnewQ =
            moldThermalState::Tnew(C, hA, Tw, T0, hFilm, Tfilm, dt, Q2);
        const scalar lhsQ = C*(TnewQ - T0);
        const scalar rhsQ =
            dt*(hFilm*(Tfilm - TnewQ) + hA*(Tw - TnewQ) + Q2);

        checkBool
        (
            "moldThermalState: discrete energy balance holds with a source",
            relDiff(lhsQ, rhsQ) < 1e-12
        );
    }

    // Steady state: repeated updates converge to the conductance-weighted
    // mean of the casting-side and water-side driving temperatures
    {
        const scalar Teq = (hFilm*Tfilm + hA*Tw)/(hFilm + hA);

        scalar T = T0;
        for (label i = 0; i < 50; ++i)
        {
            T = moldThermalState::Tnew(C, hA, Tw, T, hFilm, Tfilm, dt);
        }

        Info<< "    steady T = " << T << " (expected " << Teq << ")" << endl;

        checkBool
        (
            "moldThermalState: steady state is the weighted mean",
            relDiff(T, Teq) < 1e-12
        );
    }

    // Unconditionally stable and non-overshooting: a time step far longer
    // than the coupling time constant lands exactly at the equilibrium
    {
        const scalar Teq = (hFilm*Tfilm + hA*Tw)/(hFilm + hA);
        const scalar T = moldThermalState::Tnew(C, hA, Tw, T0, hFilm, Tfilm, 1e6);

        checkBool
        (
            "moldThermalState: large dt lands at the equilibrium",
            relDiff(T, Teq) < 1e-12
        );
    }

    // dt -> 0 returns the previous temperature
    {
        const scalar T =
            moldThermalState::Tnew(C, hA, Tw, T0, hFilm, Tfilm, 1e-15);

        checkBool
        (
            "moldThermalState: dt -> 0 returns the previous temperature",
            relDiff(T, T0) < 1e-12
        );
    }

    // No coupling: the temperature must not change
    {
        const scalar T = moldThermalState::Tnew(C, 0, Tw, T0, 0, Tfilm, dt);

        checkBool
        (
            "moldThermalState: uncoupled mass keeps its temperature",
            relDiff(T, T0) < 1e-14
        );
    }

    // Combined conductive paths: the driving temperature is the
    // conductance-weighted mean of the two path temperatures
    {
        const scalar expected = (100.0*300.0 + 300.0*400.0)/400.0;

        checkBool
        (
            "moldThermalState: Tdrv is the conductance-weighted mean",
            relDiff(moldThermalState::Tdrv(100, 300, 300, 400), expected)
          < 1e-12
        );
        checkBool
        (
            "moldThermalState: Tdrv with zero conductances returns Ta",
            moldThermalState::Tdrv(0, 350, 0, 400) == 350
        );
    }
}


void ventOrificeTests()
{
    const scalar p0 = 1e5;   // Pa
    const scalar rho = 1.2;  // kg/m^3
    const scalar CdA = 1e-6; // m^2

    // Zero flow recovers the ambient pressure
    checkBool
    (
        "ventOrifice: zero flow gives the ambient pressure",
        ventOrifice::pVent(p0, rho, 0, CdA) == p0
    );

    // Quadratic in the mass flow, with the excess following the flow
    // direction (outward positive, inward negative)
    {
        const scalar m = 1e-4;
        const scalar dp1 = ventOrifice::pVent(p0, rho, m, CdA) - p0;
        const scalar dp2 = ventOrifice::pVent(p0, rho, 2*m, CdA) - p0;
        const scalar dpm = ventOrifice::pVent(p0, rho, -m, CdA) - p0;

        checkBool
        (
            "ventOrifice: back-pressure quadratic in the mass flow",
            relDiff(dp2, 4*dp1) < 1e-12
        );
        checkBool
        (
            "ventOrifice: pressure excess follows the flow direction",
            dp1 > 0 && dpm < 0 && relDiff(dpm, -dp1) < 1e-12
        );
    }

    // Hand value: dp = m^2/(2*rho*CdA^2)
    {
        const scalar m = 1e-4;
        const scalar expected = p0 + m*m/(2*rho*CdA*CdA);

        Info<< "    pVent(m=1e-4) = "
            << ventOrifice::pVent(p0, rho, m, CdA)
            << " (expected " << expected << ")" << endl;

        checkBool
        (
            "ventOrifice: hand-computed back-pressure",
            relDiff(ventOrifice::pVent(p0, rho, m, CdA), expected) < 1e-12
        );
    }

    // Zero area disables the resistance (fully open vent)
    checkBool
    (
        "ventOrifice: zero CdA leaves the pressure at p0",
        ventOrifice::pVent(p0, rho, 1e-3, 0) == p0
    );
}


void crossWlfTests()
{
    IStringStream is(crossWlfDictString);
    dictionary coeffsDict(is);

    const scalar n(0.393539);
    const scalar D1(3.76174e15);
    const scalar D2(153.15);
    const scalar A1(33.21);
    const scalar A2(51.6);
    const scalar etaMax(1e6);

    // Zero strain-rate limit approaches eta0(T)
    {
        const scalar T(450);
        const scalar eta0 =
            D1*std::exp(-A1*(T - D2)/(A2 + T - D2));
        const scalar eta =
            laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
            (
                coeffsDict, 1e5, T, 1e-6
            );

        Info<< "    eta(gammaDot -> 0) = " << eta
            << ", eta0(450 K) = " << eta0 << endl;

        checkBool
        (
            "CrossWlf: gammaDot -> 0 gives eta -> eta0(T) (rtol 1e-4)",
            relDiff(eta, eta0) < 1e-4
        );
    }

    // High strain-rate log-log slope approaches (n - 1)
    {
        const scalar etaLo =
            laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
            (
                coeffsDict, 1e5, 480, 1e4
            );
        const scalar etaHi =
            laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
            (
                coeffsDict, 1e5, 480, 1e5
            );

        const scalar slope =
            (std::log(etaHi) - std::log(etaLo))/(std::log(1e5) - std::log(1e4));

        Info<< "    log-log slope = " << slope
            << " (n - 1 = " << n - 1 << ")" << endl;

        checkBool
        (
            "CrossWlf: high-shear log-log slope approaches n - 1",
            mag(slope - (n - 1)) < 0.02
        );
    }

    // Hand-computed reference points, including the frozen region
    {
        struct Row { scalar p; scalar T; scalar gammaDot; scalar eta; };
        const Row rows[] =
        {
            {1e5,  480, 100,   5.1788325207e+02},
            {1e5,  480, 1000,  1.8213918253e+02},
            {5e7,  500, 10000, 4.5755867559e+01},
            {1e5,  160, 1e-6,  9.9879112032e+05},
            {1e5,  450, 5000,  8.8631897369e+01},
            {1e5,  450, 1e-6,  1.9420756884e+03},
        };

        bool ok(true);
        const label nCwRows = label(sizeof(rows)/sizeof(Row));
        for (label i = 0; i < nCwRows; ++i)
        {
            const scalar eta =
                laminarModels::generalisedNewtonianViscosityModels::CrossWlf::
                eta
                (
                    coeffsDict, rows[i].p, rows[i].T, rows[i].gammaDot
                );
            Info<< "    eta(" << rows[i].p << ", " << rows[i].T << ", "
                << rows[i].gammaDot << ") = " << eta
                << " (expected " << rows[i].eta << ")" << endl;
            ok = ok && relDiff(eta, rows[i].eta) < 1e-9;
        }

        checkBool("CrossWlf: hand-computed reference points reproduced", ok);
    }

    // Viscosity clamps
    {
        const scalar etaFrozen =
            laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
            (
                coeffsDict, 1e5, 140, 1e-6
            );
        const scalar etaThin =
            laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
            (
                coeffsDict, 1e5, 700, 1e7
            );

        Info<< "    eta(frozen) = " << etaFrozen
            << ", eta(thin, high shear) = " << etaThin << endl;

        checkBool
        (
            "CrossWlf: eta clamped to [etaMin, etaMax]",
            etaFrozen <= etaMax && etaThin >= 5
        );
    }
}


void coolantChannelTests()
{
    // Power-law Nusselt correlation and the HTC: Dittus-Boelter-like
    // Nu = 0.023 Re^0.8 Pr^0.4 at Re = 1e4, Pr = 7 (hand value)
    {
        const scalar Nu =
            moldingCoolantChannel::Nu(0.023, 0.8, 0.4, 1e4, 7.0);

        Info<< "    Nu(0.023, 0.8, 0.4, 1e4, 7) = " << Nu << endl;

        checkBool
        (
            "coolantChannel: hand-computed Nusselt number",
            relDiff(Nu, 79.39022851754193) < 1e-10
        );

        checkBool
        (
            "coolantChannel: h = Nu k / D",
            relDiff(moldingCoolantChannel::htcFromNu(100, 0.6, 0.01), 6000)
         < 1e-12
        );
    }

    // Faces sharing an axial position form one well-mixed cross-section
    // whose conductances add up
    {
        List<scalar> magSf(4, 1.0);
        List<scalar> proj(4);
        proj[0] = 0;
        proj[1] = 0;
        proj[2] = 0.01;
        proj[3] = 0.01;

        List<scalar> groupHA;
        labelList groupStart;
        moldingCoolantChannel::group
        (
            magSf,
            proj,
            2.0,
            1e-9,
            groupHA,
            groupStart
        );

        checkBool
        (
            "coolantChannel: equal positions form one cross-section",
            groupHA.size() == 2
         && relDiff(groupHA[0], 4.0) < 1e-14
         && relDiff(groupHA[1], 4.0) < 1e-14
         && groupStart[0] == 0
         && groupStart[1] == 2
        );
    }

    // Exact discrete march plus the hand-computed group temperatures and
    // the channel energy balance
    {
        List<scalar> groupHA(3, 50.0);
        List<scalar> groupTw(3, 350.0);
        List<scalar> groupTc;

        const scalar hATc = moldingCoolantChannel::march
        (
            300,
            1000,
            groupHA,
            groupTw,
            groupTc
        );

        Info<< "    group inlet temperatures = " << groupTc << endl;

        checkBool
        (
            "coolantChannel: discrete march matches the hand solution",
            relDiff(groupTc[0], 300.0) < 1e-14
         && relDiff(groupTc[1], 302.5) < 1e-14
         && relDiff(groupTc[2], 304.875) < 1e-14
         && relDiff(hATc, 50.0*(300.0 + 302.5 + 304.875)) < 1e-14
        );

        const scalar Tout =
            groupTc[2] + 50.0*(350.0 - groupTc[2])/1000.0;

        const scalar q =
            50.0*(350.0 - groupTc[0])
          + 50.0*(350.0 - groupTc[1])
          + 50.0*(350.0 - groupTc[2]);

        const scalar expected = 1000.0*(Tout - 300.0);

        Info<< "    channel pick-up = " << q
            << " W (mdotCp dT = " << expected << " W)" << endl;

        checkBool
        (
            "coolantChannel: channel energy balance holds",
            relDiff(q, expected) < 1e-12
        );
    }

    // Many small cross-sections converge to the analytic plug-flow
    // exponential T(x) = Ts - (Ts - Tin) exp(-hA x/(mdot Cp))
    {
        const label n = 200;
        List<scalar> groupHA(n, 1000.0/n);   // total NTU = 1
        List<scalar> groupTw(n, 350.0);
        List<scalar> groupTc;

        moldingCoolantChannel::march(300, 1000, groupHA, groupTw, groupTc);

        const scalar Tdisc =
            groupTc[n - 1]
          + groupHA[n - 1]*(350.0 - groupTc[n - 1])/1000.0;
        const scalar Tanalytic = 350.0 - 50.0*std::exp(-1.0);

        Info<< "    discrete outlet = " << Tdisc
            << " K (analytic " << Tanalytic << " K)" << endl;

        checkBool
        (
            "coolantChannel: march matches the analytic exponential "
            "(rtol < 1e-3)",
            relDiff(Tdisc, Tanalytic) < 1e-3
        );
    }
}



const char* runnerConstantDictString = R"(
inletTemperature 480;
cp 2400;
rho 800;
viscosity { type constant; mu 100; }
feed { length 0.1; diameter 0.006; }
gates { g1 { length 0.02; diameter 0.004; } }
)";

const char* runnerConstantTwoGatesDictString = R"(
inletTemperature 480;
cp 2400;
rho 800;
viscosity { type constant; mu 100; }
feed { length 0.1; diameter 0.006; }
gates
{
    g1 { length 0.02; diameter 0.004; }
    g2 { length 0.02; diameter 0.002; }
}
)";

const char* runnerPowerLawTwoGatesDictString = R"(
inletTemperature 480;
cp 2400;
rho 800;
viscosity { type powerLaw; K 1e4; n 0.5; }
feed { length 0.1; diameter 0.006; }
gates
{
    g1 { length 0.02; diameter 0.004; }
    g2 { length 0.02; diameter 0.002; }
}
)";

const char* runnerCooledDictString = R"(
inletTemperature 480;
cp 2400;
rho 800;
viscosity { type constant; mu 100; }
feed { length 0.1; diameter 0.006; wallTemperature 500; htc 2000; }
gates { g1 { length 0.02; diameter 0.004; } }
)";


void runnerNetworkTests()
{
    // Hagen-Poiseuille and wall-shear-rate helpers (hand values)
    {
        const scalar dp =
            moldingRunnerNetwork::hagenPoiseuille(100, 0.1, 0.006, 3e-6);
        const scalar gd = moldingRunnerNetwork::shearRate(3e-6, 0.006);

        Info<< "    HP(100, 0.1, 0.006, 3e-6) = " << dp
            << " Pa, shearRate = " << gd << " 1/s" << endl;

        checkBool
        (
            "runnerNetwork: Hagen-Poiseuille hand value",
            relDiff(dp, 943140.4035075279) < 1e-12
        );
        checkBool
        (
            "runnerNetwork: wall shear rate hand value",
            relDiff(gd, 141.4710605261292) < 1e-12
        );
    }

    // A single feed + gate chain reproduces the exact Hagen-Poiseuille
    // pressure drop of the two segments in series
    {
        IStringStream is(runnerConstantDictString);
        dictionary dict(is);
        moldingRunnerNetwork network(dict);

        const scalar Q = 3e-6;
        const scalar expected =
            moldingRunnerNetwork::hagenPoiseuille(100, 0.1, 0.006, Q)
          + moldingRunnerNetwork::hagenPoiseuille(100, 0.02, 0.004, Q);

        Info<< "    series dp = " << network.pressureDrop(Q)
            << " Pa (expected " << expected << " Pa)" << endl;

        checkBool
        (
            "runnerNetwork: series chain reproduces Hagen-Poiseuille",
            relDiff(network.pressureDrop(Q), expected) < 1e-12
         && relDiff(network.gateFlow(0, Q), Q) < 1e-12
        );
    }

    // Parallel branches split by the analytic resistance ratio
    {
        IStringStream is(runnerConstantTwoGatesDictString);
        dictionary dict(is);
        moldingRunnerNetwork network(dict);

        const scalar Q = 3e-6;
        const scalar Q1 = network.gateFlow(0, Q);
        const scalar Q2 = network.gateFlow(1, Q);

        Info<< "    constant split Q1/Q2 = " << Q1/Q2
            << " (expected 16), Q1 = " << Q1 << " m^3/s" << endl;

        checkBool
        (
            "runnerNetwork: constant-viscosity split by resistance ratio",
            relDiff(Q1/Q2, 16.0) < 1e-9
         && relDiff(Q1 + Q2, Q) < 1e-12
         && relDiff(Q1, 2.823529411764706e-06) < 1e-9
        );
    }

    // Power-law parallel split: Qi/Qj = (Dj/Di)^(3 + 1/n)
    {
        IStringStream is(runnerPowerLawTwoGatesDictString);
        dictionary dict(is);
        moldingRunnerNetwork network(dict);

        const scalar Q = 3e-6;
        const scalar Q1 = network.gateFlow(0, Q);
        const scalar Q2 = network.gateFlow(1, Q);

        Info<< "    power-law split Q1/Q2 = " << Q1/Q2
            << " (expected 32)" << endl;

        checkBool
        (
            "runnerNetwork: power-law split follows (D1/D2)^(3 + 1/n)",
            relDiff(Q1/Q2, 32.0) < 1e-6
         && relDiff(Q1 + Q2, Q) < 1e-12
        );
    }

    // Melt temperature: adiabatic segments leave it unchanged, a
    // wall-coupled feed follows the analytic exponential
    {
        IStringStream is(runnerCooledDictString);
        dictionary dict(is);
        moldingRunnerNetwork network(dict);

        const scalar Q = 3e-6;
        const scalar expected = 489.6059471207037;

        Info<< "    cooled gate temperature = "
            << network.gateTemperature(0, Q)
            << " K (expected " << expected << " K)" << endl;

        checkBool
        (
            "runnerNetwork: wall-coupled melt temperature exponential",
            relDiff(network.gateTemperature(0, Q), expected) < 1e-12
        );
    }
}


const char* crystallizationDictString = R"(
avramiExponent 2;
rateConstant 0.1;
peakTemperature 400;
windowWidth 40;
peakTemperaturePressureShift 1e-7;
latentHeat 2e5;
rho 800;
)";


void crystallizationTests()
{
    IStringStream is(crystallizationDictString);
    dictionary dict(is);
    moldingCrystallization xtal(dict);

    // Gaussian rate window and the pressure shift
    {
        const scalar K0 = xtal.K(400, 0);
        const scalar K1 = xtal.K(420, 0);
        const scalar K2 = xtal.K(380, 0);

        Info<< "    K(400) = " << K0 << ", K(420) = " << K1
            << ", Tmax(1e7 Pa) = " << xtal.Tmax(1e7) << " K" << endl;

        checkBool
        (
            "crystallization: rate window peaks at Kmax with half width W/2",
            relDiff(K0, 0.1) < 1e-14
         && relDiff(K1, 0.05) < 1e-14
         && relDiff(K2, 0.05) < 1e-14
         && relDiff(xtal.Tmax(1e7), 401.0) < 1e-12
        );
    }

    // Isothermal exact Avrami step: chi = 1 - exp(-(K t)^n)
    {
        const scalar chi = xtal.advance(0, 400, 0, 5.0);

        Info<< "    advance(0, 400, 0, 5) = " << chi
            << " (expected 0.22119921692859512)" << endl;

        checkBool
        (
            "crystallization: exact isothermal Avrami step",
            relDiff(chi, 0.22119921692859512) < 1e-12
        );
    }

    // Piecewise-constant stages compose through the equivalent time:
    // 1 - exp(-(K1 t1 + K2 t2)^n)
    {
        const scalar chi =
            xtal.advance(xtal.advance(0, 400, 0, 3.0), 420, 0, 4.0);

        checkBool
        (
            "crystallization: piecewise stages compose",
            relDiff(chi, 0.22119921692859512) < 1e-12
        );
    }

    // Boundedness and the vanishing rate at chi = 1
    {
        const scalar chi = xtal.advance(0.5, 400, 0, 1e6);

        checkBool
        (
            "crystallization: chi stays in [0, 1]",
            chi >= 0 && chi <= 1 && relDiff(chi, 1.0) < 1e-12
         && xtal.dchiDt(1, 400, 0) == 0
         && xtal.advance(1, 400, 0, 10) == 1
        );
    }

    // Latent source rho L dchi/dt
    {
        const scalar q = xtal.latentSource(0, 400, 0, 5.0);

        Info<< "    latent source = " << q << " W/m^3" << endl;

        checkBool
        (
            "crystallization: latent source is rho L dchi/dt",
            relDiff(q, 7078374.941715044) < 1e-10
         && xtal.latentSource(1, 400, 0, 5.0) == 0
        );
    }
}



const char* fiberDictString = R"(
aspectRatio 3;
interactionCoefficient 0.01;
closure quadratic;
)";

const char* fiberSphereDictString = R"(
aspectRatio 1;
interactionCoefficient 0.01;
closure quadratic;
)";

const char* fiberJefferyDictString = R"(
aspectRatio 3;
interactionCoefficient 0;
closure quadratic;
)";


void fiberOrientationTests()
{
    IStringStream is(fiberDictString);
    dictionary dict(is);
    moldingFiberOrientation fibers(dict);

    const scalar lambda = 0.8;
    const scalar gammaDot = 1.0;
    const scalar theta0 = 0.3;

    tensor gradU(tensor::zero);
    gradU(0, 1) = gammaDot;

    // Shape factor
    checkBool
    (
        "fiberOrientation: shape factor (r^2 - 1)/(r^2 + 1)",
        relDiff(moldingFiberOrientation::shapeFactor(3), 0.8) < 1e-14
     && moldingFiberOrientation::shapeFactor(1) == 0
     && relDiff(fibers.lambda(), lambda) < 1e-14
    );

    // tr(a) is an invariant of the evolution; the advance keeps it at 1
    {
        const symmTensor a0(0.6, 0.2, 0, 0.4, 0, 0);
        const symmTensor da(fibers.dadt(a0, gradU));
        const symmTensor a1(fibers.advance(a0, gradU, 1e-3));

        Info<< "    tr(da/dt) = " << tr(da) << ", tr(a_new) = " << tr(a1)
            << endl;

        checkBool
        (
            "fiberOrientation: trace invariance and normalisation",
            mag(tr(da)) < 1e-14 && relDiff(tr(a1), 1.0) < 1e-14
        );
    }

    // Isotropic state is an equilibrium for spheres (lambda = 0): the
    // vorticity and closure terms vanish and the diffusion term is zero
    {
        IStringStream isSphere(fiberSphereDictString);
        dictionary sphereDict(isSphere);
        moldingFiberOrientation spheres(sphereDict);

        const symmTensor iso(symmTensor::I/3);
        const symmTensor da(spheres.dadt(iso, gradU));

        checkBool
        (
            "fiberOrientation: isotropic state is an equilibrium for "
            "spheres",
            mag(da.xx()) < 1e-14 && mag(da.xy()) < 1e-14
         && mag(da.yy()) < 1e-14
        );
    }

    // Boundedness under shear: eigenvalues in [0, 1], trace 1
    {
        symmTensor a(symmTensor::I/3);

        for (label i = 0; i < 20000; ++i)
        {
            a = fibers.advance(a, gradU, 1e-4);
        }

        const vector eigs(eigenValues(a));

        Info<< "    eigenvalues after shear = " << eigs << endl;

        checkBool
        (
            "fiberOrientation: orientation tensor stays bounded",
            relDiff(tr(a), 1.0) < 1e-12
         && eigs.x() > -1e-9 && eigs.z() < 1 + 1e-9
        );
    }

    // Single-fibre Jeffery orbit: the quadratic closure is exact for a
    // rank-one tensor, so a must follow a = n n with the Jeffery angle
    // equation dtheta/dt = (gdot/2)((lambda-1)cos^2 - (1+lambda)sin^2)
    {
        IStringStream isJeffery(fiberJefferyDictString);
        dictionary jefferyDict(isJeffery);
        moldingFiberOrientation jeffery(jefferyDict);

        const scalar c0(std::cos(theta0));
        const scalar s0(std::sin(theta0));
        const symmTensor a0(c0*c0, c0*s0, 0, s0*s0, 0, 0);

        // Jeffery angle equation, RK4 with a fine step
        const scalar dtRef = 1e-6;
        const label nRef = 200000;
        const scalar dt = 1e-5;
        const label n = nRef*dtRef/dt;

        scalar theta = theta0;
        symmTensor a = a0;

        const auto dtheta = [&](const scalar th)
        {
            const scalar c(std::cos(th));
            const scalar s(std::sin(th));

            return 0.5*gammaDot*((lambda - 1)*c*c - (1 + lambda)*s*s);
        };

        scalar maxErr = 0;

        for (label step = 0; step < n; ++step)
        {
            // RK4 reference
            for (label r = 0; r < label(dt/dtRef); ++r)
            {
                const scalar k1 = dtheta(theta);
                const scalar k2 = dtheta(theta + 0.5*dtRef*k1);
                const scalar k3 = dtheta(theta + 0.5*dtRef*k2);
                const scalar k4 = dtheta(theta + dtRef*k3);

                theta += dtRef*(k1 + 2*k2 + 2*k3 + k4)/6;
            }

            a = jeffery.advance(a, gradU, dt);

            const scalar c(std::cos(theta));
            const scalar s(std::sin(theta));

            maxErr = max(maxErr, mag(a.xx() - c*c));
            maxErr = max(maxErr, mag(a.xy() - c*s));
            maxErr = max(maxErr, mag(a.yy() - s*s));
        }

        Info<< "    max Jeffery orbit error = " << maxErr << endl;

        checkBool
        (
            "fiberOrientation: quadratic closure reproduces the Jeffery "
            "orbit",
            maxErr < 1e-6
        );
    }
}



const char* shrinkageDictString = R"(
referenceDensity 950;
referenceTemperature 293.15;
elasticModulus 2e9;
poissonRatio 0.3;
thermalExpansion 7e-5;
)";


void shrinkageTests()
{
    IStringStream is(shrinkageDictString);
    dictionary dict(is);
    moldingShrinkage shrink(dict);

    // Volumetric shrinkage
    {
        const scalar S0 = shrink.volumetricShrinkage(950);
        const scalar S1 = shrink.volumetricShrinkage(1.05*950);

        Info<< "    S(rhoRef) = " << S0 << ", S(1.05 rhoRef) = " << S1
            << endl;

        checkBool
        (
            "shrinkage: zero at the reference density and 1 - rhoRef/rho",
            mag(S0) < 1e-14
         && relDiff(S1, 1 - 1.0/1.05) < 1e-12
         && shrink.volumetricShrinkage(0.95*950) < 0
        );
    }

    // Thermal stress indicator
    {
        const scalar sigma = shrink.thermalStress(373);
        const scalar expected = 2e9/(1 - 0.3)*7e-5*(293.15 - 373);

        Info<< "    sigma(373 K) = " << sigma << " Pa" << endl;

        checkBool
        (
            "shrinkage: constrained thermal stress indicator",
            relDiff(sigma, expected) < 1e-12
         && sigma < 0
        );
    }

    // Void (cavitation) fraction of a sealed melt (task 018a, stage 1):
    // an isochore sealed at 40 MPa and 480 K develops voids once cooling
    // raises rho(pv, T) above the sealed density
    {
        IStringStream tis(taitDictString);
        dictionary tdict(tis);
        Tait<specie> tait("melt", tdict);

        const scalar p0 = 4e7;
        const scalar T0 = 480;
        const scalar T1 = 350;
        const scalar rhoSealed = tait.rho(p0, T0);

        const scalar void0 = shrink.voidFraction(rhoSealed, tait.rho(0, T0));
        const scalar void1 = shrink.voidFraction(rhoSealed, tait.rho(0, T1));
        const scalar expected1 = 1 - rhoSealed/tait.rho(0, T1);

        Info<< "    void fraction, sealed at (40 MPa, 480 K): T=480 K "
            << void0 << ", T=350 K " << void1 << endl;

        checkBool
        (
            "shrinkage: no void while the sealed melt stays compressed, "
            "void appears as rho(pv, T) exceeds the sealed density",
            void0 < 1e-14
         && relDiff(void1, expected1) < 1e-12
         && void1 > 1e-3
         && shrink.voidFraction(rhoSealed, 0.5*rhoSealed) < 1e-14
        );
    }

    // Crystallinity coupling (task 034): the shrinkage gains
    // chiShrinkage per unit relative crystallinity
    {
        IStringStream cis(
            "referenceDensity 950;"
            "referenceTemperature 293.15;"
            "elasticModulus 2e9;"
            "poissonRatio 0.3;"
            "thermalExpansion 7e-5;"
            "crystallinityShrinkage 0.02;"
        );
        dictionary cdict(cis);
        moldingShrinkage cshrink(cdict);

        const scalar S0 = cshrink.volumetricShrinkage(950, 0);
        const scalar S1 = cshrink.volumetricShrinkage(950, 1);

        Info<< "    shrinkage with crystallinity: S(chi=0) = " << S0
            << ", S(chi=1) = " << S1 << endl;

        checkBool
        (
            "shrinkage: the crystallinity coupling adds chiShrinkage per "
            "unit chi and leaves the default case unchanged",
            mag(S0) < 1e-14
         && relDiff(S1 - S0, 0.02) < 1e-12
         && relDiff(cshrink.chiShrinkage(), 0.02) < 1e-12
         && mag(shrink.chiShrinkage()) < 1e-14
        );
    }

    // The cavitation pressure is optional and defaults to vacuum
    {
        IStringStream vis(
            "referenceDensity 950;"
            "referenceTemperature 293.15;"
            "elasticModulus 2e9;"
            "poissonRatio 0.3;"
            "thermalExpansion 7e-5;"
            "voidPressure 1e5;"
        );
        dictionary vdict(vis);
        moldingShrinkage vshrink(vdict);

        checkBool
        (
            "shrinkage: cavitation pressure defaults to vacuum and reads "
            "the optional voidPressure",
            mag(shrink.pv()) < 1e-14
         && relDiff(vshrink.pv(), 1e5) < 1e-12
        );
    }
}



const char* crossWlfPressureDictString = R"(
n 0.393539;
tauStar 64568.9;
D1 3.76174e15;
D2 153.15;
D3 1e-7;
A1 33.21;
A2 51.6;
etaMin 5;
etaMax 1e6;
gammaDotMin 1e-6;
)";


void pressureDependentViscosityTests()
{
    // CrossWlf pressure shift TStar = D2 + D3 p: the viscosity must grow
    // with pressure, and D3 = 0 must leave it pressure-independent
    IStringStream is(crossWlfPressureDictString);
    dictionary dict(is);

    const scalar etaLow =
        laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
        (dict, 1e5, 480, 100);
    const scalar etaHigh =
        laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
        (dict, 4e7, 480, 100);

    Info<< "    eta(p=1e5) = " << etaLow
        << " Pa s, eta(p=4e7) = " << etaHigh << " Pa s" << endl;

    checkBool
    (
        "CrossWlf: pressure shift raises the viscosity with pressure",
        relDiff(etaLow, 517.9224369741157) < 1e-10
     && relDiff(etaHigh, 533.9137831768737) < 1e-10
     && etaHigh > etaLow
    );

    {
        IStringStream isZero(crossWlfDictString);
        dictionary zeroDict(isZero);

        checkBool
        (
            "CrossWlf: D3 = 0 leaves the viscosity pressure-independent",
            laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
            (zeroDict, 1e5, 480, 100)
         == laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
            (zeroDict, 4e7, 480, 100)
        );
    }

    // Crystallinity viscosity factor (1 - chi/chiInf)^(-a), clipped
    {
        const auto factor =
            laminarModels::generalisedNewtonianViscosityModels::CrossWlf::
            crystallinityFactor;

        Info<< "    crystallinity factors: " << factor(0, 0.5, 2)
            << ", " << factor(0.25, 0.5, 2)
            << ", " << factor(0.4, 0.5, 2)
            << ", " << factor(0.5, 0.5, 2) << endl;

        checkBool
        (
            "CrossWlf: crystallinity factor (1 - chi/chiInf)^(-a) with a "
            "clipped divergence",
            relDiff(factor(0, 0.5, 2), 1.0) < 1e-14
         && relDiff(factor(0.25, 0.5, 2), 4.0) < 1e-12
         && relDiff(factor(0.4, 0.5, 2), 25.0) < 1e-12
         && relDiff(factor(0.5, 0.5, 2), 1e6) < 1e-12
        );
    }
}



const char* viscoelasticDictString = R"(
relaxationTime 1;
zeroShearViscosity 1000;
mobilityFactor 0;
)";

const char* giesekusDictString = R"(
relaxationTime 1;
zeroShearViscosity 1000;
mobilityFactor 0.5;
)";

const char* ucmOscillatoryDictString = R"(
relaxationTime 0.01;
zeroShearViscosity 1000;
mobilityFactor 0;
)";


void viscoelasticTests()
{
    IStringStream is(viscoelasticDictString);
    dictionary dict(is);
    moldingViscoelastic ucm(dict);

    const scalar eta0 = 1000;
    const scalar lambda = 1;
    const scalar gammaDot = 0.1;

    tensor L(tensor::zero);
    L(0, 1) = gammaDot;

    // UCM start-up shear
    {
        symmTensor tau(symmTensor::zero);
        const scalar dt = 1e-4;

        for (label i = 0; i < 200000; ++i)
        {
            tau = ucm.advance(tau, L, dt);
        }

        const scalar tEnd = 200000*dt;
        const scalar expected =
            eta0*gammaDot*(1 - std::exp(-tEnd/lambda));

        Info<< "    UCM start-up tau_xy = " << tau.xy()
            << " Pa (expected " << expected << " Pa)" << endl;

        checkBool
        (
            "viscoelastic: UCM start-up shear follows "
            "eta0 gammadot (1 - exp(-t/lambda))",
            relDiff(tau.xy(), expected) < 1e-4
        );

        // Steady first normal stress difference
        const scalar N1 = tau.xx() - tau.yy();

        checkBool
        (
            "viscoelastic: UCM steady N1 = 2 eta0 lambda gammadot^2",
            relDiff(N1, 2*eta0*lambda*gammaDot*gammaDot) < 1e-3
        );
    }

    // UCM relaxation from a steady state
    {
        symmTensor tau(symmTensor::zero);
        const scalar dt = 1e-4;

        for (label i = 0; i < 20000; ++i)
        {
            tau = ucm.advance(tau, L, dt);
        }

        const scalar tau0 = tau.xy();

        // Stop the shear and relax
        const tensor Lzero(tensor::zero);

        for (label i = 0; i < 10000; ++i)
        {
            tau = ucm.advance(tau, Lzero, dt);
        }

        const scalar expected = tau0*std::exp(-1.0);

        Info<< "    UCM relaxation tau_xy = " << tau.xy()
            << " Pa (expected " << expected << " Pa)" << endl;

        checkBool
        (
            "viscoelastic: relaxation decays as exp(-t/lambda)",
            relDiff(tau.xy(), expected) < 1e-4
        );
    }

    // Giesekus: shear thinning and positive N1
    {
        IStringStream isG(giesekusDictString);
        dictionary gDict(isG);
        moldingViscoelastic giesekus(gDict);

        const scalar g1 = 0.1;
        const scalar g2 = 1.0;
        tensor L1(tensor::zero);
        tensor L2(tensor::zero);
        L1(0, 1) = g1;
        L2(0, 1) = g2;

        symmTensor t1(symmTensor::zero);
        symmTensor t2(symmTensor::zero);

        for (label i = 0; i < 100000; ++i)
        {
            t1 = giesekus.advance(t1, L1, 1e-4);
        }
        for (label i = 0; i < 100000; ++i)
        {
            t2 = giesekus.advance(t2, L2, 1e-4);
        }

        const scalar eta1 = t1.xy()/g1;
        const scalar eta2 = t2.xy()/g2;

        Info<< "    Giesekus eta(0.1) = " << eta1
            << " Pa s, eta(1) = " << eta2 << " Pa s, N1(1) = "
            << t2.xx() - t2.yy() << " Pa" << endl;

        checkBool
        (
            "viscoelastic: Giesekus shear thins and gives a positive N1",
            eta2 < eta1
         && eta2 > 0
         && (t2.xx() - t2.yy()) > 0
        );
    }

    // Small-amplitude oscillatory shear: the steady-state shear stress
    // amplitude is eta0 gammadot0/sqrt(1 + (lambda omega)^2)
    {
        IStringStream isO(ucmOscillatoryDictString);
        dictionary oDict(isO);
        moldingViscoelastic ucmO(oDict);

        const scalar lambda = 0.01;
        const scalar eta0 = 1000;
        const scalar gammaDot0 = 0.01;
        const scalar omega = 2*constant::mathematical::pi*10;
        const scalar period = 2*constant::mathematical::pi/omega;
        const scalar dt = 1e-4;

        symmTensor tau(symmTensor::zero);

        scalar t = 0;
        scalar maxLast = 0;
        const scalar tStart = 5*period;

        for (label i = 0; i < 12000; ++i)
        {
            tensor L(tensor::zero);
            L(0, 1) = gammaDot0*std::cos(omega*t);

            tau = ucmO.advance(tau, L, dt);

            t += dt;

            if (t > tStart)
            {
                maxLast = max(maxLast, mag(tau.xy()));
            }
        }

        const scalar expected =
            eta0*gammaDot0/std::sqrt(1 + (lambda*omega)*(lambda*omega));

        Info<< "    UCM oscillatory |tau_xy| = " << maxLast
            << " Pa (expected " << expected << " Pa)" << endl;

        checkBool
        (
            "viscoelastic: UCM oscillatory shear amplitude matches "
            "eta0 gammadot0/sqrt(1+(lambda omega)^2)",
            relDiff(maxLast, expected) < 1e-2
        );
    }

    // No flow leaves the stress at zero
    {
        symmTensor tau(symmTensor::zero);
        const symmTensor t = ucm.advance(tau, tensor::zero, 1);

        checkBool
        (
            "viscoelastic: no flow keeps the stress at zero",
            mag(t.xx()) < 1e-14 && mag(t.xy()) < 1e-14
         && mag(t.yy()) < 1e-14
        );
    }
}



const char* warpageDictString = R"(
thermalExpansion 7e-5;
elasticModulus 2e9;
poissonRatio 0.3;
referenceTemperature 363;
)";


void warpageTests()
{
    IStringStream is(warpageDictString);
    dictionary dict(is);
    moldingWarpage warp(dict);

    const scalar alpha = 7e-5;
    const scalar h = 0.002;

    // Linear through-thickness profile: 353 K bottom, 373 K top,
    // reference 363 K -> zero membrane strain, kappa = alpha dT/h
    {
        const label n = 8;
        List<scalar> T(n);

        for (label i = 0; i < n; ++i)
        {
            T[i] = 353 + 20*(i + 0.5)/n;
        }

        const scalar kappa = warp.freeCurvature(T, h);
        const scalar strain = warp.freeStrain(T, h);
        const scalar expected = alpha*20/h;

        Info<< "    linear profile: kappa = " << kappa
            << " 1/m (expected " << expected << "), strain = "
            << strain << endl;

        checkBool
        (
            "warpage: linear profile gives the bimetal curvature "
            "alpha dT/h with zero membrane strain",
            relDiff(kappa, expected) < 1e-12 && mag(strain) < 1e-15
        );
    }

    // Uniform profile: no curvature, membrane strain alpha dT
    {
        const label n = 8;
        List<scalar> T(n, 400.0);

        checkBool
        (
            "warpage: uniform profile gives no curvature",
            mag(warp.freeCurvature(T, h)) < 1e-12
         && relDiff(warp.freeStrain(T, h), alpha*(400 - 363)) < 1e-14
        );
    }

    // Symmetric parabolic profile: no curvature by symmetry and the
    // analytical membrane strain alpha c h^2/12
    {
        const label n = 1024;
        const scalar c = 1e5;      // K/m^2
        List<scalar> T(n);

        for (label i = 0; i < n; ++i)
        {
            const scalar y = (i + 0.5)*h/n;
            T[i] = 363 + c*(y - 0.5*h)*(y - 0.5*h);
        }

        const scalar kappa = warp.freeCurvature(T, h);
        const scalar strain = warp.freeStrain(T, h);
        const scalar expected = alpha*c*h*h/12;

        Info<< "    parabolic profile: kappa = " << kappa
            << " 1/m, strain = " << strain
            << " (expected " << expected << ")" << endl;

        checkBool
        (
            "warpage: symmetric parabolic profile has no curvature and "
            "the analytical membrane strain",
            mag(kappa) < 1e-12 && relDiff(strain, expected) < 1e-4
        );
    }

    // Self-equilibrated residual stress distribution
    {
        const label n = 8;
        const scalar Teq = 363;
        List<scalar> T(n);
        List<scalar> sigma(n);

        for (label i = 0; i < n; ++i)
        {
            T[i] = 353 + 20*(i + 0.5)/n;
        }

        warp.residualStress(T, h, sigma);

        scalar mean = 0;
        forAll(sigma, i)
        {
            mean += sigma[i];
        }
        mean /= n;

        // sigma_i = E/(1-nu) alpha (T_i - Tref) for a zero-mean profile
        const scalar coeff = 2e9/(1 - 0.3)*alpha;
        scalar maxErr = 0;
        forAll(sigma, i)
        {
            maxErr = max(maxErr, mag(sigma[i] - coeff*(T[i] - Teq))/1e6);
        }

        Info<< "    residual stress: mean = " << mean
            << " Pa, max|err| = " << maxErr << " MPa" << endl;

        checkBool
        (
            "warpage: residual stress is self-equilibrated and matches "
            "E/(1-nu) alpha (T - Tref)",
            mag(mean) < 1e-6 && maxErr < 1e-12
        );
    }

    // Timoshenko bimetal curvature: equal layers of equal modulus reduce
    // to kappa = 3 dAlpha dT/(2h)
    {
        const scalar h1 = 0.001;
        const scalar h2 = 0.001;
        const scalar kappa = moldingWarpage::bimetalCurvature
        (
            h1, h2, 2e9, 2e9, 2.5e-5, 0.5e-5, 20
        );
        const scalar expected = 1.5*2e-5*20/(h1 + h2);

        Info<< "    bimetal kappa = " << kappa
            << " 1/m (expected " << expected << ")" << endl;

        checkBool
        (
            "warpage: bimetal curvature reduces to 3 dAlpha dT/(2h) for "
            "equal layers",
            relDiff(kappa, expected) < 1e-12
        );
    }

    // Simply supported strip deflection profile from a uniform curvature
    {
        const label n = 65;
        const scalar L = 0.05;
        const scalar kappa = 0.7;
        List<scalar> kList(n, kappa);
        List<scalar> w(n);

        warp.deflectionProfile(kList, L, w);

        // Uniform curvature: w(x) = kappa x (L - x)/2
        scalar maxErr = 0;
        for (label i = 0; i < n; ++i)
        {
            const scalar x = L*i/(n - 1);
            const scalar wAna = kappa*x*(L - x)/2;
            maxErr = max(maxErr, mag(w[i] - wAna));
        }

        const scalar wMidAna = kappa*L*L/8;

        Info<< "    deflection profile: w(L/2) = " << w[(n - 1)/2]
            << " m (expected " << wMidAna << " m), max|err| = "
            << maxErr << " m" << endl;

        checkBool
        (
            "warpage: uniform curvature gives the parabolic deflection "
            "kappa x (L - x)/2",
            relDiff(w[(n - 1)/2], wMidAna) < 1e-3 && maxErr < 1e-5
        );
    }

    // Strip deflection and constrained residual stress hand values
    {
        const scalar kappa = 0.7;
        const scalar L = 0.05;
        const scalar delta = warp.deflection(kappa, L);
        const scalar sigma = warp.constrainedStress(353);

        const scalar deltaExp = kappa*L*L/2;
        const scalar sigmaExp = 2e9/(1 - 0.3)*alpha*(363 - 353);

        Info<< "    deflection = " << delta << " m, constrained stress = "
            << sigma << " Pa" << endl;

        checkBool
        (
            "warpage: strip deflection and constrained residual stress",
            relDiff(delta, deltaExp) < 1e-14
         && relDiff(sigma, sigmaExp) < 1e-14
        );
    }
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

int main()
{
    Info<< "moldingFoam model tests" << nl
        << "=======================" << endl;

    taitTests();
    latentHeatTests();
    moldThermalTests();
    coolantChannelTests();
    crystallizationTests();
    shrinkageTests();
    viscoelasticTests();
    warpageTests();
    fiberOrientationTests();
    pressureDependentViscosityTests();
    runnerNetworkTests();
    ventOrificeTests();
    crossWlfTests();

    if (nFailed)
    {
        Info<< nl << nFailed << " test(s) FAILED" << endl;
        return 1;
    }

    Info<< nl << "All tests passed" << endl;

    return 0;
}


// ************************************************************************* //
