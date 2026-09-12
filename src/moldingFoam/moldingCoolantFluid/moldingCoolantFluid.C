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

\*---------------------------------------------------------------------------*/

#include "moldingCoolantFluid.H"
#include "fvmDdt.H"
#include "fvmDiv.H"
#include "fvmLaplacian.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace solvers
{
    defineTypeNameAndDebug(moldingCoolantFluid, 0);
    addToRunTimeSelectionTable(solver, moldingCoolantFluid, fvMesh);
}
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::solvers::moldingCoolantFluid::moldingCoolantFluid(fvMesh& mesh)
:
    incompressibleFluid(mesh),
    T_
    (
        IOobject
        (
            "T",
            mesh.time().name(),
            mesh,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        mesh
    ),
    rho_("rho", dimDensity, 0),
    Cp_("Cp", dimEnergy/dimMass/dimTemperature, 0),
    kappa_("kappa", dimPower/dimLength/dimTemperature, 0)
{
    readProperties(mesh);
    T_.correctBoundaryConditions();

    Info<< "moldingCoolantFluid: rho = " << rho_.value()
        << " kg/m^3, Cp = " << Cp_.value()
        << " J/kg/K, kappa = " << kappa_.value() << " W/m/K" << endl;
}


Foam::solvers::moldingCoolantFluid::~moldingCoolantFluid()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::solvers::moldingCoolantFluid::readProperties(const fvMesh& mesh)
{
    const IOdictionary props
    (
        IOobject
        (
            "physicalProperties",
            mesh.time().constant(),
            mesh,
            IOobject::MUST_READ,
            IOobject::NO_WRITE
        )
    );

    rho_ = dimensionedScalar("rho", dimDensity, props);
    Cp_ = dimensionedScalar
    (
        "Cp",
        dimEnergy/dimMass/dimTemperature,
        props
    );
    kappa_ = dimensionedScalar
    (
        "kappa",
        dimPower/dimLength/dimTemperature,
        props
    );

    if
    (
        rho_.value() <= 0
     || Cp_.value() <= 0
     || kappa_.value() <= 0
    )
    {
        FatalIOErrorInFunction(props)
            << "The coolant properties must be positive: rho = "
            << rho_.value() << ", Cp = " << Cp_.value()
            << ", kappa = " << kappa_.value() << exit(FatalIOError);
    }
}


void Foam::solvers::moldingCoolantFluid::thermophysicalPredictor()
{
    // Passive temperature: advection with the incompressible volumetric
    // flux and constant-property conduction
    const dimensionedScalar alphaEff(kappa_/(rho_*Cp_));

    fvScalarMatrix TEqn
    (
        fvm::ddt(T_)
      + fvm::div(phi, T_)
      - fvm::laplacian(alphaEff, T_)
    );

    TEqn.solve();

    // Diagnostic: the net boundary heat input (the sum of the conductive
    // fluxes over every patch) and the mass-flow weighted outlet
    // temperature, so the energy balance can be checked against the
    // enthalpy rise
    if (mesh_.time().timeIndex() % 100 == 0)
    {
        scalar qNet = 0;

        forAll(T_.boundaryField(), patchi)
        {
            const fvPatchScalarField& Tp = T_.boundaryField()[patchi];

            qNet += gSum
            (
                kappa_.value()*Tp.snGrad()*Tp.patch().magSf()
            );
        }

        scalar mdot = 0;
        scalar mdotT = 0;

        // Outflow patches are those with a positive net volumetric flux
        forAll(phi_.boundaryField(), patchi)
        {
            const fvsPatchScalarField& phip = phi_.boundaryField()[patchi];
            const scalar mf = gSum(phip);

            if (mf > small)
            {
                mdot += mf;
                mdotT += gSum(phip*T_.boundaryField()[patchi]);
            }
        }

        Info<< "moldingCoolantFluid: net boundary heat = " << qNet
            << " W, outlet mdot = " << rho_.value()*mdot
            << " kg/s, outlet bulk T = "
            << (mag(mdot) > small ? mdotT/mdot : scalar(0)) << " K" << endl;
    }
}


// ************************************************************************* //
