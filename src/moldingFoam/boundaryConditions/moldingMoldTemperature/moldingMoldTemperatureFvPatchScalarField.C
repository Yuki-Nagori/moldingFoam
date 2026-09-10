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

#include "moldingMoldTemperatureFvPatchScalarField.H"
#include "moldThermalState.H"
#include "moldingCoolantChannel.H"
#include "fieldMapper.H"
#include "thermophysicalTransportModel.H"
#include "Pstream.H"
#include "ListOps.H"
#include "dictionary.H"
#include "ZeroConstant.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

//- Deep copy a Function1 held by autoPtr; the OpenFOAM autoPtr copy would
//  transfer ownership and leave the source unallocated (crashes any later
//  write of the source, e.g. in decomposePar)
static autoPtr<Function1<scalar>> cloneQ
(
    const autoPtr<Function1<scalar>>& Q
)
{
    return
        Q.valid()
      ? autoPtr<Function1<scalar>>(Q->clone().ptr())
      : autoPtr<Function1<scalar>>();
}

} // End namespace Foam


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingMoldTemperatureFvPatchScalarField::
moldingMoldTemperatureFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const dictionary& dict
)
:
    fixedValueFvPatchScalarField(p, iF),
    C_(dict.lookup<scalar>("heatCapacity")),
    waterHTC_(dict.lookupOrDefault<scalar>("waterHTC", 0)),
    wettedArea_(dict.lookupOrDefault<scalar>("wettedArea", 0)),
    Tw_(dict.lookupOrDefault<scalar>("waterTemperature", 300)),
    wallResistance_(dict.lookupOrDefault<scalar>("wallResistance", 0)),
    deepMoldTemperature_
    (
        dict.lookupOrDefault<scalar>("deepMoldTemperature", 300)
    ),
    T_
    (
        IOobject
        (
            "T_" + patch().name(),
            time().name(),
            db()
        ),
        dimensionedScalar(dimTemperature, dict.lookup<scalar>("T"))
    ),
    Q_
    (
        dict.found("Q")
      ? Function1<scalar>::New
        (
            "Q",
            time().userUnits(),
            dimPower,
            dict
        )
      : autoPtr<Function1<scalar>>(new Function1s::ZeroConstant<scalar>("Q"))
    ),
    coolant_
    (
        dict.found("coolant")
      ? autoPtr<dictionary>(new dictionary(dict.subDict("coolant")))
      : autoPtr<dictionary>()
    ),
    mdot_(0),
    cp_(0),
    inletTemperature_(300),
    direction_(vector::zero),
    htc_(0),
    groupHA_(),
    hCoolant_(0),
    channelReady_(false)
{
    if (C_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The mould heat capacity must be positive: heatCapacity = "
            << C_ << exit(FatalIOError);
    }

    if (wallResistance_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The wall thermal resistance must be non-negative: "
            << "wallResistance = " << wallResistance_
            << exit(FatalIOError);
    }

    if (waterHTC_ < 0 || wettedArea_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The cooling-water parameters must be non-negative: "
            << "waterHTC = " << waterHTC_
            << ", wettedArea = " << wettedArea_
            << exit(FatalIOError);
    }

    if (coolant_.valid())
    {
        const dictionary& coolant = *coolant_;

        mdot_ = coolant.lookup<scalar>("massFlowRate");
        cp_ = coolant.lookup<scalar>("cp");
        inletTemperature_ = coolant.lookup<scalar>("inletTemperature");
        direction_ = coolant.lookup<vector>("direction");

        if (mdot_ <= 0)
        {
            FatalIOErrorInFunction(coolant)
                << "The coolant mass flow rate must be positive: "
                << "massFlowRate = " << mdot_ << exit(FatalIOError);
        }

        if (cp_ <= 0)
        {
            FatalIOErrorInFunction(coolant)
                << "The coolant specific heat must be positive: cp = "
                << cp_ << exit(FatalIOError);
        }

        if (mag(direction_) <= small)
        {
            FatalIOErrorInFunction(coolant)
                << "The coolant channel direction must be non-zero: "
                << "direction = " << direction_ << exit(FatalIOError);
        }

        direction_ /= mag(direction_);

        if (coolant.found("htc"))
        {
            htc_ = coolant.lookup<scalar>("htc");
        }
        else
        {
            const dictionary& nuDict = coolant.subDict("Nu");

            const scalar C = nuDict.lookup<scalar>("C");
            const scalar m = nuDict.lookup<scalar>("m");
            const scalar n = nuDict.lookup<scalar>("n");
            const scalar Re = nuDict.lookup<scalar>("Re");
            const scalar Pr = nuDict.lookup<scalar>("Pr");
            const scalar k = nuDict.lookup<scalar>("k");
            const scalar D = nuDict.lookup<scalar>("D");

            if (Re <= 0 || Pr <= 0 || k <= 0 || D <= 0)
            {
                FatalIOErrorInFunction(nuDict)
                    << "The Nusselt correlation requires positive Re, Pr, "
                    << "k and D: Re = " << Re << ", Pr = " << Pr
                    << ", k = " << k << ", D = " << D
                    << exit(FatalIOError);
            }

            htc_ = moldingCoolantChannel::htcFromNu
            (
                moldingCoolantChannel::Nu(C, m, n, Re, Pr),
                k,
                D
            );
        }

        if (htc_ < 0)
        {
            FatalIOErrorInFunction(coolant)
                << "The coolant heat transfer coefficient must be "
                << "non-negative: htc = " << htc_ << exit(FatalIOError);
        }
    }

    fvPatchScalarField::operator=(T_.value());
}


