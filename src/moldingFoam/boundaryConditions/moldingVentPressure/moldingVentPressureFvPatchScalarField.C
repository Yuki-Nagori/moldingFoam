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

#include "moldingVentPressureFvPatchScalarField.H"
#include "moldingStage.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingVentPressureFvPatchScalarField::
moldingVentPressureFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const dictionary& dict
)
:
    mixedFvPatchScalarField(p, iF, dict, false),
    p0_(dict.lookup<scalar>("p0"))
{
    refValue() = p0_;
    refGrad() = 0;
    valueFraction() = 1;

    if (dict.found("value"))
    {
        mixedFvPatchScalarField::operator==
        (
            Field<scalar>("value", iF.dimensions(), dict, p.size())
        );
    }
    else
    {
        mixedFvPatchScalarField::operator==(patchInternalField());
    }
}


Foam::moldingVentPressureFvPatchScalarField::
moldingVentPressureFvPatchScalarField
(
    const moldingVentPressureFvPatchScalarField& ptf,
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const fieldMapper& mapper
)
:
    mixedFvPatchScalarField(ptf, p, iF, mapper),
    p0_(ptf.p0_)
{}


Foam::moldingVentPressureFvPatchScalarField::
moldingVentPressureFvPatchScalarField
(
    const moldingVentPressureFvPatchScalarField& ptf,
    const DimensionedField<scalar, fvMesh>& iF
)
:
    mixedFvPatchScalarField(ptf, iF),
    p0_(ptf.p0_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::moldingVentPressureFvPatchScalarField::updateCoeffs()
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
        // Melt front at the vent: no through-flow (the matching
        // moldingVentVelocity condition sets the velocity to zero)
        valueFraction() = 0;
        refGrad() = 0;
    }
    else
    {
        // Open vent: fixed atmospheric pressure
        valueFraction() = 1;
        refValue() = p0_;
        refGrad() = 0;
    }

    mixedFvPatchScalarField::updateCoeffs();
}


void Foam::moldingVentPressureFvPatchScalarField::write
(
    Ostream& os
) const
{
    mixedFvPatchScalarField::write(os);
    writeEntry(os, "p0", p0_);
    writeEntry(os, "value", *this);
}


// * * * * * * * * * * * * * * * Build Macro Function  * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchScalarField,
        moldingVentPressureFvPatchScalarField
    );
}

// ************************************************************************* //
