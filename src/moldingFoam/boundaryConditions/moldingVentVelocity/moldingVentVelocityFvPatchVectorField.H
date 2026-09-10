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
    Foam::moldingVentVelocityFvPatchVectorField

Description
    Vent velocity boundary condition for \c U that passes air but not
    polymer:

    - while the vent is open, the upstream pressureInletOutletVelocity
      behaviour (outflow follows the flux, inflow from the inlet value);
    - once the melt front reaches the vent (\c moldingStage::ventSealed),
      zero velocity on the patch, matching the zero-flux
      moldingVentPressure condition.

    The seal state is shared through the moldingStage object registered
    on the mesh by the moldingFoam solver; using this condition with a
    solver other than moldingFoam is a fatal error.

    Example specification:
    \verbatim
        vent
        {
            type       moldingVentVelocity;
            value      uniform (0 0 0);
        }
    \endverbatim

See also
    Foam::moldingStage
    Foam::moldingVentPressureFvPatchScalarField
    Foam::pressureInletOutletVelocityFvPatchVectorField

SourceFiles
    moldingVentVelocityFvPatchVectorField.C

\*---------------------------------------------------------------------------*/

#ifndef moldingVentVelocityFvPatchVectorField_H
#define moldingVentVelocityFvPatchVectorField_H

#include "pressureInletOutletVelocityFvPatchVectorField.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
         Class moldingVentVelocityFvPatchVectorField Declaration
\*---------------------------------------------------------------------------*/

class moldingVentVelocityFvPatchVectorField
:
    public pressureInletOutletVelocityFvPatchVectorField
{
public:

    //- Runtime type information
    TypeName("moldingVentVelocity");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingVentVelocityFvPatchVectorField
        (
            const fvPatch&,
            const DimensionedField<vector, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping onto a new patch
        moldingVentVelocityFvPatchVectorField
        (
            const moldingVentVelocityFvPatchVectorField&,
            const fvPatch&,
            const DimensionedField<vector, fvMesh>&,
            const fieldMapper&
        );

        //- Disallow copy without setting internal field reference
        moldingVentVelocityFvPatchVectorField
        (
            const moldingVentVelocityFvPatchVectorField&
        ) = delete;

        //- Copy constructor setting internal field reference
        moldingVentVelocityFvPatchVectorField
        (
            const moldingVentVelocityFvPatchVectorField&,
            const DimensionedField<vector, fvMesh>&
        );

        //- Construct and return a clone setting internal field reference
        virtual tmp<fvPatchVectorField> clone
        (
            const DimensionedField<vector, fvMesh>& iF
        ) const
        {
            return tmp<fvPatchVectorField>
            (
                new moldingVentVelocityFvPatchVectorField(*this, iF)
            );
        }


    // Member Functions

        //- Update the coefficients associated with the boundary field
        virtual void updateCoeffs();
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
