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

\*---------------------------------------------------------------------------*/

#include "moldingPrghPressureFvPatchScalarField.H"
#include "moldingStage.H"
#include "addToRunTimeSelectionTable.H"
#include "fvPatchFieldMapper.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

moldingPrghPressureFvPatchScalarField::moldingPrghPressureFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const dictionary& dict
)
:
    mixedFvPatchScalarField(p, iF, dict)
{
    if (dict.found("value"))
    {
        mixedFvPatchScalarField::operator==
        (
            Field<scalar>("value", dict, p.size())
        );
    }
    else
    {
        mixedFvPatchScalarField::operator==
        (
            patchInternalField()
        );
    }
}


moldingPrghPressureFvPatchScalarField::moldingPrghPressureFvPatchScalarField
(
    const moldingPrghPressureFvPatchScalarField& mpppsf,
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const fieldMapper& m
)
:
    mixedFvPatchScalarField(mpppsf, p, iF, m)
{}


moldingPrghPressureFvPatchScalarField::moldingPrghPressureFvPatchScalarField
(
    const moldingPrghPressureFvPatchScalarField& mpppsf,
    const DimensionedField<scalar, fvMesh>& iF
)
:
    mixedFvPatchScalarField(mpppsf, iF)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void moldingPrghPressureFvPatchScalarField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    const moldingStage& stage = db().lookupObject<moldingStage>
    (
        moldingStage::typeName
    );

    if (stage.packing())
    {
        // Packing: prescribe the packing pressure target at the gate.
        // p_rgh = p - rho*(g.h); for the thin cavities of the moldingFoam
        // contract the hydrostatic head is negligible, so p_rgh equals the
        // target pressure
        valueFraction() = 1.0;
        refValue() = stage.pressure(patch().time().value());
        refGrad() = 0.0;
    }
    else
    {
        // Filling: the flow is volumetric-flow-rate controlled by the
        // moldingInletVelocity boundary condition, so the gate pressure
        // gradient is left to the pressure equation
        valueFraction() = 0.0;
        refGrad() = 0.0;
    }

    mixedFvPatchScalarField::updateCoeffs();
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchScalarField,
        moldingPrghPressureFvPatchScalarField
    );
}


// ************************************************************************* //
