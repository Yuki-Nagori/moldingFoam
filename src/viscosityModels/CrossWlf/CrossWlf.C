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

\*---------------------------------------------------------------------------*/

#include "CrossWlf.H"
#include "fvcGrad.H"
#include "symmTensorField.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace laminarModels
{
namespace generalisedNewtonianViscosityModels
{
    defineTypeNameAndDebug(CrossWlf, 0);

    // Register in the generalised Newtonian viscosity model family
    addToRunTimeSelectionTable
    (
        generalisedNewtonianViscosityModel,
        CrossWlf,
        dictionary
    );
}
}
}


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

Foam::scalar
Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf::
crystallinityFactor
(
    const scalar chi,
    const scalar chiInfinity,
    const scalar exponent
)
{
    // Clip the factor so a fully crystallised cell stays numerically
    // finite (the factor models the sharp viscosity rise; the momentum
    // solve cannot use an infinite viscosity)
    return
        min
        (
            pow(max(1 - max(chi, scalar(0))/chiInfinity, small), -exponent),
            scalar(1e6)
        );
}


Foam::scalar
Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf::etaValue
(
    const coeffs& c,
    scalar p,
    scalar T,
    scalar gammaDot
)
{
    // Floor the strain rate to prevent the zero-shear-rate limit diverging
    gammaDot = max(gammaDot, c.gammaDotMin);

    // WLF temperature offset including the pressure shift
    const scalar TStar = c.D2 + c.D3*p;
    const scalar x = T - TStar;

    // Floor the WLF denominator: below it the exponential argument would
    // run away in the frozen region
    const scalar denom = max(c.A2 + x, scalar(small));

    // Cap the exponent argument so that eta0 <= etaMax; this is what keeps
    // the model bounded in the frozen region (eCap = log(etaMax/D1),
    // pre-computed at read time)
    scalar e = -c.A1*x/denom;
    e = min(e, c.eCap);

    const scalar eta0 = c.D1*exp(e);

    const scalar eta =
        eta0
       /(scalar(1) + pow(eta0*gammaDot/c.tauStar, c.oneMinusN));

    return min(max(eta, c.etaMin), c.etaMax);
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf::CrossWlf
(
    const dictionary& viscosityProperties,
    const viscosity& viscosity,
    const volVectorField& U
)
:
    strainRateViscosityModel(viscosityProperties, viscosity, U),
    coeffs_()
{
    read(viscosityProperties);
    correct();
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf::coeffs
Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf::readCoeffs
(
    const dictionary& coeffsDict
)
{
    coeffs c;
    c.n = coeffsDict.lookup<scalar>("n");
    c.tauStar = coeffsDict.lookup<scalar>("tauStar");
    c.D1 = coeffsDict.lookup<scalar>("D1");
    c.D2 = coeffsDict.lookup<scalar>("D2");
    c.D3 = coeffsDict.lookupOrDefault<scalar>("D3", 0);
    c.A1 = coeffsDict.lookup<scalar>("A1");
    c.A2 = coeffsDict.lookup<scalar>("A2");
    c.etaMin = coeffsDict.lookup<scalar>("etaMin");
    c.etaMax = coeffsDict.lookup<scalar>("etaMax");
    c.gammaDotMin =
        coeffsDict.lookupOrDefault<scalar>("gammaDotMin", 1e-6);

    // Optional Lipscomb orientation correction (task 034): the viscosity
    // gains (ratio - 1) * 3/2 * (a:D)^2/(D:D) up to the axial ratio
    c.lipscombRatio = coeffsDict.lookupOrDefault<scalar>("lipscombRatio", 1);
    c.orientationField =
        coeffsDict.lookupOrDefault<word>("orientationField", "a");
    c.useOrientation = c.lipscombRatio > 1;

    // Optional crystallinity correction: eta *= (1 - chi/chiInf)^(-a)
    c.useCrystallinity = coeffsDict.found("crystallinity");

    if (c.useCrystallinity)
    {
        const dictionary& xtal(coeffsDict.subDict("crystallinity"));

        c.chiInfinity = xtal.lookup<scalar>("chiInfinity");
        c.chiExponent = xtal.lookup<scalar>("exponent");

        if (c.chiInfinity <= 0 || c.chiInfinity > 1 || c.chiExponent < 0)
        {
            FatalIOErrorInFunction(xtal)
                << "The crystallinity correction requires 0 < chiInfinity "
                << "<= 1 and exponent >= 0: chiInfinity = " << c.chiInfinity
                << ", exponent = " << c.chiExponent
                << exit(FatalIOError);
        }
    }
    else
    {
        c.chiInfinity = 1;
        c.chiExponent = 0;
    }

    // Derived, per-call invariants hoisted out of the per-cell loops
    c.oneMinusN = 1 - c.n;
    c.eCap = log(c.etaMax/c.D1);

    return c;
}


bool Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf::read
(
    const dictionary& viscosityProperties
)
{
    strainRateViscosityModel::read(viscosityProperties);

    const dictionary& coeffsDict =
        viscosityProperties.optionalTypeDict(typeName);

    coeffs_ = readCoeffs(coeffsDict);

    if
    (
        coeffs_.n <= 0 || coeffs_.n >= 1
     || coeffs_.tauStar <= 0
     || coeffs_.D1 <= 0
     || coeffs_.etaMin <= 0
     || coeffs_.etaMax <= coeffs_.etaMin
     || coeffs_.gammaDotMin <= 0
    )
    {
        FatalIOErrorInFunction(viscosityProperties)
            << "CrossWlf coefficients out of range" << nl
            << "    n           = " << coeffs_.n << nl
            << "    tauStar     = " << coeffs_.tauStar << nl
            << "    D1          = " << coeffs_.D1 << nl
            << "    etaMin      = " << coeffs_.etaMin << nl
            << "    etaMax      = " << coeffs_.etaMax << nl
            << "    gammaDotMin = " << coeffs_.gammaDotMin
            << exit(FatalIOError);
    }

    Info<< "CrossWlf: n = " << coeffs_.n << ", tauStar = " << coeffs_.tauStar
        << ", D1 = " << coeffs_.D1 << ", D2 = " << coeffs_.D2
        << ", D3 = " << coeffs_.D3 << ", A1 = " << coeffs_.A1
        << ", A2 = " << coeffs_.A2 << ", etaMin = " << coeffs_.etaMin
        << ", etaMax = " << coeffs_.etaMax << endl;

    return true;
}


Foam::scalar
Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
(
    const dictionary& coeffsDict,
    scalar p,
    scalar T,
    scalar gammaDot
)
{
    return etaValue(readCoeffs(coeffsDict), p, T, gammaDot);
}


Foam::scalar
Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf::eta
(
    const coeffs& c,
    scalar p,
    scalar T,
    scalar gammaDot
)
{
    return etaValue(c, p, T, gammaDot);
}


Foam::tmp<Foam::volScalarField>
Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf::nu
(
    const volScalarField& nu0,
    const volScalarField& strainRate
) const
{
    const fvMesh& mesh = U_.mesh();

    // The temperature, pressure and mixture density fields are provided by
    // the solver module (e.g. moldingFoam); a missing field is fatal
    const volScalarField& T = mesh.lookupObject<volScalarField>("T");
    const volScalarField& p = mesh.lookupObject<volScalarField>("p");
    const volScalarField& rho = mesh.lookupObject<volScalarField>("rho");

    // Construct an unregistered transient field (no object-registry churn
    // across the per-step corrections)
    tmp<volScalarField> tnu
    (
        new volScalarField
        (
            IOobject
            (
                typedName("nu"),
                Time::timeName(mesh.time().value()),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE,
                false
            ),
            mesh,
            dimensionedScalar(dimKinematicViscosity, small)
        )
    );

    // Internal field: single-source evaluation through the scalar model
    {
        const scalarField& Tc = T.primitiveField();
        const scalarField& pc = p.primitiveField();
        const scalarField& rhoc = rho.primitiveField();
        const scalarField& gammadotc = strainRate.primitiveField();

        scalarField& nuc = tnu.ref().primitiveFieldRef();
        forAll(nuc, i)
        {
            nuc[i] = etaValue(coeffs_, pc[i], Tc[i], gammadotc[i])/rhoc[i];
        }

        // Optional Lipscomb orientation correction (task 034): the
        // apparent viscosity follows the alignment of the strain rate
        // with the fibre orientation (quadratic closure: (A:D):D = (a:D)^2)
        if
        (
            coeffs_.useOrientation
         && mesh.foundObject<volSymmTensorField>(coeffs_.orientationField)
        )
        {
            const symmTensorField& ac =
                mesh.lookupObject<volSymmTensorField>
                (
                    coeffs_.orientationField
                ).primitiveField();

            const volSymmTensorField D(symm(fvc::grad(U_)));
            const symmTensorField& Dc = D.primitiveField();

            forAll(nuc, i)
            {
                const scalar aD = ac[i] && Dc[i];
                const scalar DD = Dc[i] && Dc[i];

                if (DD > small)
                {
                    const scalar f
                    (
                        min
                        (
                            max
                            (
                                1 + (coeffs_.lipscombRatio - 1)
                                   *1.5*aD*aD/DD,
                                scalar(1)
                            ),
                            coeffs_.lipscombRatio
                        )
                    );

                    nuc[i] *= f;
                }
            }
        }

        // Optional crystallinity correction: the viscosity rises sharply
        // as the relative crystallinity grows
        if (coeffs_.useCrystallinity && mesh.foundObject<volScalarField>("chi"))
        {
            const scalarField& chic =
                mesh.lookupObject<volScalarField>("chi").primitiveField();

            forAll(nuc, i)
            {
                nuc[i] *= crystallinityFactor
                (
                    chic[i],
                    coeffs_.chiInfinity,
                    coeffs_.chiExponent
                );
            }
        }
    }

    // Boundary field
    volScalarField::Boundary& nuBf = tnu.ref().boundaryFieldRef();
    forAll(nuBf, patchi)
    {
        const scalarField& Tp = T.boundaryField()[patchi];
        const scalarField& pp = p.boundaryField()[patchi];
        const scalarField& rhop = rho.boundaryField()[patchi];
        const scalarField& gammadotp = strainRate.boundaryField()[patchi];

        scalarField& nup = nuBf[patchi];
        forAll(nup, i)
        {
            nup[i] =
                etaValue(coeffs_, pp[i], Tp[i], gammadotp[i])/rhop[i];
        }
    }

    return tnu;
}


// ************************************************************************* //
