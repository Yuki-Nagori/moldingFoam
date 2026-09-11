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
    under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    moldingFoam is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with moldingFoam.  If not, see <http://www.gnu.org/licenses/>.

Class
    Foam::moldingRunnerTemperatureFvPatchScalarField

Description
    Gate melt temperature boundary condition fed by a 1D runner network.

    The patch temperature is the runner network's gate temperature at the
    current gate flow rate: the steady 1D energy balance along the
    feed and gate segments

    \verbatim
        Tout = Twall + (Tin - Twall) exp(-htc pi D L/(mdot cp))
    \endverbatim

    evaluated with the network's viscosity model (pressure drop / flow
    split) so a hot runner with a controlled wall temperature sets the
    melt temperature entering the cavity.

    The optional dictionary entries mirror
    \c moldingInletVelocity: \c runner (the network sub-dictionary),
    \c gate (the gate name) and \c totalFlowRate. During filling the gate
    flow comes from the network's equal-pressure-drop split; during
    packing the local patch flux is used.

    Example:
    \verbatim
        inlet
        {
            type            moldingRunnerTemperature;
            totalFlowRate   3e-06;
            gate            gate1;
            runner
            {
                inletTemperature 480;
                cp               2400;
                rho              800;
                viscosity { type powerLaw; K 1e4; n 0.5; }
                feed { length 0.05; diameter 0.004; wallTemperature 500; htc 2000; }
                gates { gate1 { length 0.01; diameter 0.002; } }
            }
            value           uniform 480;
        }
    \endverbatim

SourceFiles
    moldingRunnerTemperatureFvPatchScalarField.C

See also
    Foam::moldingRunnerNetwork
    Foam::moldingInletVelocityFvPatchVectorField

\*---------------------------------------------------------------------------*/

#ifndef moldingRunnerTemperatureFvPatchScalarField_H
#define moldingRunnerTemperatureFvPatchScalarField_H

#include "fixedValueFvPatchFields.H"
#include "moldingRunnerNetwork.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
        Class moldingRunnerTemperatureFvPatchScalarField Declaration
\*---------------------------------------------------------------------------*/

class moldingRunnerTemperatureFvPatchScalarField
:
    public fixedValueFvPatchScalarField
{
    // Private Data

        //- Runner network (kept for writing)
        autoPtr<dictionary> runner_;

        //- Cached network
        autoPtr<moldingRunnerNetwork> network_;

        //- Gate name and index
        word gate_;

        //- Total runner flow rate [m^3/s]
        scalar totalFlowRate_;


public:

    //- Runtime type information
    TypeName("moldingRunnerTemperature");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingRunnerTemperatureFvPatchScalarField
        (
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping onto a new patch
        moldingRunnerTemperatureFvPatchScalarField
        (
            const moldingRunnerTemperatureFvPatchScalarField&,
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const fieldMapper&
        );

        //- Disallow copy without setting internal field reference
        moldingRunnerTemperatureFvPatchScalarField
        (
            const moldingRunnerTemperatureFvPatchScalarField&
        ) = delete;

        //- Copy constructor setting internal field reference
        moldingRunnerTemperatureFvPatchScalarField
        (
            const moldingRunnerTemperatureFvPatchScalarField&,
            const DimensionedField<scalar, fvMesh>&
        );

        //- Construct and return a clone setting internal field reference
        virtual tmp<fvPatchScalarField> clone
        (
            const DimensionedField<scalar, fvMesh>& iF
        ) const
        {
            return tmp<fvPatchScalarField>
            (
                new moldingRunnerTemperatureFvPatchScalarField(*this, iF)
            );
        }


    // Member Functions

        //- Update the coefficients associated with the patch field
        virtual void updateCoeffs();

        //- Write
        virtual void write(Ostream&) const;


    // Member Operators

        //- Inherit assignment
        using fixedValueFvPatchScalarField::operator=;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
