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

#include "viscoelasticStress.H"
#include "fvMatrices.H"
#include "addToRunTimeSelectionTable.H"
#include "fvcGrad.H"
#include "fvcDiv.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

defineTypeNameAndDebug(viscoelasticStress, 0);

addToRunTimeSelectionTable
(
    fvModel,
    viscoelasticStress,
    dictionary
);


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

viscoelasticStress::viscoelasticStress
(
    const word& name,
    const word& modelType,
    const fvMesh& mesh,
    const dictionary& dict
)
:
    fvModel(name, modelType, mesh, dict),
    model_(dict),
    UName_("U"),
    tau_
    (
        IOobject
        (
            "tau",
            mesh.time().name(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedSymmTensor("tau", dimPressure, symmTensor::zero)
    ),
    timeIndex_(-1)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

wordList viscoelasticStress::addSupFields() const
{
    return wordList(1, UName_);
}


bool viscoelasticStress::addsSupToField(const word& fieldName) const
{
    return fieldName == UName_;
}


void viscoelasticStress::addSup
(
    const volScalarField& rho,
    const volVectorField& U,
    fvMatrix<vector>& eqn
) const
{
    // Advance the extra stress once per time step with the local velocity
    // gradient; the guard avoids over-integrating across the PIMPLE
    // correctors
    if (timeIndex_ != mesh().time().timeIndex())
    {
        const scalar dt = mesh().time().deltaTValue();
        const volTensorField gradU(fvc::grad(U));

        symmTensorField& tauc = tau_.primitiveFieldRef();
        const tensorField& gc = gradU.primitiveField();

        forAll(tauc, i)
        {
            tauc[i] = model_.advance(tauc[i], gc[i], dt);
        }

        tau_.correctBoundaryConditions();
        timeIndex_ = mesh().time().timeIndex();
    }

    // The polymer stress adds div(tau) to the momentum equation
    eqn += fvc::div(tau_);
}


bool viscoelasticStress::movePoints()
{
    return true;
}


void viscoelasticStress::topoChange(const polyTopoChangeMap&)
{}


void viscoelasticStress::mapMesh(const polyMeshMap&)
{}


void viscoelasticStress::distribute(const polyDistributionMap&)
{}


bool viscoelasticStress::read(const dictionary& dict)
{
    return true;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fv
} // End namespace Foam

// ************************************************************************* //
