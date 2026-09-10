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
#include "fieldMapper.H"
#include "thermophysicalTransportModel.H"
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
    )
{
    if (C_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The mould heat capacity must be positive: heatCapacity = "
            << C_ << exit(FatalIOError);
    }

    if (waterHTC_ < 0 || wettedArea_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The cooling-water parameters must be non-negative: "
            << "waterHTC = " << waterHTC_
            << ", wettedArea = " << wettedArea_
            << exit(FatalIOError);
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
    T_(ptf.T_),
    Q_(cloneQ(ptf.Q_))
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
    T_(ptf.T_),
    Q_(cloneQ(ptf.Q_))
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

    T_.value() = moldThermalState::Tnew
    (
        C_,
        waterHTC_*wettedArea_,
        Tw_,
        T_.oldTime().value(),
        hFilm,
        Tfilm,
        time().deltaTValue(),
        Q_->value(time().value())
    );

    operator==(T_.value());

    fixedValueFvPatchScalarField::updateCoeffs();
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
