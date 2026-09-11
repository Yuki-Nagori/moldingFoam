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
    Foam::moldingPrghPressureFvPatchScalarField

Description
    Gate pressure boundary condition for \c p_rgh for the moldingFoam
    solver module implementing the filling and packing modes of the V/P
    switch (stage M2 of the moldingFoam roadmap).

    The condition reads the moulding stage from the moldingStage object
    that the moldingFoam solver registers on the mesh:

    - filling: zero-gradient (the flow is volumetric-flow-rate controlled
      by the moldingInletVelocity boundary condition);
    - packing: Dirichlet; \c p_rgh is set to the packing pressure target
      returned by the \c pressure curve of \c constant/moldingDict,
      evaluated relative to the V/P switch time. With an optional
      \c runner sub-dictionary the 1D runner network pressure drop at the
      current gate flow is subtracted from the target, so the runner
      participates in the packing pressure transmission.

    The condition prescribes \c p_rgh, i.e. \f$ p - \rho \boldsymbol{g}
    \cdot \boldsymbol{h} \f$. For the thin-walled plaque cavities targeted
    by the moldingFoam contract the hydrostatic head is negligible
    (\f$ \rho g h \lesssim 10 \, \mathrm{Pa} \f$), so the prescribed value
    equals the packing pressure target; this assumption is part of the
    moldingFoam contract.

    Using this condition with a solver other than moldingFoam is a fatal
    error.

    Example specification:
    \verbatim
        inlet
        {
            type            moldingPrghPressure;
            refValue        uniform 1e5;
            refGradient     uniform 0;
            valueFraction   uniform 0;
            value           uniform 1e5;
        }
    \endverbatim

    Example with a runner network (the packing target is the machine
    pressure; the gate sees it reduced by the runner pressure drop):

    \verbatim
        inlet
        {
            type            moldingPrghPressure;
            refValue        uniform 1e5;
            refGradient     uniform 0;
            valueFraction   uniform 0;
            value           uniform 1e5;
            runner
            {
                inletTemperature 480;
                cp               2400;
                rho              800;
                viscosity { type powerLaw; K 1e4; n 0.5; }
                feed { length 0.05; diameter 0.004; }
                gates { gate1 { length 0.01; diameter 0.002; } }
            }
        }
    \endverbatim

See also
    Foam::moldingStage
    Foam::moldingInletVelocityFvPatchVectorField
    Foam::mixedFvPatchScalarField

SourceFiles
    moldingPrghPressureFvPatchScalarField.C

\*---------------------------------------------------------------------------*/

#ifndef moldingPrghPressureFvPatchScalarField_H
#define moldingPrghPressureFvPatchScalarField_H

#include "mixedFvPatchFields.H"
#include "moldingRunnerNetwork.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
               Class moldingPrghPressureFvPatchScalarField Declaration
\*---------------------------------------------------------------------------*/

class moldingPrghPressureFvPatchScalarField
:
    public mixedFvPatchScalarField
{
    // Private Data

        //- Optional 1D runner network (kept for writing)
        autoPtr<dictionary> runner_;


public:

    //- Runtime type information
    TypeName("moldingPrghPressure");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingPrghPressureFvPatchScalarField
        (
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping onto a new patch
        moldingPrghPressureFvPatchScalarField
        (
            const moldingPrghPressureFvPatchScalarField&,
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const fieldMapper&
        );

        //- Disallow copy without setting internal field reference
        moldingPrghPressureFvPatchScalarField
        (
            const moldingPrghPressureFvPatchScalarField&
        ) = delete;

        //- Copy constructor setting internal field reference
        moldingPrghPressureFvPatchScalarField
        (
            const moldingPrghPressureFvPatchScalarField&,
            const DimensionedField<scalar, fvMesh>&
        );

        //- Construct from dictionary re-mapped onto a new patch
        moldingPrghPressureFvPatchScalarField
        (
            const moldingPrghPressureFvPatchScalarField&,
            const DimensionedField<scalar, fvMesh>&,
            const dictionary&
        );

        //- Construct and return a clone setting internal field reference
        virtual tmp<fvPatchField<scalar>> clone
        (
            const DimensionedField<scalar, fvMesh>& iF
        ) const
        {
            return tmp<fvPatchField<scalar>>
            (
                new moldingPrghPressureFvPatchScalarField(*this, iF)
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
