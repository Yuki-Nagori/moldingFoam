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

#include "moldingVoidClosure.H"
#include "VoFSolver.H"
#include "fvmSup.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{
    defineTypeNameAndDebug(moldingVoidClosure, 0);

    addToRunTimeSelectionTable
    (
        fvModel,
        moldingVoidClosure,
        dictionary
    );
}
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::fv::moldingVoidClosure::moldingVoidClosure
(
    const word& sourceName,
    const word& modelType,
    const fvMesh& mesh,
    const dictionary& dict
)
:
    fvModel(sourceName, modelType, mesh, dict),

    mixture_
    (
        mesh.lookupObjectRef<compressibleTwoPhaseVoFMixture>
        (
            "phaseProperties"
        )
    ),

    cavitation_(Foam::compressible::cavitationModel::New(dict, mixture_)),

    rhoRef_(mesh.nCells(), 0),

    band_(dict.lookupOrDefault<scalar>("band", 0.2))
{
    if (band_ <= 0 || band_ > 1)
    {
        FatalIOErrorInFunction(dict)
            << "The evaporation-ceiling ramp width must be in (0, 1]: "
            << "band = " << band_ << exit(FatalIOError);
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::wordList Foam::fv::moldingVoidClosure::addSupFields() const
{
    return
    {
        mixture_.rho1().name(),
        mixture_.rho2().name()
    };
}


Foam::volScalarField::Internal Foam::fv::moldingVoidClosure::scaledLimit
(
    const volScalarField::Internal& limit,
    const volScalarField::Internal& field,
    const word& name
) const
{
    // Element-wise scaling with the dimensions of the unscaled field.
    // (The DimensionedField product of a dimensionless factor by a field
    // squares the dimensions in this OpenFOAM build, so the product is
    // assembled explicitly.)
    Field<scalar> values(mesh().nCells());

    forAll(values, i)
    {
        values[i] = limit[i]*field[i];
    }

    return volScalarField::Internal
    (
        volScalarField::Internal::New
        (
            name,
            mesh(),
            field.dimensions(),
            values
        )
    );
}


Foam::tmp<Foam::volScalarField::Internal>
Foam::fv::moldingVoidClosure::evaporationLimit() const
{
    const volScalarField::Internal& alpha1 = mixture_.alpha1().internalField();
    const volScalarField::Internal& rho = mixture_.rho().internalField();
    const volScalarField::Internal& rho1 = mixture_.rho1().internalField();
    const volScalarField::Internal& p =
        mesh().lookupObject<volScalarField>("p").internalField();
    const volScalarField::Internal pSat(cavitation_->pSat1());

    tmp<volScalarField::Internal> tLimit
    (
        volScalarField::Internal::New
        (
            "cavitationEvaporationLimit",
            mesh(),
            dimless,
            Field<scalar>(mesh().nCells(), 1.0)
        )
    );
    volScalarField::Internal& limit = tLimit.ref();


    forAll(rhoRef_, celli)
    {
        const scalar voidNow = 1 - alpha1[celli];

        // Capture the sealed bulk density as soon as the cell reaches the
        // saturation pressure (the void is still closed there, so the
        // mixture density is the melt density at the pin); also capture at
        // the first sign of a void should the pressure path be late.
        // Re-arm once the void has closed and the cell is repressurised.
        if (rhoRef_[celli] <= 0)
        {
            if (voidNow > 1e-6 || p[celli] <= pSat[celli])
            {
                rhoRef_[celli] = rho[celli];
            }
        }
        else if (voidNow < 1e-9 && p[celli] > pSat[celli])
        {
            rhoRef_[celli] = 0;
        }

        // Isochoric PVT closure ceiling: the void the sealed melt must
        // open at the current pressure and temperature. While the pressure
        // sits below the pin (tension) the ceiling grows with the melt
        // density deficit, which lets the evaporation pull it back up.
        const scalar phiClosure =
            rhoRef_[celli] > 0
          ? max(scalar(0), 1 - rhoRef_[celli]/rho1[celli])
          : scalar(0);

        // Cap the evaporation at the ceiling: full rate below the ramp,
        // zero at (and above) the ceiling so that the void tracks the
        // closure instead of the phase-change rate
        limit[celli] =
            phiClosure > small
          ? min
            (
                scalar(1),
                max(scalar(0), (phiClosure - voidNow)/(band_*phiClosure))
            )
          : scalar(0);
    }

    return tLimit;
}


Foam::tmp<Foam::volScalarField::Internal>
Foam::fv::moldingVoidClosure::closureVoid() const
{
    const volScalarField::Internal& rho1 = mixture_.rho1().internalField();

    tmp<volScalarField::Internal> tPhi
    (
        volScalarField::Internal::New
        (
            "voidClosure",
            mesh(),
            dimless,
            Field<scalar>(mesh().nCells(), 0.0)
        )
    );
    volScalarField::Internal& phi = tPhi.ref();

    forAll(rhoRef_, celli)
    {
        phi[celli] =
            rhoRef_[celli] > 0
          ? max(scalar(0), 1 - rhoRef_[celli]/rho1[celli])
          : scalar(0);
    }

    return tPhi;
}


void Foam::fv::moldingVoidClosure::addSup
(
    const volScalarField& alpha,
    const volScalarField& rho,
    fvMatrix<scalar>& eqn
) const
{
    if (debug)
    {
        Info<< type() << ": applying source to " << eqn.psi().name() << endl;
    }

    if (&rho != &mixture_.rho1() && &rho != &mixture_.rho2())
    {
        FatalErrorInFunction
            << "Support for field " << rho.name() << " is not implemented"
            << exit(FatalError);
    }

    const solvers::VoFSolver& solver =
        mesh().lookupObject<solvers::VoFSolver>(solver::typeName);

    const volScalarField& alpha1 = mixture_.alpha1();
    const volScalarField& alpha2 = mixture_.alpha2();

    const scalar s = &alpha == &alpha1 ? +1 : -1;

    // Per-cell evaporation ceiling factor (also refreshes rhoRef_). The
    // tmp is kept alive: the reference must outlive the full expression
    const tmp<volScalarField::Internal> tLimit(evaporationLimit());
    const volScalarField::Internal& limit = tLimit();

    // Volume-fraction linearisation
    if (&alpha == &eqn.psi())
    {
        const Pair<tmp<volScalarField::Internal>> mDot12Alpha
        (
            cavitation_->mDot12Alpha()
        );
        const volScalarField::Internal& mDot1Alpha2 = mDot12Alpha[0]();
        const volScalarField::Internal& mDot2Alpha1 = mDot12Alpha[1]();

        // Element 1 is the vaporisation (Cv) coefficient: the melt loses
        // volume through its implicit sink, so that is what the closure
        // ceiling caps. Element 0 (condensation/Cc) stays unrestricted so
        // that a void can always close again.
        const volScalarField::Internal mDot2Limited
        (
            scaledLimit(limit, mDot2Alpha1, "mDot2Alpha1Limited")
        );

        eqn +=
            (&alpha == &alpha1 ? mDot1Alpha2 : mDot2Limited)
          - fvm::Sp(mDot1Alpha2 + mDot2Limited, eqn.psi());
    }

    // Pressure linearisation
    else if (&eqn.psi() == &solver.p_rgh)
    {
        const Pair<tmp<volScalarField::Internal>> mDot12P
        (
            cavitation_->mDot12P()
        );
        const volScalarField::Internal& mDot1P = mDot12P[0];
        const volScalarField::Internal& mDot2P = mDot12P[1];

        const volScalarField::Internal& rhoi =
            mixture_.rho().internalField();
        const volScalarField& ghField =
            mesh().lookupObject<volScalarField>("gh");
        const volScalarField::Internal& gh = ghField.internalField();

        const volScalarField::Internal mDot2PLimited
        (
            scaledLimit(limit, mDot2P, "mDot2PLimited")
        );

        eqn +=
            fvm::Sp(s*(mDot1P - mDot2PLimited), eqn.psi())
          + s*(mDot1P - mDot2PLimited)*rhoi*gh
          - s*(mDot1P*cavitation_->pSat1() - mDot2PLimited*cavitation_->pSat2());
    }

    // Explicit non-linearised value. Used in density predictors and
    // continuity error terms.
    else
    {
        const Pair<tmp<volScalarField::Internal>> mDot12Alpha
        (
            cavitation_->mDot12Alpha()
        );
        const volScalarField::Internal mDot1(mDot12Alpha[0]*alpha2);
        const volScalarField::Internal mDot2(mDot12Alpha[1]*alpha1);
        const volScalarField::Internal mDot2Limited
        (
            scaledLimit(limit, mDot2, "mDot2Limited")
        );

        eqn += s*(mDot1 - mDot2Limited);
    }
}


void Foam::fv::moldingVoidClosure::correct()
{
    cavitation_->correct();
}


bool Foam::fv::moldingVoidClosure::read(const dictionary& dict)
{
    dict.readIfPresent("band", band_);

    cavitation_->read(dict);

    return true;
}


bool Foam::fv::moldingVoidClosure::movePoints()
{
    return true;
}


void Foam::fv::moldingVoidClosure::topoChange(const polyTopoChangeMap& map)
{
    // Re-arm the closure reference on a changed mesh (the sealed-state
    // reference is only meaningful for the cells it was captured on)
    rhoRef_.setSize(mesh().nCells(), 0);
}


void Foam::fv::moldingVoidClosure::mapMesh(const polyMeshMap&)
{}


void Foam::fv::moldingVoidClosure::distribute(const polyDistributionMap& map)
{
    rhoRef_.setSize(mesh().nCells(), 0);
}


// ************************************************************************* //
