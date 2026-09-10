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

#include "moldingVentPressureFvPatchScalarField.H"
#include "moldingStage.H"
#include "ventOrifice.H"
#include "addToRunTimeSelectionTable.H"
#include "surfaceFields.H"
#include "volFields.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingVentPressureFvPatchScalarField::
moldingVentPressureFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const dictionary& dict
)
:
    mixedFvPatchScalarField(p, iF, dict, false),
    p0_(dict.lookup<scalar>("p0")),
    CdA_(dict.lookupOrDefault<scalar>("CdA", 0))
{
    if (CdA_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The vent effective discharge area must be non-negative: "
            << "CdA = " << CdA_ << exit(FatalIOError);
    }

    refValue() = p0_;
    refGrad() = 0;
    valueFraction() = 1;

    if (dict.found("value"))
    {
        mixedFvPatchScalarField::operator==
        (
            Field<scalar>("value", iF.dimensions(), dict, p.size())
        );
    }
    else
    {
        mixedFvPatchScalarField::operator==(patchInternalField());
    }
}


Foam::moldingVentPressureFvPatchScalarField::
moldingVentPressureFvPatchScalarField
(
    const moldingVentPressureFvPatchScalarField& ptf,
    const fvPatch& p,
    const DimensionedField<scalar, fvMesh>& iF,
    const fieldMapper& mapper
)
:
    mixedFvPatchScalarField(ptf, p, iF, mapper),
    p0_(ptf.p0_),
    CdA_(ptf.CdA_)
{}


Foam::moldingVentPressureFvPatchScalarField::
moldingVentPressureFvPatchScalarField
(
    const moldingVentPressureFvPatchScalarField& ptf,
    const DimensionedField<scalar, fvMesh>& iF
)
:
    mixedFvPatchScalarField(ptf, iF),
    p0_(ptf.p0_),
    CdA_(ptf.CdA_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::moldingVentPressureFvPatchScalarField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    const moldingStage& stage = db().lookupObject<moldingStage>
    (
        moldingStage::typeName
    );

    if (stage.ventSealed())
    {
        // Melt front at the vent: no through-flow (the matching
        // moldingVentVelocity condition sets the velocity to zero)
        valueFraction() = 0;
        refGrad() = 0;
    }
    else
    {
        // Open vent: ambient pressure plus, for a restricted vent (CdA>0),
        // the orifice back-pressure driven by the current escaping mass
        // flow. The flow is taken from the previous pressure-solver
        // iteration, so the coupling is explicit but converges within the
        // PIMPLE correctors
        valueFraction() = 1;
        refGrad() = 0;

        if (CdA_ > 0)
        {
            const label patchi = patch().index();

            const fvsPatchField<scalar>& phip =
                patch().lookupPatchField<surfaceScalarField, scalar>
                (
                    "phi"
                );

            const volScalarField& rho =
                db().lookupObject<volScalarField>("rho");

            const scalarField& rhop = rho.boundaryField()[patchi];
            const scalarField magSf(patch().magSf());

            const scalar mDot(gSum(rhop*phip));
            const scalar rhoMean
            (
                gSum(rhop*magSf)/max(gSum(magSf), small)
            );

            refValue() = ventOrifice::pVent(p0_, rhoMean, mDot, CdA_);
        }
        else
        {
            refValue() = p0_;
        }
    }

    mixedFvPatchScalarField::updateCoeffs();
}


void Foam::moldingVentPressureFvPatchScalarField::write
(
    Ostream& os
) const
{
    mixedFvPatchScalarField::write(os);
    writeEntry(os, "p0", p0_);
    writeEntry(os, "CdA", CdA_);
    writeEntry(os, "value", *this);
}


// * * * * * * * * * * * * * * * Build Macro Function  * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchScalarField,
        moldingVentPressureFvPatchScalarField
    );
}

// ************************************************************************* //
