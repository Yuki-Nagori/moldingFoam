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
    thermo_(solidThermo::New(mesh)),
    thermophysicalTransport
    (
        solidThermophysicalTransportModel::New(thermo_())
    ),
    T_(thermo_->T())
{
    thermo_->validate("moldingCoolantFluid", "e", "h");

    Info<< "moldingCoolantFluid: rho = " << gAverage(thermo_->rho())
        << " kg/m^3, Cp = " << gAverage(thermo_->Cp())
        << " J/kg/K, kappa = " << gAverage(thermo_->kappa())
        << " W/m/K" << endl;
}


Foam::solvers::moldingCoolantFluid::~moldingCoolantFluid()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::solvers::moldingCoolantFluid::thermophysicalPredictor()
{
    // Passive temperature: advection with the incompressible volumetric
    // flux and constant-property conduction. The properties come from the
    // constant-property thermo.
    const volScalarField& rho = thermo_->rho();
    const volScalarField& Cp = thermo_->Cp();

    const tmp<volScalarField> tAlphaEff
    (
        thermophysicalTransport->kappaEff()/(rho*Cp)
    );

    fvScalarMatrix TEqn
    (
        fvm::ddt(T_)
      + fvm::div(phi, T_)
      - fvm::laplacian(tAlphaEff, T_)
    );

    TEqn.solve();

    // Diagnostic: the net boundary heat input and the mass-flow weighted
    // outlet temperature, so the energy balance can be checked against the
    // enthalpy rise
    if (mesh_.time().timeIndex() % 100 == 0)
    {
        const volScalarField& kappa = thermo_->kappa();

        scalar qNet = 0;

        forAll(T_.boundaryField(), patchi)
        {
            const fvPatchScalarField& Tp = T_.boundaryField()[patchi];

            qNet += gSum
            (
                kappa.boundaryField()[patchi]
               *Tp.snGrad()
               *Tp.patch().magSf()
            );
        }

        scalar mdot = 0;
        scalar mdotT = 0;

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
            << " W, outlet mdot = " << gAverage(rho)*mdot
            << " kg/s, outlet bulk T = "
            << (mag(mdot) > small ? mdotT/mdot : scalar(0)) << " K" << endl;
    }
}


// ************************************************************************* //
