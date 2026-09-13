/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | moldingFoam: injection molding solver modules
   \\      /  Website        | https://openfoam.org
    \\  /   A nd           | Copyright (C) 2011-2026 OpenFOAM Foundation
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

#include "moldingTractionDisplacementFvPatchVectorField.H"
#include "addToRunTimeSelectionTable.H"
#include "volFields.H"
#include "physicalProperties.H"
#include "OSspecific.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

moldingTractionDisplacementFvPatchVectorField::
moldingTractionDisplacementFvPatchVectorField
(
    const fvPatch& p,
    const DimensionedField<vector, fvMesh>& iF,
    const dictionary& dict
)
:
    fixedGradientFvPatchVectorField(p, iF),
    traction_("traction", dimPressure, dict, p.size()),
    pressure_
    (
        Function1<scalar>::New
        (
            "pressure",
            time().userUnits(),
            dimPressure,
            dict
        )
    ),
    eigenstrainName_(dict.lookupOrDefault<word>("eigenstrain", "eigenstrain")),
    eigenstrainOwner_(),
    eigenstrainPtr_(nullptr)
{
    fvPatchVectorField::operator=(patchInternalField());
    gradient() = Zero;
}


moldingTractionDisplacementFvPatchVectorField::
moldingTractionDisplacementFvPatchVectorField
(
    const moldingTractionDisplacementFvPatchVectorField& tdpvf,
    const fvPatch& p,
    const DimensionedField<vector, fvMesh>& iF,
    const fieldMapper& mapper
)
:
    fixedGradientFvPatchVectorField(tdpvf, p, iF, mapper),
    traction_(mapper(tdpvf.traction_)),
    pressure_(tdpvf.pressure_, false),
    eigenstrainName_(tdpvf.eigenstrainName_),
    eigenstrainOwner_(),
    eigenstrainPtr_(nullptr)
{}


moldingTractionDisplacementFvPatchVectorField::
moldingTractionDisplacementFvPatchVectorField
(
    const moldingTractionDisplacementFvPatchVectorField& tdpvf,
    const DimensionedField<vector, fvMesh>& iF
)
:
    fixedGradientFvPatchVectorField(tdpvf, iF),
    traction_(tdpvf.traction_),
    pressure_(tdpvf.pressure_, false),
    eigenstrainName_(tdpvf.eigenstrainName_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void moldingTractionDisplacementFvPatchVectorField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    const label patchi = patch().index();

    const solidDisplacementThermo& thermo =
        db().lookupObject<solidDisplacementThermo>
        (
            physicalProperties::typeName
        );

    const scalarField& E = thermo.E(patchi);
    const scalarField& nu = thermo.nu(patchi);

    const scalarField mu(E/(2.0*(1.0 + nu)));
    const scalarField lambda
    (
        thermo.planeStress()
      ? nu*E/((1 + nu)*(1 - nu))
      : nu*E/((1 + nu)*(1 - 2*nu))
    );
    const scalarField threeK
    (
        thermo.planeStress()
      ? E/(1 - nu)
      : E/(1 - 2*nu)
    );

    const scalarField twoMuLambda(2*mu + lambda);

    const vectorField n(patch().nf());

    const fvPatchField<symmTensor>& sigmaD =
        patch().lookupPatchField<volSymmTensorField, symmTensor>("sigmaD");

    gradient() =
    (
        (traction_ - pressure_->value(time().value())*n)
      + twoMuLambda*fvPatchField<vector>::snGrad() - (n & sigmaD)
    )/twoMuLambda;

    if (thermo.thermalStress())
    {
        const scalarField& alphav = thermo.alphav(patchi);

        gradient() +=
            n*threeK*alphav*thermo.T().boundaryField()[patchi]/twoMuLambda;
    }

    // Anisotropic eigenstrain (task 034 follow-up): the free-surface
    // traction carries sigma_th = -threeK*eigenstrain, the tensor
    // generalisation of the isotropic -I*threeK*alphav*T term above
    if (eigenstrainPtr_ == nullptr)
    {
        const fvMesh& mesh = patch().boundaryMesh().mesh();

        if (mesh.foundObject<volSymmTensorField>(eigenstrainName_))
        {
            eigenstrainPtr_ =
                &mesh.lookupObject<volSymmTensorField>(eigenstrainName_);
        }
        else
        {
            // The condition is first evaluated at the first solved time,
            // which for steady runs is beyond the initial directory: look
            // for the field at the current time, then at the start time
            word instance(mesh.time().name());

            const fileName casePath(mesh.time().path());

            if
            (
                !isFile(casePath/instance/eigenstrainName_)
            )
            {
                instance =
                    mesh.time().timeName(mesh.time().startTime().value());
            }

            IOobject io
            (
                eigenstrainName_,
                instance,
                mesh,
                IOobject::READ_IF_PRESENT,
                IOobject::NO_WRITE
            );

            // Read the field from the case (the value-carrying constructor
            // would skip the file read)
            if (isFile(casePath/instance/eigenstrainName_))
            {
                eigenstrainOwner_.reset
                (
                    new volSymmTensorField(io, mesh)
                );
            }
            else
            {
                eigenstrainOwner_.reset
                (
                    new volSymmTensorField
                    (
                        io,
                        mesh,
                        dimensionedSymmTensor
                        (
                            eigenstrainName_,
                            dimless,
                            symmTensor::zero
                        )
                    )
                );
            }

            eigenstrainPtr_ = &(eigenstrainOwner_());
        }
    }

    const volSymmTensorField& eigenstrain = *eigenstrainPtr_;

    gradient() +=
        (n & (threeK*eigenstrain.boundaryField()[patchi]))/twoMuLambda;

    fixedGradientFvPatchVectorField::updateCoeffs();
}


void moldingTractionDisplacementFvPatchVectorField::write(Ostream& os) const
{
    fvPatchVectorField::write(os);
    writeEntry(os, "traction", traction_);
    writeEntry(os, time().userUnits(), dimPressure, pressure_());
    writeEntry(os, "eigenstrain", eigenstrainName_);
    writeEntry(os, "value", *this);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

makePatchTypeField
(
    fvPatchVectorField,
    moldingTractionDisplacementFvPatchVectorField
);

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