Foam::moldingMoldTemperatureFvPatchScalarField::
moldingMoldTemperatureFvPatchScalarField
(
    const moldingMoldTemperatureFvPatchScalarField& ptf,
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const fieldMapper& mapper
)
:
    fixedValueFvPatchScalarField(ptf, p, iF, mapper),
    C_(ptf.C_),
    waterHTC_(ptf.waterHTC_),
    wettedArea_(ptf.wettedArea_),
    Tw_(ptf.Tw_),
    wallResistance_(ptf.wallResistance_),
    deepMoldTemperature_(ptf.deepMoldTemperature_),
    T_(ptf.T_),
    Q_(cloneQ(ptf.Q_)),
    coolant_
    (
        ptf.coolant_.valid()
      ? autoPtr<dictionary>(new dictionary(*ptf.coolant_))
      : autoPtr<dictionary>()
    ),
    mdot_(ptf.mdot_),
    cp_(ptf.cp_),
    inletTemperature_(ptf.inletTemperature_),
    direction_(ptf.direction_),
    htc_(ptf.htc_),
    groupHA_(),
    hCoolant_(0),
    channelReady_(false)
{}


Foam::moldingMoldTemperatureFvPatchScalarField::
moldingMoldTemperatureFvPatchScalarField
(
    const moldingMoldTemperatureFvPatchScalarField& ptf,
    const DimensionedField<scalar, fvMesh>& iF
)
:
    fixedValueFvPatchScalarField(ptf, iF),
    C_(ptf.C_),
    waterHTC_(ptf.waterHTC_),
    wettedArea_(ptf.wettedArea_),
    Tw_(ptf.Tw_),
    wallResistance_(ptf.wallResistance_),
    deepMoldTemperature_(ptf.deepMoldTemperature_),
    T_(ptf.T_),
    Q_(cloneQ(ptf.Q_)),
    coolant_
    (
        ptf.coolant_.valid()
      ? autoPtr<dictionary>(new dictionary(*ptf.coolant_))
      : autoPtr<dictionary>()
    ),
    mdot_(ptf.mdot_),
    cp_(ptf.cp_),
    inletTemperature_(ptf.inletTemperature_),
    direction_(ptf.direction_),
    htc_(ptf.htc_),
    groupHA_(ptf.groupHA_),
    hCoolant_(ptf.hCoolant_),
    channelReady_(ptf.channelReady_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::moldingMoldTemperatureFvPatchScalarField::map
(
    const fvPatchScalarField& ptf,
    const fieldMapper& mapper
)
{
    fixedValueFvPatchScalarField::map(ptf, mapper);
}


void Foam::moldingMoldTemperatureFvPatchScalarField::reset
(
    const fvPatchScalarField& ptf
)
{
    fixedValueFvPatchScalarField::reset(ptf);
}


void Foam::moldingMoldTemperatureFvPatchScalarField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    const thermophysicalTransportModel& ttm =
        db().lookupType<thermophysicalTransportModel>
        (
            internalField().group()
        );

    // Casting-side face conductances, identical to the implicit boundary
    // coefficients of the energy equation
    const scalarField Hf
    (
        ttm.kappaEff(patch().index())
       *patch().magSf()
       *patch().deltaCoeffs()
    );

    const scalar hFilm(gSum(Hf));
    const scalar Tfilm
    (
        hFilm > small
      ? gSum(Hf*patchInternalField())/hFilm
      : T_.value()
    );

    // Cooling path and the optional deep-mould path through the wall
    // thermal resistance; both are combined into one conductance and a
    // conductance-weighted driving term
    const scalar hDeep
    (
        wallResistance_ > 0
      ? gSum(patch().magSf())/wallResistance_
      : scalar(0)
    );

    scalar hWater = 0;
    scalar hATw = 0;

    if (coolant_.valid())
    {
        if (!channelReady_)
        {
            buildChannel();
        }

        // March the channel cross-sections from the previous mould
        // temperature; the state update below then treats the coolant
        // path implicitly for those section temperatures
        const scalarField groupTw(groupHA_.size(), T_.oldTime().value());
        List<scalar> groupTc;

        hATw = moldingCoolantChannel::march
        (
            inletTemperature_,
            mdot_*cp_,
            groupHA_,
            groupTw,
            groupTc
        );
        hWater = hCoolant_;
    }
    else
    {
        hWater = waterHTC_*wettedArea_;
        hATw = hWater*Tw_;
    }

    const scalar hA = hWater + hDeep;
    hATw += hDeep*deepMoldTemperature_;

    T_.value() = moldThermalState::Tnew
    (
        C_,
        hA,
        hA > small ? hATw/hA : T_.value(),
        T_.oldTime().value(),
        hFilm,
        Tfilm,
        time().deltaTValue(),
        Q_->value(time().value())
    );

    operator==(T_.value());

    fixedValueFvPatchScalarField::updateCoeffs();
}


