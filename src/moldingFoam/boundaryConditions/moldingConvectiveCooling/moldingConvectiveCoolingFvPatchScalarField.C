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

#include "moldingConvectiveCoolingFvPatchScalarField.H"
#include "addToRunTimeSelectionTable.H"
#include "fvPatchFieldMapper.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingConvectiveCoolingFvPatchScalarField::
moldingConvectiveCoolingFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const dictionary& dict
)
:
    mixedFvPatchScalarField(p, iF, dict),
    kappa_(dict.lookup<scalar>("kappa")),
    htc_(dict.lookup<scalar>("htc")),
    Ta_(dict.lookup<scalar>("coolantTemperature"))
{
    if (kappa_ <= 0 || htc_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The convective cooling boundary requires kappa > 0 and "
            << "htc >= 0: kappa = " << kappa_ << ", htc = " << htc_
            << exit(FatalIOError);
    }

    refValue() = Ta_;
    refGrad() = 0;
    valueFraction() = 0;
}


Foam::moldingConvectiveCoolingFvPatchScalarField::
moldingConvectiveCoolingFvPatchScalarField
(
    const moldingConvectiveCoolingFvPatchScalarField& ptf,
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const fieldMapper& mapper
)
:
    mixedFvPatchScalarField(ptf, p, iF, mapper),
    kappa_(ptf.kappa_),
    htc_(ptf.htc_),
    Ta_(ptf.Ta_)
{}


Foam::moldingConvectiveCoolingFvPatchScalarField::
moldingConvectiveCoolingFvPatchScalarField
(
    const moldingConvectiveCoolingFvPatchScalarField& ptf,
    const DimensionedField<scalar, fvMesh>& iF
)
:
    mixedFvPatchScalarField(ptf, iF),
    kappa_(ptf.kappa_),
    htc_(ptf.htc_),
    Ta_(ptf.Ta_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::moldingConvectiveCoolingFvPatchScalarField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    // Implicit Robin condition: -kappa dT/dn = h (Tw - Ta)
    // Tw = (h Ta + k delta Tint)/(h + k delta), so the mixed weight on the
    // coolant reference value is h/(h + k delta)
    const scalarField kDelta(kappa_*patch().deltaCoeffs());

    valueFraction() = htc_/(kDelta + htc_);
    refValue() = Ta_;
    refGrad() = 0;

    mixedFvPatchScalarField::updateCoeffs();
}


void Foam::moldingConvectiveCoolingFvPatchScalarField::write
(
    Ostream& os
) const
{
    mixedFvPatchScalarField::write(os);

    writeEntry(os, "kappa", kappa_);
    writeEntry(os, "htc", htc_);
    writeEntry(os, "coolantTemperature", Ta_);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchScalarField,
        moldingConvectiveCoolingFvPatchScalarField
    );
}

// ************************************************************************* //
