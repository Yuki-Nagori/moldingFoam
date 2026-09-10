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

\*---------------------------------------------------------------------------*/

#include "moldingVentVelocityFvPatchVectorField.H"
#include "moldingStage.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingVentVelocityFvPatchVectorField::
moldingVentVelocityFvPatchVectorField
(
    const fvPatch& p,
    const DimensionedField<vector, fvMesh>& iF,
    const dictionary& dict
)
:
    pressureInletOutletVelocityFvPatchVectorField(p, iF, dict)
{}


Foam::moldingVentVelocityFvPatchVectorField::
moldingVentVelocityFvPatchVectorField
(
    const moldingVentVelocityFvPatchVectorField& ptf,
    const fvPatch& p,
    const DimensionedField<vector, fvMesh>& iF,
    const fieldMapper& mapper
)
:
    pressureInletOutletVelocityFvPatchVectorField(ptf, p, iF, mapper)
{}


Foam::moldingVentVelocityFvPatchVectorField::
moldingVentVelocityFvPatchVectorField
(
    const moldingVentVelocityFvPatchVectorField& ptf,
    const DimensionedField<vector, fvMesh>& iF
)
:
    pressureInletOutletVelocityFvPatchVectorField(ptf, iF)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::moldingVentVelocityFvPatchVectorField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    const moldingStage& stage = db().lookupObject<moldingStage>
    (
        moldingStage::typeName
    );

    if (stage.ventSealed())
    {
        // Melt front at the vent: no-slip (zero velocity) on the patch
        valueFraction() = I;
        refValue() = Zero;
        refGrad() = Zero;

        directionMixedFvPatchVectorField::updateCoeffs();
        directionMixedFvPatchVectorField::evaluate();
    }
    else
    {
        pressureInletOutletVelocityFvPatchVectorField::updateCoeffs();
    }
}


// * * * * * * * * * * * * * * * Build Macro Function  * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchVectorField,
        moldingVentVelocityFvPatchVectorField
    );
}

// ************************************************************************* //
