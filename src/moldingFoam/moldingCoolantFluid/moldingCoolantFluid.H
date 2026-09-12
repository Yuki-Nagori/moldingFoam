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

Class
    Foam::solvers::moldingCoolantFluid

Description
    Incompressible coolant-flow module for the mould cooling-channel
    region (task 036, route A).

    Builds on \c incompressibleFluid for the constant-density water motion
    and adds the passive temperature equation

    \verbatim
        dT/dt + div(phi, T) - laplacian(alphaEff, T) = 0
        alphaEff = kappa/(rho Cp)
    \endverbatim

    The temperature is advected by the incompressible volumetric flux and
    coupled to the neighbouring solid regions through the standard
    \c coupledTemperature boundary conditions, so the module can take part
    in a multi-region conjugate heat transfer (CHT) run together with the
    \c solid mould and the \c moldingFoam cavity.

    The constant density and the neglected buoyancy are consistent for
    the forced-convection cooling channels (the temperature is passive);
    the properties are read from \c constant/physicalProperties:

    \verbatim
        rho   1000;   // [kg/m^3]
        Cp    4182;   // [J/kg/K]
        kappa 0.6;    // [W/m/K]
    \endverbatim

SourceFiles
    moldingCoolantFluid.C

\*---------------------------------------------------------------------------*/

#ifndef moldingCoolantFluid_H
#define moldingCoolantFluid_H

#include "incompressibleFluid.H"
#include "volFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace solvers
{

/*---------------------------------------------------------------------------*\
                    Class moldingCoolantFluid Declaration
\*---------------------------------------------------------------------------*/

class moldingCoolantFluid
:
    public incompressibleFluid
{
    // Private Data

        //- Temperature field [K]
        volScalarField T_;

        //- Coolant density [kg/m^3]
        dimensionedScalar rho_;

        //- Coolant heat capacity [J/kg/K]
        dimensionedScalar Cp_;

        //- Coolant thermal conductivity [W/m/K]
        dimensionedScalar kappa_;


    // Private Member Functions

        //- Read the physical properties
        void readProperties(const fvMesh& mesh);


public:

    //- Runtime type information
    TypeName("moldingCoolantFluid");


    // Constructors

        //- Construct from the region mesh
        moldingCoolantFluid(fvMesh& mesh);


    //- Destructor
    virtual ~moldingCoolantFluid();


    // Member Functions

        //- Access the temperature
        const volScalarField& T() const
        {
            return T_;
        }

        //- Solve the temperature equation
        virtual void thermophysicalPredictor();
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace solvers
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
