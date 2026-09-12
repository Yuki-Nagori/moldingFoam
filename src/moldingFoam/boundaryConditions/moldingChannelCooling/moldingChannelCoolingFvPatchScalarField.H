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
    Foam::moldingChannelCoolingFvPatchScalarField

Description
    Convective cooling boundary for a solid region's coolant-channel wall
    driven by a one-dimensional plug-flow channel (task 036, route C).

    The patch faces are grouped into cross-sections along \c direction
    and the coolant bulk temperature is marched from the inlet,
    exchanging heat with the local wall temperature:

    \verbatim
        mdot Cp (Tc_out - Tc_in) = sum_f htc A_f (Tw_f - Tc_in)
    \endverbatim

    The implicit Robin weight uses the solid conductivity and the face
    delta coefficient, with the marched local coolant temperature as the
    reference (this mirrors \c moldingConvectiveCooling but replaces the
    uniform ambient temperature with the axial march of
    Foam::moldingCoolantChannel).

Usage
    \table
        Property          | Description                          | Req'd? | Default
        kappa             | Solid thermal conductivity [W/m/K]   | yes    |
        htc               | Channel HTC [W/m^2/K]                | yes    |
        massFlowRate      | Coolant mass flow [kg/s]             | yes    |
        cp                | Coolant heat capacity [J/kg/K]       | yes    |
        inletTemperature  | Coolant inlet temperature [K]        | yes    |
        direction         | Channel axis                         | no     | (1 0 0)
    \endtable

    Example:
    \verbatim
        channelWall
        {
            type             moldingChannelCooling;
            kappa            0.2;
            htc              5000;
            massFlowRate     0.0001;
            cp               4182;
            inletTemperature 350;
            direction        (1 0 0);
        }
    \endverbatim

SourceFiles
    moldingChannelCoolingFvPatchScalarField.C

\*---------------------------------------------------------------------------*/

#ifndef moldingChannelCoolingFvPatchScalarField_H
#define moldingChannelCoolingFvPatchScalarField_H

#include "mixedFvPatchFields.H"
#include "volFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
        Class moldingChannelCoolingFvPatchScalarField Declaration
\*---------------------------------------------------------------------------*/

class moldingChannelCoolingFvPatchScalarField
:
    public mixedFvPatchScalarField
{
    // Private Data

        //- Solid thermal conductivity [W/m/K]
        scalar kappa_;

        //- Channel heat transfer coefficient [W/m^2/K]
        scalar htc_;

        //- Coolant mass flow rate [kg/s]
        scalar mdot_;

        //- Coolant heat capacity [J/kg/K]
        scalar cp_;

        //- Coolant inlet temperature [K]
        scalar inletTemperature_;

        //- Channel axis
        vector direction_;

        //- Group axial positions (globally consistent)
        List<scalar> groupPos_;

        //- Group conductances htc*A [W/K]
        List<scalar> groupHA_;

        //- Has the channel geometry been grouped?
        bool channelReady_;


    // Private Member Functions

        //- Group the patch faces into cross-sections (global, once)
        void buildChannel();


public:

    //- Runtime type information
    TypeName("moldingChannelCooling");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingChannelCoolingFvPatchScalarField
        (
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping onto a new patch
        moldingChannelCoolingFvPatchScalarField
        (
            const moldingChannelCoolingFvPatchScalarField&,
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const fvPatchFieldMapper&
        );

        //- Construct as copy setting the internal field reference
        moldingChannelCoolingFvPatchScalarField
        (
            const moldingChannelCoolingFvPatchScalarField&,
            const DimensionedField<scalar, fvMesh>&
        );

        //- Construct and return a clone
        virtual tmp<fvPatchScalarField> clone() const
        {
            return tmp<fvPatchScalarField>
            (
                new moldingChannelCoolingFvPatchScalarField
                (
                    *this,
                    this->internalField()
                )
            );
        }

        //- Construct and return a clone setting the internal field
        virtual tmp<fvPatchScalarField> clone
        (
            const DimensionedField<scalar, fvMesh>& iF
        ) const
        {
            return tmp<fvPatchScalarField>
            (
                new moldingChannelCoolingFvPatchScalarField(*this, iF)
            );
        }


    // Member Functions

        //- Update the coefficients
        virtual void updateCoeffs();

        //- Write
        virtual void write(Ostream&) const;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
