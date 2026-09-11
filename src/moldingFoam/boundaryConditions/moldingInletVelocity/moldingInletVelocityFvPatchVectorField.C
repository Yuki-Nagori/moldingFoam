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
#include "moldingRunnerNetwork.H"
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
    volumetricFlowRate_(dict.lookupOrDefault<scalar>("volumetricFlowRate", 0)),
    area_(-1),
    runner_
    (
        dict.found("runner")
      ? autoPtr<dictionary>(new dictionary(dict.subDict("runner")))
      : autoPtr<dictionary>()
    ),
    gate_(dict.lookupOrDefault<word>("gate", word::null)),
    gateIndex_(-1),
    totalFlowRate_(dict.lookupOrDefault<scalar>("totalFlowRate", 0))
{
    if (runner_.valid())
    {
        if (gate_ == word::null)
        {
            FatalIOErrorInFunction(dict)
                << "The runner-coupled moldingInletVelocity condition "
                << "requires the gate name: gate" << exit(FatalIOError);
        }

        if (totalFlowRate_ <= 0)
        {
            FatalIOErrorInFunction(dict)
                << "The runner-coupled moldingInletVelocity condition "
                << "requires a positive totalFlowRate: totalFlowRate = "
                << totalFlowRate_ << exit(FatalIOError);
        }

        const moldingRunnerNetwork network(*runner_);

        for (label g = 0; g < network.nGates(); ++g)
        {
            if (network.gate(g).name == gate_)
            {
                gateIndex_ = g;
            }
        }

        if (gateIndex_ < 0)
        {
            FatalIOErrorInFunction(*runner_)
                << "The runner network has no gate named " << gate_
                << exit(FatalIOError);
        }
    }
    else if (volumetricFlowRate_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The moldingInletVelocity condition requires a positive "
            << "volumetricFlowRate or a runner network: volumetricFlowRate = "
            << volumetricFlowRate_ << exit(FatalIOError);
    }
}


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
    area_(pivpvf.area_),
    runner_
    (
        pivpvf.runner_.valid()
      ? autoPtr<dictionary>(new dictionary(*pivpvf.runner_))
      : autoPtr<dictionary>()
    ),
    gate_(pivpvf.gate_),
    gateIndex_(pivpvf.gateIndex_),
    totalFlowRate_(pivpvf.totalFlowRate_)
{}


moldingInletVelocityFvPatchVectorField::moldingInletVelocityFvPatchVectorField
(
    const moldingInletVelocityFvPatchVectorField& pivpvf,
    const DimensionedField<vector, fvMesh>& iF
)
:
    fixedValueFvPatchVectorField(pivpvf, iF),
    volumetricFlowRate_(pivpvf.volumetricFlowRate_),
    area_(pivpvf.area_),
    runner_
    (
        pivpvf.runner_.valid()
      ? autoPtr<dictionary>(new dictionary(*pivpvf.runner_))
      : autoPtr<dictionary>()
    ),
    gate_(pivpvf.gate_),
    gateIndex_(pivpvf.gateIndex_),
    totalFlowRate_(pivpvf.totalFlowRate_)
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

    if (stage.gateSealed())
    {
        // The gate has frozen off at the end of packing: no through-flow
        operator==(vector::zero);
    }
    else if (stage.packing())
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
        // Filling: flow-rate controlled injection, either directly or
        // through the equal-pressure-drop split of the runner network
        if (area_ < 0)
        {
            area_ = gSum(mag(patch().Sf()));
        }

        scalar Q = volumetricFlowRate_;

        if (runner_.valid())
        {
            const moldingRunnerNetwork network(*runner_);

            Q = network.gateFlow(gateIndex_, totalFlowRate_);
        }

        operator==(patch().nf()*(-Q/area_));
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
