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

#include "moldingChannelCoolingFvPatchScalarField.H"
#include "moldingCoolantChannel.H"
#include "volFields.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * * * //

moldingChannelCoolingFvPatchScalarField::
moldingChannelCoolingFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const dictionary& dict
)
:
    mixedFvPatchScalarField(p, iF),
    kappa_(dict.lookup<scalar>("kappa")),
    htc_(dict.lookup<scalar>("htc")),
    mdot_(dict.lookup<scalar>("massFlowRate")),
    cp_(dict.lookup<scalar>("cp")),
    inletTemperature_(dict.lookup<scalar>("inletTemperature")),
    direction_(dict.lookupOrDefault<vector>("direction", vector(1, 0, 0))),
    groupPos_(),
    groupHA_(),
    channelReady_(false)
{
    refValue() = *this;
    refGrad() = 0;
    valueFraction() = 0;

    if (kappa_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The solid conductivity must be positive: kappa = "
            << kappa_ << exit(FatalIOError);
    }

    if (htc_ <= 0 || mdot_ <= 0 || cp_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The HTC, mass flow rate and heat capacity must be "
            << "positive: htc = " << htc_ << ", massFlowRate = " << mdot_
            << ", cp = " << cp_ << exit(FatalIOError);
    }

    if (mag(direction_) < small)
    {
        FatalIOErrorInFunction(dict)
            << "The channel direction must be non-zero: direction = "
            << direction_ << exit(FatalIOError);
    }
}


moldingChannelCoolingFvPatchScalarField::
moldingChannelCoolingFvPatchScalarField
(
    const moldingChannelCoolingFvPatchScalarField& ptf,
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const fvPatchFieldMapper& mapper
)
:
    mixedFvPatchScalarField(ptf, p, iF, mapper),
    kappa_(ptf.kappa_),
    htc_(ptf.htc_),
    mdot_(ptf.mdot_),
    cp_(ptf.cp_),
    inletTemperature_(ptf.inletTemperature_),
    direction_(ptf.direction_),
    groupPos_(),
    groupHA_(),
    channelReady_(false)
{}


moldingChannelCoolingFvPatchScalarField::
moldingChannelCoolingFvPatchScalarField
(
    const moldingChannelCoolingFvPatchScalarField& ptf,
    const DimensionedField<scalar, fvMesh>& iF
)
:
    mixedFvPatchScalarField(ptf, iF),
    kappa_(ptf.kappa_),
    htc_(ptf.htc_),
    mdot_(ptf.mdot_),
    cp_(ptf.cp_),
    inletTemperature_(ptf.inletTemperature_),
    direction_(ptf.direction_),
    groupPos_(ptf.groupPos_),
    groupHA_(ptf.groupHA_),
    channelReady_(ptf.channelReady_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::moldingChannelCoolingFvPatchScalarField::buildChannel()
{
    const label nProcs = Pstream::nProcs();
    const label myProc = Pstream::myProcNo();

    List<List<scalar>> procMagSf(nProcs);
    List<List<scalar>> procProj(nProcs);

    procMagSf[myProc] = patch().magSf();
    procProj[myProc].setSize(patch().size());

    forAll(procProj[myProc], i)
    {
        procProj[myProc][i] = patch().Cf()[i] & direction_;
    }

    Pstream::gatherList(procMagSf);
    Pstream::scatterList(procMagSf);
    Pstream::gatherList(procProj);
    Pstream::scatterList(procProj);

    List<scalar> allMagSf;
    List<scalar> allProj;

    forAll(procMagSf, r)
    {
        allMagSf.append(procMagSf[r]);
        allProj.append(procProj[r]);
    }

    labelList order;
    sortedOrder(allProj, order);

    List<scalar> sMagSf(order.size());
    List<scalar> sProj(order.size());

    forAll(order, k)
    {
        sMagSf[k] = allMagSf[order[k]];
        sProj[k] = allProj[order[k]];
    }

    const scalar span = gMax(allProj) - gMin(allProj);
    const scalar tol = 1e-8*max(span, small);

    labelList groupStart;
    moldingCoolantChannel::group
    (
        sMagSf,
        sProj,
        htc_,
        tol,
        groupHA_,
        groupStart
    );

    groupPos_.setSize(groupHA_.size());

    forAll(groupPos_, g)
    {
        groupPos_[g] = sProj[groupStart[g]];
    }

    channelReady_ = true;
}


void Foam::moldingChannelCoolingFvPatchScalarField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    if (!channelReady_)
    {
        buildChannel();
    }

    const label nGroups = groupHA_.size();

    // Wall temperature per section from the patch internal field
    // Copy the patch internal values: patchInternalField() returns a
    // temporary which must not be bound to a reference
    const scalarField Tint(patchInternalField());
    const vectorField& Cf = patch().Cf();

    List<scalar> wallSum(nGroups, scalar(0));
    List<scalar> wallCount(nGroups, scalar(0));
    labelList faceGroup(Cf.size(), label(-1));

    forAll(Cf, i)
    {
        const scalar proj = Cf[i] & direction_;

        // Nearest section (the group positions are the exact face
        // positions, so the match is exact)
        label gMin = 0;
        scalar dMin = great;

        forAll(groupPos_, g)
        {
            const scalar d = mag(proj - groupPos_[g]);

            if (d < dMin)
            {
                dMin = d;
                gMin = g;
            }
        }

        faceGroup[i] = gMin;
        wallSum[gMin] += Tint[i];
        wallCount[gMin] += 1;
    }

    // Global section wall temperatures (identical on every rank)
    List<scalar> groupTw(nGroups, scalar(0));

    forAll(groupTw, g)
    {
        const scalar s = returnReduce(wallSum[g], sumOp<scalar>());
        const scalar n = returnReduce(wallCount[g], sumOp<scalar>());
        groupTw[g] = s/max(n, 1);
    }

    // March the channel from the inlet
    List<scalar> groupTc;
    moldingCoolantChannel::march
    (
        inletTemperature_,
        mdot_*cp_,
        groupHA_,
        groupTw,
        groupTc
    );

    if (Pstream::master() && time().timeIndex() % 100 == 0 && nGroups > 0)
    {
        const label g = nGroups - 1;
        const scalar Tout =
            groupTc[g] + groupHA_[g]*(groupTw[g] - groupTc[g])/(mdot_*cp_);

        Info<< "moldingChannelCooling: Tc in/out = " << inletTemperature_
            << " / " << Tout << " K, pick-up = "
            << mdot_*cp_*(Tout - inletTemperature_) << " W" << endl;
    }

    // Implicit Robin with the local coolant bulk temperature as reference
    const scalarField kDelta(kappa_*patch().deltaCoeffs());

    scalarField& rv = refValue();
    scalarField& vf = valueFraction();

    forAll(rv, i)
    {
        rv[i] = groupTc[faceGroup[i]];
        vf[i] = htc_/(kDelta[i] + htc_);
    }

    refGrad() = 0;

    mixedFvPatchScalarField::updateCoeffs();
}


void Foam::moldingChannelCoolingFvPatchScalarField::write
(
    Ostream& os
) const
{
    mixedFvPatchScalarField::write(os);

    writeEntry(os, "kappa", kappa_);
    writeEntry(os, "htc", htc_);
    writeEntry(os, "massFlowRate", mdot_);
    writeEntry(os, "cp", cp_);
    writeEntry(os, "inletTemperature", inletTemperature_);
    writeEntry(os, "direction", direction_);
    writeEntry(os, "value", *this);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

makePatchTypeField
(
    fvPatchScalarField,
    moldingChannelCoolingFvPatchScalarField
);

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