void Foam::moldingMoldTemperatureFvPatchScalarField::buildChannel()
{
    // Gather the patch faces of every processor (rank order) so that the
    // channel can be sorted and grouped globally; the geometry and the
    // HTC are constant, so this runs once per patch
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

    hCoolant_ = 0;

    forAll(groupHA_, g)
    {
        hCoolant_ += groupHA_[g];
    }

    channelReady_ = true;
}


void Foam::moldingMoldTemperatureFvPatchScalarField::write
(
    Ostream& os
) const
{
    fvPatchScalarField::write(os);
    writeEntry(os, "heatCapacity", C_);
    writeEntry(os, "waterHTC", waterHTC_);
    writeEntry(os, "wettedArea", wettedArea_);
    writeEntry(os, "waterTemperature", Tw_);
    if (coolant_.valid())
    {
        writeEntry(os, "coolant", *coolant_);
    }
    if (wallResistance_ > 0)
    {
        writeEntry(os, "wallResistance", wallResistance_);
        writeEntry(os, "deepMoldTemperature", deepMoldTemperature_);
    }
    writeEntry(os, "T", T_.value());
    writeEntry(os, Q_());
    writeEntry(os, "value", *this);
}


// * * * * * * * * * * * * * * * Build Macro Function  * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchScalarField,
        moldingMoldTemperatureFvPatchScalarField
    );
}

// ************************************************************************* //
