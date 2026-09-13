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
#include "moldingRunnerNetwork.H"
#include "surfaceFields.H"
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
    mixedFvPatchScalarField(p, iF, dict),
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
    relaxation_(dict.lookupOrDefault<scalar>("relaxation", 1)),
    pSwitch_(0),
    tSwitch_(-1),
    rampArmed_(false)
{
    if (relaxation_ <= 0 || relaxation_ > 1)
    {
        FatalIOErrorInFunction(dict)
            << "The packing pressure relaxation must lie in (0, 1]: "
            << "relaxation = " << relaxation_ << exit(FatalIOError);
    }

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
    mixedFvPatchScalarField(mpppsf, p, iF, m),
    runner_
    (
        mpppsf.runner_.valid()
      ? autoPtr<dictionary>(new dictionary(*mpppsf.runner_))
      : autoPtr<dictionary>()
    ),
    network_
    (
        mpppsf.network_.valid()
      ? autoPtr<moldingRunnerNetwork>(new moldingRunnerNetwork(*mpppsf.network_))
      : autoPtr<moldingRunnerNetwork>()
    ),
    relaxation_(mpppsf.relaxation_),
    pSwitch_(mpppsf.pSwitch_),
    tSwitch_(mpppsf.tSwitch_),
    rampArmed_(mpppsf.rampArmed_)
{}


moldingPrghPressureFvPatchScalarField::moldingPrghPressureFvPatchScalarField
(
    const moldingPrghPressureFvPatchScalarField& mpppsf,
    const DimensionedField<scalar, fvMesh>& iF
)
:
    mixedFvPatchScalarField(mpppsf, iF),
    runner_
    (
        mpppsf.runner_.valid()
      ? autoPtr<dictionary>(new dictionary(*mpppsf.runner_))
      : autoPtr<dictionary>()
    ),
    network_
    (
        mpppsf.network_.valid()
      ? autoPtr<moldingRunnerNetwork>(new moldingRunnerNetwork(*mpppsf.network_))
      : autoPtr<moldingRunnerNetwork>()
    ),
    relaxation_(mpppsf.relaxation_),
    pSwitch_(mpppsf.pSwitch_),
    tSwitch_(mpppsf.tSwitch_),
    rampArmed_(mpppsf.rampArmed_)
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

    // Gate seal ramp factor: the Dirichlet weight decreases from 1 to 0
    // over the optional gateSealRamp, so the fixed-pressure gate blends
    // smoothly into the zero-flux sealed gate
    const scalar sealFactor(stage.gateSealFactor(patch().time().value()));

    if (sealFactor <= 0)
    {
        // The gate has frozen off at the end of packing: zero normal
        // flux, enforced together with the zero-velocity
        // moldingInletVelocity condition
        rampArmed_ = false;
        valueFraction() = 0.0;
        refGrad() = 0.0;
    }
    else if (stage.packing())
    {
        // Packing: prescribe the packing pressure target at the gate.
        // p_rgh = p - rho*(g.h); for the thin cavities of the moldingFoam
        // contract the hydrostatic head is negligible, so p_rgh equals the
        // target pressure. With a runner network the machine target is
        // reduced by the runner pressure drop at the current gate flow.
        scalar pTarget(stage.pressure(patch().time().value()));

        if (network_.valid())
        {
            const surfaceScalarField& phi =
                db().lookupObject<surfaceScalarField>("phi");

            const fvsPatchField<scalar>& phip =
                patch().patchField<surfaceScalarField, scalar>(phi);

            pTarget -= network_->pressureDrop(mag(gSum(phip)));
        }

        // Ramp from the gate pressure captured when packing started:
        // a fill-fraction-triggered switch happens while the gate
        // pressure is well below the packing table's first point, and
        // applying that step in one update drives the melt transonic
        const scalar t = patch().time().value();

        if (!rampArmed_)
        {
            // Start the ramp from the first packing update: the switch
            // may be several steps in the past by then, and clocking
            // from the switch time would apply a fraction of the step
            // immediately
            rampArmed_ = true;
            tSwitch_ = t;
            pSwitch_ = gAverage(*this);
        }

        const scalar rampTime(stage.pressureRamp());
        const scalar ramp
        (
            rampTime > 0
          ? min(scalar(1), (t - tSwitch_)/rampTime)
          : scalar(1)
        );

        pTarget = pSwitch_ + ramp*(pTarget - pSwitch_);

        valueFraction() = sealFactor;

        // Optional relaxation of the target towards the current value
        if (relaxation_ < 1)
        {
            refValue() =
                (1 - relaxation_)*Field<scalar>(*this)
              + relaxation_*pTarget;
        }
        else
        {
            refValue() = pTarget;
        }

        refGrad() = 0.0;
    }
    else
    {
        // Filling: the flow is volumetric-flow-rate controlled by the
        // moldingInletVelocity boundary condition, so the gate pressure
        // gradient is left to the pressure equation
        rampArmed_ = false;
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
