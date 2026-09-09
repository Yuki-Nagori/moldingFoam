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

    hMelt thermodynamics (apparent-Cp latent heat):
    - the latent-Cp peak vanishes at the band edges (C1 continuity);
    - its integral across the transition band equals latentHeat;
    - dHs/dT matches the apparent Cp;
    - latentHeat = 0 reproduces constant-Cp behaviour.

\*---------------------------------------------------------------------------*/

#include "Tait.H"
#include "hMeltThermo.H"
#include "CrossWlf.H"
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

    // The apparent-Cp peak vanishes at the band edges (C1 continuity)
    checkBool
    (
        "hMelt: latent-Cp peak vanishes at the band edges",
        thermo.Cp(p, Tt - band) == Cp0 && thermo.Cp(p, Tt + band) == Cp0
    );

    // Peak of dw/dT is 1.5/(2*band) at the band centre
    checkBool
    (
        "hMelt: apparent-Cp peak reproduced at the band centre",
        relDiff(thermo.Cp(p, Tt), Cp0 + L*1.5/(2*band)) < 1e-12
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

    // dHs/dT matches the apparent Cp inside the band (away from the
    // second-derivative discontinuities at the edges)
    {
        bool ok = true;
        const scalar h = 1e-3;
        for
        (
            scalar T = Tt - band + 0.05;
            T <= Tt + band - 0.05 + small;
            T += 0.05
        )
        {
            const scalar dhsFD =
                (thermo.hs(p, T + h) - thermo.hs(p, T - h))/(2*h);
            ok = ok && relDiff(dhsFD, thermo.Cp(p, T)) < 1e-8;
        }
        checkBool("hMelt: dHs/dT matches the apparent Cp", ok);
    }

    // latentHeat = 0 reproduces constant-Cp behaviour
    {
        hMelt plain(Tait<specie>("melt", dict), Cp0, 0, 0, Tstd, 0);

        checkBool
        (
            "hMelt: latentHeat = 0 reproduces constant-Cp behaviour",
            plain.Cp(p, 480) == Cp0
         && plain.hs(p, 480) == Cp0*(480 - Tstd)
        );
    }
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


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

int main()
{
    Info<< "moldingFoam model tests" << nl
        << "=======================" << endl;

    taitTests();
    latentHeatTests();
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
