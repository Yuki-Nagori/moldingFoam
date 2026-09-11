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
    Foam::moldingConvectiveCoolingFvPatchScalarField

Description
    Convective (Robin) cooling boundary for the mould temperature in
    multi-region CHT cases, used as the equivalent of the cooling
    channels on the mould outer surfaces.

    The wall flux balances conduction into the wall and convection to the
    coolant,

    \verbatim
        -kappa dT/dn = h (Tw - Ta)
    \endverbatim

    imposed implicitly through the mixed formulation

    \verbatim
        valueFraction = kappa delta/(kappa delta + h),  refValue = Ta
    \endverbatim

    so h -> infinity recovers the fixed water temperature and h = 0 the
    adiabatic wall. kappa is the solid conductivity [W/m/K].

    Example:
    \verbatim
        moldTop
        {
            type            moldingConvectiveCooling;
            kappa           50;     // [W/m/K]
            htc             2000;   // [W/m^2/K]
            coolantTemperature 300; // [K]
            value           uniform 353;
        }
    \endverbatim

SourceFiles
    moldingConvectiveCoolingFvPatchScalarField.C

\*---------------------------------------------------------------------------*/

#ifndef moldingConvectiveCoolingFvPatchScalarField_H
#define moldingConvectiveCoolingFvPatchScalarField_H

#include "mixedFvPatchFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
      Class moldingConvectiveCoolingFvPatchScalarField Declaration
\*---------------------------------------------------------------------------*/

class moldingConvectiveCoolingFvPatchScalarField
:
    public mixedFvPatchScalarField
{
    // Private Data

        //- Solid conductivity [W/m/K]
        scalar kappa_;

        //- Coolant heat transfer coefficient [W/m^2/K]
        scalar htc_;

        //- Coolant temperature [K]
        scalar Ta_;


public:

    //- Runtime type information
    TypeName("moldingConvectiveCooling");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingConvectiveCoolingFvPatchScalarField
        (
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping onto a new patch
        moldingConvectiveCoolingFvPatchScalarField
        (
            const moldingConvectiveCoolingFvPatchScalarField&,
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const fieldMapper&
        );

        //- Disallow copy without setting internal field reference
        moldingConvectiveCoolingFvPatchScalarField
        (
            const moldingConvectiveCoolingFvPatchScalarField&
        ) = delete;

        //- Copy constructor setting internal field reference
        moldingConvectiveCoolingFvPatchScalarField
        (
            const moldingConvectiveCoolingFvPatchScalarField&,
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
                new moldingConvectiveCoolingFvPatchScalarField(*this, iF)
            );
        }


    // Member Functions

        //- Update the coefficients associated with the patch field
        virtual void updateCoeffs();

        //- Write
        virtual void write(Ostream&) const;


    // Member Operators

        //- Inherit assignment
        using mixedFvPatchScalarField::operator=;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
