/*--------------------------------*- C++ -*----------------------------------*\
  =========                 |
  \\      /  F ield         | moldingFoam: injection molding solver modules
   \\      /   O peration   | https://openfoam.org
    \\      /    A nd       | Copyright (C) 2026 Yuki Lu
     \\/     M anipulation  |
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

#include "moldingEigenstrain.H"
#include "fvMatrices.H"
#include "fvcDiv.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{
    defineTypeNameAndDebug(moldingEigenstrain, 0);

    addToRunTimeSelectionTable
    (
        fvModel,
        moldingEigenstrain,
        dictionary
    );
}
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::fv::moldingEigenstrain::moldingEigenstrain
(
    const word& sourceName,
    const word& modelType,
    const fvMesh& mesh,
    const dictionary& dict
)
:
    fvModel(sourceName, modelType, mesh, dict),

    thermo_
    (
        mesh.lookupObjectRef<solidDisplacementThermo>
        (
            physicalProperties::typeName
        )
    ),

    eigenstrainName_(dict.lookupOrDefault<word>("eigenstrain", "eigenstrain")),

    DName_("D")
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::wordList Foam::fv::moldingEigenstrain::addSupFields() const
{
    return {DName_};
}


bool Foam::fv::moldingEigenstrain::addsSupToField(const word& fieldName) const
{
    return fieldName == DName_;
}


void Foam::fv::moldingEigenstrain::addSup
(
    const volVectorField& D,
    fvMatrix<vector>& eqn
) const
{
    const volSymmTensorField& eigenstrain =
        mesh().lookupObject<volSymmTensorField>(eigenstrainName_);

    // threeK exactly as the solver builds it (see solidDisplacement.C)
    const volScalarField& E = thermo_.E();
    const volScalarField& nu = thermo_.nu();

    const volScalarField threeK
    (
        thermo_.planeStress()
      ? E/(1 - nu)
      : E/(1 - 2*nu)
    );

    // The solver assembles rho*fvModels().d2dt2(D) and d2dt2 is an ordinary
    // source assembly, so the source is added divided by rho: the outer
    // multiplication then restores div(threeK*eigenstrain)
    const volScalarField& rho = thermo_.rho();

    // Sign convention, measured on the graded-bar probe (task 040 §9c):
    // the source assembled through the d2dt2 hook reaches the equation with
    // a flipped sign (a +div source produced a -div response of exactly the
    // same magnitude, -0.150 vs the required +0.150), so it is added
    // negated here. The solver's own thermal term uses fvc::grad(...)
    // directly on the field-level +=, which is not flipped.
    eqn -= fvc::div(threeK*eigenstrain)/rho;
}


bool Foam::fv::moldingEigenstrain::read(const dictionary& dict)
{
    dict.readIfPresent("eigenstrain", eigenstrainName_);

    return true;
}


bool Foam::fv::moldingEigenstrain::movePoints()
{
    return true;
}


void Foam::fv::moldingEigenstrain::topoChange(const polyTopoChangeMap&)
{}


void Foam::fv::moldingEigenstrain::mapMesh(const polyMeshMap&)
{}


void Foam::fv::moldingEigenstrain::distribute(const polyDistributionMap&)
{}


// ************************************************************************* //
