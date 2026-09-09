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

#include "moldingInletVelocityFvPatchVectorField.H"
#include "moldingStage.H"
#include "addToRunTimeSelectionTable.H"
#include "fvPatchFieldMapper.H"
#include "surfaceFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

moldingInletVelocityFvPatchVectorField::moldingInletVelocityFvPatchVectorField
(
    const fvPatch& p,
    const DimensionedField<vector, fvMesh>& iF,
    const dictionary& dict
)
:
    fixedValueFvPatchVectorField(p, iF, dict),
    volumetricFlowRate_(dict.lookup<scalar>("volumetricFlowRate")),
    area_(-1)
{}


moldingInletVelocityFvPatchVectorField::moldingInletVelocityFvPatchVectorField
(
    const moldingInletVelocityFvPatchVectorField& pivpvf,
    const fvPatch& p,
    const DimensionedField<vector, fvMesh>& iF,
    const fieldMapper& m
)
:
    fixedValueFvPatchVectorField(pivpvf, p, iF, m),
    volumetricFlowRate_(pivpvf.volumetricFlowRate_),
    area_(pivpvf.area_)
{}


moldingInletVelocityFvPatchVectorField::moldingInletVelocityFvPatchVectorField
(
    const moldingInletVelocityFvPatchVectorField& pivpvf,
    const DimensionedField<vector, fvMesh>& iF
)
:
    fixedValueFvPatchVectorField(pivpvf, iF),
    volumetricFlowRate_(pivpvf.volumetricFlowRate_),
    area_(pivpvf.area_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void moldingInletVelocityFvPatchVectorField::updateCoeffs()
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
        // Packing: pressure controlled; the velocity follows the local
        // volumetric flux computed by the pressure equation, which is
        // driven by the packing pressure target imposed by the
        // moldingPrghPressure boundary condition
        const surfaceScalarField& phi =
            db().lookupObject<surfaceScalarField>("phi");

        const fvsPatchField<scalar>& phip =
            patch().patchField<surfaceScalarField, scalar>(phi);

        operator==(patch().nf()*phip/patch().magSf());
    }
    else
    {
        // Filling: flow-rate controlled injection
        if (area_ < 0)
        {
            area_ = gSum(mag(patch().Sf()));
        }

        operator==(patch().nf()*(-volumetricFlowRate_/area_));
    }

    fixedValueFvPatchVectorField::updateCoeffs();
}


void moldingInletVelocityFvPatchVectorField::write(Ostream& os) const
{
    fixedValueFvPatchVectorField::write(os);

    writeEntry(os, "volumetricFlowRate", volumetricFlowRate_);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchVectorField,
        moldingInletVelocityFvPatchVectorField
    );
}


// ************************************************************************* //
