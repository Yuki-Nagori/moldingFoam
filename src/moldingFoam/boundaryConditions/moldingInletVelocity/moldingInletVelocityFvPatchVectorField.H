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

Class
    Foam::moldingInletVelocityFvPatchVectorField

Description
    Gate velocity boundary condition for the moldingFoam solver module
    implementing the filling and packing injection modes of the V/P
    switch (stage M2 of the moldingFoam roadmap).

    The condition reads the moulding stage from the moldingStage object
    that the moldingFoam solver registers on the mesh:

    - filling: the velocity is volumetric-flow-rate controlled, i.e. the
      gate injects with a constant volumetric flow rate;
    - packing: the velocity follows the local volumetric flux computed by
      the pressure equation, which is driven by the packing pressure
      target imposed at the gate by the moldingPrghPressure boundary
      condition.

    Using this condition with a solver other than moldingFoam is a fatal
    error. Note that the flow rate is fixed at start-up: OpenFOAM-14
    provides no mechanism for re-reading boundary-condition data during a
    run (runTimeModifiable does not reach patch fields).

    Example specification:
    \verbatim
        inlet
        {
            type                moldingInletVelocity;
            volumetricFlowRate  3e-06;
            value               uniform (0 0 0);
        }
    \endverbatim

See also
    Foam::moldingStage
    Foam::moldingPrghPressureFvPatchScalarField
    Foam::flowRateInletVelocityFvPatchVectorField
    Foam::pressureInletVelocityFvPatchVectorField

SourceFiles
    moldingInletVelocityFvPatchVectorField.C

\*---------------------------------------------------------------------------*/

#ifndef moldingInletVelocityFvPatchVectorField_H
#define moldingInletVelocityFvPatchVectorField_H

#include "fixedValueFvPatchFields.H"
#include "Function1.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
              Class moldingInletVelocityFvPatchVectorField Declaration
\*---------------------------------------------------------------------------*/

class moldingInletVelocityFvPatchVectorField
:
    public fixedValueFvPatchVectorField
{
    // Private Data

        //- Injection volumetric flow rate during filling [m^3/s]
        scalar volumetricFlowRate_;

        //- Patch area; computed on first use and cached [m^2]
        scalar area_;


public:

    //- Runtime type information
    TypeName("moldingInletVelocity");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingInletVelocityFvPatchVectorField
        (
            const fvPatch&,
            const DimensionedField<vector, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping onto a new patch
        moldingInletVelocityFvPatchVectorField
        (
            const moldingInletVelocityFvPatchVectorField&,
            const fvPatch&,
            const DimensionedField<vector, fvMesh>&,
            const fieldMapper&
        );

        //- Disallow copy without setting internal field reference
        moldingInletVelocityFvPatchVectorField
        (
            const moldingInletVelocityFvPatchVectorField&
        ) = delete;

        //- Copy constructor setting internal field reference
        moldingInletVelocityFvPatchVectorField
        (
            const moldingInletVelocityFvPatchVectorField&,
            const DimensionedField<vector, fvMesh>&
        );

        //- Construct from dictionary re-mapped onto a new patch
        moldingInletVelocityFvPatchVectorField
        (
            const moldingInletVelocityFvPatchVectorField&,
            const DimensionedField<vector, fvMesh>&,
            const dictionary&
        );

        //- Construct and return a clone setting internal field reference
        virtual tmp<fvPatchVectorField> clone
        (
            const DimensionedField<vector, fvMesh>& iF
        ) const
        {
            return tmp<fvPatchVectorField>
            (
                new moldingInletVelocityFvPatchVectorField(*this, iF)
            );
        }


    // Member Functions

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
