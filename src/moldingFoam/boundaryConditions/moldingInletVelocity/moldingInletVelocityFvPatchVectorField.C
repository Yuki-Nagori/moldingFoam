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

namespace
{

//- Deep copy a Function1 held by autoPtr (the autoPtr copy would transfer
//  ownership and crash later writes of the source)
Foam::autoPtr<Foam::Function1<Foam::scalar>> cloneProfile
(
    const Foam::autoPtr<Foam::Function1<Foam::scalar>>& p
)
{
    return
        p.valid()
      ? Foam::autoPtr<Foam::Function1<Foam::scalar>>(p->clone().ptr())
      : Foam::autoPtr<Foam::Function1<Foam::scalar>>();
}

} // End anonymous namespace
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
    flowRateProfile_
    (
        dict.found("volumetricFlowRateProfile")
      ? Function1<scalar>::New
        (
            "volumetricFlowRateProfile",
            time().userUnits(),
            dimensionSet(0, 3, -1, 0, 0, 0, 0),
            dict
        )
      : autoPtr<Function1<scalar>>()
    ),
    area_(-1),
    runner_
    (
        dict.found("runner")
      ? autoPtr<dictionary>(new dictionary(dict.subDict("runner")))
      : autoPtr<dictionary>()
    ),
    network_
    (
        runner_.valid()
      ? autoPtr<moldingRunnerNetwork>(new moldingRunnerNetwork(*runner_))
      : autoPtr<moldingRunnerNetwork>()
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

        for (label g = 0; g < network_->nGates(); ++g)
        {
            if (network_->gate(g).name == gate_)
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
    else if (volumetricFlowRate_ <= 0 && !flowRateProfile_.valid())
    {
        FatalIOErrorInFunction(dict)
            << "The moldingInletVelocity condition requires a positive "
            << "volumetricFlowRate, a volumetricFlowRateProfile or a "
            << "runner network: volumetricFlowRate = "
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
    flowRateProfile_(cloneProfile(pivpvf.flowRateProfile_)),
    area_(pivpvf.area_),
    runner_
    (
        pivpvf.runner_.valid()
      ? autoPtr<dictionary>(new dictionary(*pivpvf.runner_))
      : autoPtr<dictionary>()
    ),
    network_
    (
        pivpvf.network_.valid()
      ? autoPtr<moldingRunnerNetwork>(new moldingRunnerNetwork(*pivpvf.network_))
      : autoPtr<moldingRunnerNetwork>()
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
    flowRateProfile_(cloneProfile(pivpvf.flowRateProfile_)),
    area_(pivpvf.area_),
    runner_
    (
        pivpvf.runner_.valid()
      ? autoPtr<dictionary>(new dictionary(*pivpvf.runner_))
      : autoPtr<dictionary>()
    ),
    network_
    (
        pivpvf.network_.valid()
      ? autoPtr<moldingRunnerNetwork>(new moldingRunnerNetwork(*pivpvf.network_))
      : autoPtr<moldingRunnerNetwork>()
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

    // Gate seal ramp factor: 1 before the seal, decreasing to 0 over the
    // optional gateSealRamp, so the gate flux is ramped down smoothly
    const scalar sealFactor(stage.gateSealFactor(patch().time().value()));

    if (sealFactor <= 0)
    {
        // The gate has frozen off at the end of packing: no through-flow
        operator==(vector::zero);
    }
    else if (stage.packing())
    {
        // Packing: pressure controlled; the velocity follows the local
        // volumetric flux computed by the pressure equation, which is
        // driven by the packing pressure target imposed by the
        // moldingPrghPressure boundary condition, scaled by the seal ramp
        const surfaceScalarField& phi =
            db().lookupObject<surfaceScalarField>("phi");

        const fvsPatchField<scalar>& phip =
            patch().patchField<surfaceScalarField, scalar>(phi);

        operator==(patch().nf()*phip/patch().magSf()*sealFactor);
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

        if (flowRateProfile_.valid())
        {
            Q = flowRateProfile_->value(patch().time().value());
        }

        if (network_.valid())
        {
            Q = network_->gateFlow(gateIndex_, totalFlowRate_);
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
