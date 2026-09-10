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

#include "moldingSlipVelocityFvPatchVectorField.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingSlipVelocityFvPatchVectorField::
moldingSlipVelocityFvPatchVectorField
(
    const fvPatch& p,
    const DimensionedField<vector, fvMesh>& iF,
    const dictionary& dict
)
:
    partialSlipFvPatchVectorField(p, iF),
    slipLength_(dict.lookup<scalar>("slipLength"))
{
    if (slipLength_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The Navier slip length must be non-negative: slipLength = "
            << slipLength_ << exit(FatalIOError);
    }

    valueFraction() = 1.0/(1.0 + slipLength_*patch().deltaCoeffs());

    if (dict.found("value"))
    {
        fvPatchVectorField::operator=
        (
            vectorField("value", iF.dimensions(), dict, p.size())
        );
    }
    else
    {
        fvPatchVectorField::operator=(patchInternalField());
    }

    evaluate();
}


Foam::moldingSlipVelocityFvPatchVectorField::
moldingSlipVelocityFvPatchVectorField
(
    const moldingSlipVelocityFvPatchVectorField& ptf,
    const fvPatch& p,
    const DimensionedField<vector, fvMesh>& iF,
    const fieldMapper& mapper
)
:
    partialSlipFvPatchVectorField(ptf, p, iF, mapper),
    slipLength_(ptf.slipLength_)
{}


Foam::moldingSlipVelocityFvPatchVectorField::
moldingSlipVelocityFvPatchVectorField
(
    const moldingSlipVelocityFvPatchVectorField& ptf,
    const DimensionedField<vector, fvMesh>& iF
)
:
    partialSlipFvPatchVectorField(ptf, iF),
    slipLength_(ptf.slipLength_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::moldingSlipVelocityFvPatchVectorField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    valueFraction() = 1.0/(1.0 + slipLength_*patch().deltaCoeffs());

    fvPatchField<vector>::updateCoeffs();
}


void Foam::moldingSlipVelocityFvPatchVectorField::write
(
    Ostream& os
) const
{
    partialSlipFvPatchVectorField::write(os);
    writeEntry(os, "slipLength", slipLength_);
    writeEntry(os, "value", *this);
}


// * * * * * * * * * * * * * * * Build Macro Function  * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchVectorField,
        moldingSlipVelocityFvPatchVectorField
    );
}

// ************************************************************************* //
