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
    Foam::moldingVentPressureFvPatchScalarField

Description
    Vent pressure boundary condition for \c p_rgh that passes air but not
    polymer, modelling the real venting behaviour of a mould:

    - while the vent is open (air): pressure \c p0, plus an orifice
      back-pressure when an effective discharge area \c CdA is given
      (a restricted vent raises the cavity back-pressure with the escaping
      gas flow, see Foam::ventOrifice);
    - once the melt front reaches the vent (\c moldingStage::ventSealed),
      zero normal flux, imposed together with the zero-velocity
      moldingVentVelocity condition, so no polymer escapes and the
      cavity pressure level is set by the remaining boundaries.

    The seal state is shared through the moldingStage object registered
    on the mesh by the moldingFoam solver; using this condition with a
    solver other than moldingFoam is a fatal error.

    Example specification:
    \verbatim
        vent
        {
            type       moldingVentPressure;
            p0         uniform 1e5;
            value      uniform 1e5;
        }
    \endverbatim

See also
    Foam::moldingStage
    Foam::moldingVentVelocityFvPatchVectorField

SourceFiles
    moldingVentPressureFvPatchScalarField.C

\*---------------------------------------------------------------------------*/

#ifndef moldingVentPressureFvPatchScalarField_H
#define moldingVentPressureFvPatchScalarField_H

#include "mixedFvPatchFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
          Class moldingVentPressureFvPatchScalarField Declaration
\*---------------------------------------------------------------------------*/

class moldingVentPressureFvPatchScalarField
:
    public mixedFvPatchScalarField
{
    // Private Data

        //- Vent (atmospheric) pressure [Pa]
        scalar p0_;

        //- Effective discharge area of the vent [m^2]; zero (default)
        //  models a fully open vent with no back-pressure
        scalar CdA_;


public:

    //- Runtime type information
    TypeName("moldingVentPressure");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingVentPressureFvPatchScalarField
        (
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping onto a new patch
        moldingVentPressureFvPatchScalarField
        (
            const moldingVentPressureFvPatchScalarField&,
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const fieldMapper&
        );

        //- Disallow copy without setting internal field reference
        moldingVentPressureFvPatchScalarField
        (
            const moldingVentPressureFvPatchScalarField&
        ) = delete;

        //- Copy constructor setting internal field reference
        moldingVentPressureFvPatchScalarField
        (
            const moldingVentPressureFvPatchScalarField&,
            const DimensionedField<scalar, fvMesh>&
        );

        //- Construct and return a clone setting internal field reference
        virtual tmp<fvPatchField<scalar>> clone
        (
            const DimensionedField<scalar, fvMesh>& iF
        ) const
        {
            return tmp<fvPatchField<scalar>>
            (
                new moldingVentPressureFvPatchScalarField(*this, iF)
            );
        }


    // Member Functions

        //- Vent pressure [Pa]
        scalar p0() const
        {
            return p0_;
        }

        //- Effective discharge area [m^2]
        scalar CdA() const
        {
            return CdA_;
        }

        //- Update the coefficients associated with the boundary field
        virtual void updateCoeffs();

        //- Write
        virtual void write(Ostream&) const;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
