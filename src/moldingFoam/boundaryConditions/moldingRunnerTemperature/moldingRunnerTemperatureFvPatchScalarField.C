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

#include "moldingRunnerTemperatureFvPatchScalarField.H"
#include "moldingStage.H"
#include "addToRunTimeSelectionTable.H"
#include "fvPatchFieldMapper.H"
#include "surfaceFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

moldingRunnerTemperatureFvPatchScalarField::
moldingRunnerTemperatureFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const dictionary& dict
)
:
    fixedValueFvPatchScalarField(p, iF, dict),
    runner_(new dictionary(dict.subDict("runner"))),
    network_(new moldingRunnerNetwork(*runner_)),
    gate_(dict.lookup<word>("gate")),
    totalFlowRate_(dict.lookup<scalar>("totalFlowRate"))
{
    if (totalFlowRate_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The runner-coupled melt temperature requires a positive "
            << "totalFlowRate: totalFlowRate = " << totalFlowRate_
            << exit(FatalIOError);
    }

    label gateIndex(-1);

    for (label g = 0; g < network_->nGates(); ++g)
    {
        if (network_->gate(g).name == gate_)
        {
            gateIndex = g;
        }
    }

    if (gateIndex < 0)
    {
        FatalIOErrorInFunction(*runner_)
            << "The runner network has no gate named " << gate_
            << exit(FatalIOError);
    }
}


moldingRunnerTemperatureFvPatchScalarField::
moldingRunnerTemperatureFvPatchScalarField
(
    const moldingRunnerTemperatureFvPatchScalarField& ptf,
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const fieldMapper& m
)
:
    fixedValueFvPatchScalarField(ptf, p, iF, m),
    runner_(new dictionary(*ptf.runner_)),
    network_(new moldingRunnerNetwork(*runner_)),
    gate_(ptf.gate_),
    totalFlowRate_(ptf.totalFlowRate_)
{}


moldingRunnerTemperatureFvPatchScalarField::
moldingRunnerTemperatureFvPatchScalarField
(
    const moldingRunnerTemperatureFvPatchScalarField& ptf,
    const DimensionedField<scalar, fvMesh>& iF
)
:
    fixedValueFvPatchScalarField(ptf, iF),
    runner_(new dictionary(*ptf.runner_)),
    network_(new moldingRunnerNetwork(*runner_)),
    gate_(ptf.gate_),
    totalFlowRate_(ptf.totalFlowRate_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void moldingRunnerTemperatureFvPatchScalarField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    label gateIndex(-1);

    for (label g = 0; g < network_->nGates(); ++g)
    {
        if (network_->gate(g).name == gate_)
        {
            gateIndex = g;
        }
    }

    scalar Q = totalFlowRate_;
    bool pressureControlled = false;

    if (db().foundObject<moldingStage>(moldingStage::typeName))
    {
        const moldingStage& stage =
            db().lookupObject<moldingStage>(moldingStage::typeName);

        if (stage.packing())
        {
            pressureControlled = true;
        }
    }

    if (pressureControlled)
    {
        const surfaceScalarField& phi =
            db().lookupObject<surfaceScalarField>("phi");

        const fvsPatchField<scalar>& phip =
            patch().patchField<surfaceScalarField, scalar>(phi);

        Q = mag(gSum(phip));
    }

    const scalar Tg = network_->gateTemperature(gateIndex, Q);

    operator==(Tg);

    fixedValueFvPatchScalarField::updateCoeffs();
}


void moldingRunnerTemperatureFvPatchScalarField::write
(
    Ostream& os
) const
{
    fixedValueFvPatchScalarField::write(os);

    writeEntry(os, "totalFlowRate", totalFlowRate_);
    writeEntry(os, "gate", gate_);
    writeEntry(os, "runner", *runner_);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchScalarField,
        moldingRunnerTemperatureFvPatchScalarField
    );
}

// ************************************************************************* //
