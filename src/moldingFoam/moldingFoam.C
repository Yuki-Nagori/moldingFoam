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

#include "moldingFoam.H"
#include "moldingStage.H"
#include "moldingCrystallization.H"
#include "moldingFiberOrientation.H"
#include "moldingShrinkage.H"
#include "IOobject.H"
#include "moldingPrghPressureFvPatchScalarField.H"
#include "moldingVentVelocityFvPatchVectorField.H"
#include "addToRunTimeSelectionTable.H"
#include "Function1.H"
#include "IFstream.H"
#include "OSspecific.H"
#include "fvcVolumeIntegrate.H"
#include "fvcMeshPhi.H"
#include "fvcDdt.H"
#include "fvcDiv.H"
#include "fvmDiv.H"
#include "fvmSup.H"
#include "fvmLaplacian.H"
#include "fixedValueFvPatchFields.H"
#include "syncTools.H"
#include "processorFvPatch.H"
#include "Pstream.H"
#include "boolList.H"

// * * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace solvers
{
    defineTypeNameAndDebug(moldingFoam, 0);

    addToRunTimeSelectionTable(solver, moldingFoam, fvMesh);
}
}


// * * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

namespace
{

//- Path of a constant dictionary that is region aware: in a multi-region
//  case the mesh registry's dbDir carries the region name, so the
//  dictionary is read from constant/<region>/<name>; for a single-region
//  case the dbDir is empty and the path is constant/<name>
Foam::fileName constantDictPath
(
    const Foam::objectRegistry& obr,
    const Foam::word& name
)
{
    return Foam::IOobject
    (
        name,
        obr.time().constant(),
        obr,
        Foam::IOobject::NO_READ,
        Foam::IOobject::NO_WRITE,
        false
    ).filePath(false);
}

} // End anonymous namespace


Foam::solvers::moldingFoam::moldingFoam(fvMesh& mesh)
:
    compressibleVoF(mesh),
    ejected_(false),
    ejectionTemperature_(great),
    releasePressure_(great),
    switchPressure_(great),
    gateSealTime_(great),
    gateSealRamp_(0),
    gateFreezeTemperature_(-great),
    ventSealAlpha_(0.5),
    trapAirInterval_(0),
    trapAirAlpha_(0.5),
    viscousDissipation_(false),
    dissipationCoeffs_(),
    crystallization_(),
    chi_(),
    chiInitial_(),
    fiberOrientation_(),
    a_(),
    aInitial_(),
    shrinkage_(),
    shrinkageField_(),
    shrinkageInitial_(),
    writeFillTime_(false),
    fillTime_(),
    fillTimeInitial_(),
    airTrap_(),
    nCycles_(1),
    cycle_(1),
    deltaTInitial_(runTime.deltaTValue()),
    alpha1Initial_(),
    UInitial_(),
    TInitial_(),
    pInitial_(),
    p_rghInitial_(),
    vTot_(gSum(mesh.V().primitiveField())),
    moldingDictModTime_(0)
{
    // The molding dictionary is the external case-generation contract. The
    // packing group drives the V/P switch and the packing pressure curve
    // (stage M2); the cooling group drives the ejection criterion (stage
    // M3).
    const fileName moldingDictPath(constantDictPath(mesh, "moldingDict"));

    if (isFile(moldingDictPath))
    {
        IFstream is(moldingDictPath);

        if (!is.good())
        {
            FatalIOErrorInFunction(moldingDictPath)
                << "Cannot open " << moldingDictPath
                << exit(FatalIOError);
        }

        dictionary moldingDict(is);

        const dictionary& packingDict = moldingDict.subDict("packing");
        const dictionary& coolingDict = moldingDict.subDict("cooling");

        const scalar switchFraction =
            packingDict.lookup<scalar>("switchFraction");

        autoPtr<Function1<scalar>> pressure
        (
            Function1<scalar>::New
            (
                "pressure",
                dimTime,
                dimPressure,
                packingDict
            )
        );

        const scalar ejectionTemperature =
            coolingDict.lookup<scalar>("ejectionTemperature");

        // Pressure target below which the packing pressure counts as
        // released; defaults to atmospheric to preserve the behaviour of
        // contract cases that do not specify it
        const scalar releasePressure =
            coolingDict.lookupOrDefault<scalar>("releasePressure", 1e5);

        // Optional gate pressure at which the V/P switch is triggered
        // even before the filled fraction reaches switchFraction; this
        // avoids over-compressing the cavity after the vent seals
        const scalar switchPressure =
            packingDict.lookupOrDefault<scalar>("switchPressure", great);

        // Time after the V/P switch at which the gate freezes off; by
        // default the gate seals when the packing pressure is released
        const scalar gateSealTime =
            packingDict.lookupOrDefault<scalar>("gateSealTime", great);

        // Optional gate seal ramp: the gate flow ramps to zero over this
        // duration instead of sealing instantaneously (default 0 keeps
        // the historical behaviour)
        const scalar gateSealRamp =
            packingDict.lookupOrDefault<scalar>("gateSealRamp", 0);

        // Optional gate melt temperature at or below which the gate
        // freezes off; disabled by default (time / pressure release only)
        const scalar gateFreezeTemperature =
            moldingDict.lookupOrDefault<scalar>
            (
                "gateFreezeTemperature",
                -great
            );

        // Melt volume fraction on the vent patch above which the vent is
        // sealed (polymer must not escape through the vent)
        const scalar ventSealAlpha =
            moldingDict.lookupOrDefault<scalar>("ventSealAlpha", 0.5);

        // Optional trapped-air diagnostic (see reportTrappedAir)
        const label trapAirInterval =
            moldingDict.lookupOrDefault<label>("trapAirInterval", 0);

        const scalar trapAirAlpha =
            moldingDict.lookupOrDefault<scalar>("trapAirAlpha", 0.5);

        // Optional viscous-dissipation (shear heating) source in the
        // energy equation; default false to preserve existing cases
        const bool viscousDissipation =
            moldingDict.lookupOrDefault<Switch>("viscousDissipation", false);

        // Optional number of moulding cycles; each ejection criterion met
        // resets the flow fields and starts the next cycle, keeping the
        // mould thermal state
        const label nCycles = moldingDict.lookupOrDefault<label>("nCycles", 1);

        if (nCycles < 1)
        {
            FatalIOErrorInFunction(moldingDict)
                << "The number of moulding cycles must be at least 1: "
                << "nCycles = " << nCycles << exit(FatalIOError);
        }

        ejectionTemperature_ = ejectionTemperature;
        releasePressure_ = releasePressure;
        switchPressure_ = switchPressure;
        gateSealTime_ = gateSealTime;
        gateSealRamp_ = gateSealRamp;
        gateFreezeTemperature_ = gateFreezeTemperature;
        ventSealAlpha_ = ventSealAlpha;
        trapAirInterval_ = trapAirInterval;
        trapAirAlpha_ = trapAirAlpha;
        viscousDissipation_ = viscousDissipation;
        nCycles_ = nCycles;

        if (viscousDissipation_)
        {
            readDissipationCoeffs();
        }

        // Optional crystallisation kinetics: a relative crystallinity
        // field evolved with the Nakamura/Avrami model whose latent heat
        // replaces the fixed hMelt latent-Cp platform
        if (moldingDict.found("crystallization"))
        {
            crystallization_.reset
            (
                new moldingCrystallization
                (
                    moldingDict.subDict("crystallization")
                )
            );

            chi_.reset
            (
                new volScalarField
                (
                    IOobject
                    (
                        "chi",
                        runTime.name(),
                        mesh,
                        IOobject::READ_IF_PRESENT,
                        IOobject::AUTO_WRITE
                    ),
                    mesh,
                    dimensionedScalar("chi", dimless, 0)
                )
            );

            chiInitial_.reset(new volScalarField(*chi_));
        }

        // Optional fibre orientation: the Folgar-Tucker equation evolves
        // the second-order orientation tensor a = <p p>
        if (moldingDict.found("fiberOrientation"))
        {
            fiberOrientation_.reset
            (
                new moldingFiberOrientation
                (
                    moldingDict.subDict("fiberOrientation")
                )
            );

            a_.reset
            (
                new volSymmTensorField
                (
                    IOobject
                    (
                        "a",
                        runTime.name(),
                        mesh,
                        IOobject::READ_IF_PRESENT,
                        IOobject::AUTO_WRITE
                    ),
                    mesh,
                    dimensionedSymmTensor("a", dimless, symmTensor::I/3)
                )
            );

            aInitial_.reset(new volSymmTensorField(*a_));
        }

        // Optional shrinkage and residual-stress indicators from the
        // local melt density and temperature
        if (moldingDict.found("shrinkage"))
        {
            shrinkage_.reset
            (
                new moldingShrinkage(moldingDict.subDict("shrinkage"))
            );

            shrinkageField_.reset
            (
                new volScalarField
                (
                    IOobject
                    (
                        "shrinkage",
                        runTime.name(),
                        mesh,
                        IOobject::READ_IF_PRESENT,
                        IOobject::AUTO_WRITE
                    ),
                    mesh,
                    dimensionedScalar("shrinkage", dimless, 0)
                )
            );

            shrinkageInitial_.reset(new volScalarField(*shrinkageField_));
        }

        // Optional fill-time field: the time each cell becomes filled
        // (alpha >= 0.5); its local maxima are the weld lines
        writeFillTime_ = moldingDict.lookupOrDefault<Switch>
        (
            "writeFillTime",
            false
        );

        if (writeFillTime_)
        {
            fillTime_.reset
            (
                new volScalarField
                (
                    IOobject
                    (
                        "fillTime",
                        runTime.name(),
                        mesh,
                        IOobject::READ_IF_PRESENT,
                        IOobject::AUTO_WRITE
                    ),
                    mesh,
                    dimensionedScalar("fillTime", dimTime, VGREAT)
                )
            );

            fillTimeInitial_.reset(new volScalarField(*fillTime_));
        }

        // Optional trapped-air field, written with the trapped-air
        // diagnostic
        if (trapAirInterval_ > 0)
        {
            airTrap_.reset
            (
                new volScalarField
                (
                    IOobject
                    (
                        "airTrap",
                        runTime.name(),
                        mesh,
                        IOobject::NO_READ,
                        IOobject::AUTO_WRITE
                    ),
                    mesh,
                    dimensionedScalar("airTrap", dimless, 0)
                )
            );
        }

        // The stage object registers itself on the mesh and is shared with
        // the molding boundary conditions
        if (!mesh.foundObject<moldingStage>(moldingStage::typeName))
        {
            new moldingStage
            (
                mesh,
                runTime,
                switchFraction,
                std::move(pressure),
                gateSealRamp
            );
        }

        Info<< "moldingFoam: read " << moldingDictPath << nl
            << "    packing:" << nl
            << "        switchFraction      = " << switchFraction << nl
            << "        switchPressure      = " << switchPressure << nl
            << "        gateSealTime        = " << gateSealTime << nl
            << "        gateSealRamp        = " << gateSealRamp << nl
            << "        gateFreezeTemperature = " << gateFreezeTemperature
            << nl
            << "        pressure type       = "
            << packingDict.subDict("pressure").lookup<word>("type") << nl
            << "    cooling:" << nl
            << "        ejectionTemperature = " << ejectionTemperature << nl
            << "        releasePressure     = " << releasePressure << nl
            << "    ventSealAlpha           = " << ventSealAlpha << nl
            << "    trapAirInterval         = " << trapAirInterval << nl
            << "    viscousDissipation      = " << viscousDissipation
            << endl;
    }

    // Snapshots of the initial fields for the multi-cycle reset
    alpha1Initial_.reset
    (
        new volScalarField
        (
            IOobject
            (
                "alpha1Initial",
                Time::timeName(runTime.value()),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE,
                false
            ),
            alpha1
        )
    );
    UInitial_.reset
    (
        new volVectorField
        (
            IOobject
            (
                "UInitial",
                Time::timeName(runTime.value()),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE,
                false
            ),
            U
        )
    );
    TInitial_.reset
    (
        new volScalarField
        (
            IOobject
            (
                "TInitial",
                Time::timeName(runTime.value()),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE,
                false
            ),
            mixture_.T()
        )
    );
    pInitial_.reset
    (
        new volScalarField
        (
            IOobject
            (
                "pInitial",
                Time::timeName(runTime.value()),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE,
                false
            ),
            p
        )
    );
    p_rghInitial_.reset
    (
        new volScalarField
        (
            IOobject
            (
                "p_rghInitial",
                Time::timeName(runTime.value()),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE,
                false
            ),
            p_rgh
        )
    );

    // Baseline for the runtime reload: the constructor has just applied
    // the current dictionary contents
    moldingDictModTime_ = lastModified(moldingDictPath);
}


// * * * * * * * * * * * * * * * * * Destructors  * * * * * * * * * * * * * //

Foam::solvers::moldingFoam::~moldingFoam()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::solvers::moldingFoam::read()
{
    if (compressibleVoF::read())
    {
        readMoldingDict();
        return true;
    }
    else
    {
        return false;
    }
}


void Foam::solvers::moldingFoam::readMoldingDict()
{
    const fileName moldingDictPath(constantDictPath(mesh, "moldingDict"));

    if (!isFile(moldingDictPath))
    {
        return;
    }

    const time_t modTime(lastModified(moldingDictPath));

    if (modTime == moldingDictModTime_)
    {
        return;
    }

    IFstream is(moldingDictPath);

    if (!is.good())
    {
        FatalIOErrorInFunction(moldingDictPath)
            << "Cannot open " << moldingDictPath
            << exit(FatalIOError);
    }

    dictionary moldingDict(is);

    const dictionary& coolingDict(moldingDict.subDict("cooling"));

    const scalar newEjectionTemperature
    (
        coolingDict.lookup<scalar>("ejectionTemperature")
    );

    const scalar newReleasePressure
    (
        coolingDict.lookupOrDefault<scalar>("releasePressure", 1e5)
    );

    const scalar newSwitchPressure
    (
        moldingDict.subDict("packing").lookupOrDefault<scalar>
        (
            "switchPressure",
            great
        )
    );

    const scalar newGateSealTime
    (
        moldingDict.subDict("packing").lookupOrDefault<scalar>
        (
            "gateSealTime",
            great
        )
    );

    const scalar newGateSealRamp
    (
        moldingDict.subDict("packing").lookupOrDefault<scalar>
        (
            "gateSealRamp",
            0
        )
    );

    const scalar newGateFreezeTemperature
    (
        moldingDict.lookupOrDefault<scalar>("gateFreezeTemperature", -great)
    );

    const scalar newVentSealAlpha
    (
        moldingDict.lookupOrDefault<scalar>("ventSealAlpha", 0.5)
    );

    const label newTrapAirInterval
    (
        moldingDict.lookupOrDefault<label>("trapAirInterval", 0)
    );

    const scalar newTrapAirAlpha
    (
        moldingDict.lookupOrDefault<scalar>("trapAirAlpha", 0.5)
    );

    const bool newViscousDissipation
    (
        moldingDict.lookupOrDefault<Switch>("viscousDissipation", false)
    );

    bool controlsChanged
    (
        newEjectionTemperature != ejectionTemperature_
     || newReleasePressure != releasePressure_
     || newSwitchPressure != switchPressure_
     || newGateSealTime != gateSealTime_
     || newGateSealRamp != gateSealRamp_
     || newGateFreezeTemperature != gateFreezeTemperature_
     || newVentSealAlpha != ventSealAlpha_
     || newTrapAirInterval != trapAirInterval_
     || newTrapAirAlpha != trapAirAlpha_
     || newViscousDissipation != viscousDissipation_
    );

    if (controlsChanged)
    {
        Info<< "moldingFoam: process parameters updated:"
            << " ejectionTemperature " << ejectionTemperature_
            << " -> " << newEjectionTemperature
            << ", releasePressure " << releasePressure_
            << " -> " << newReleasePressure
            << ", switchPressure " << switchPressure_
            << " -> " << newSwitchPressure
            << ", gateSealTime " << gateSealTime_
            << " -> " << newGateSealTime
            << ", gateSealRamp " << gateSealRamp_
            << " -> " << newGateSealRamp
            << ", gateFreezeTemperature " << gateFreezeTemperature_
            << " -> " << newGateFreezeTemperature
            << ", ventSealAlpha " << ventSealAlpha_
            << " -> " << newVentSealAlpha
            << ", trapAirInterval " << trapAirInterval_
            << " -> " << newTrapAirInterval
            << ", trapAirAlpha " << trapAirAlpha_
            << " -> " << newTrapAirAlpha
            << ", viscousDissipation " << viscousDissipation_
            << " -> " << newViscousDissipation << endl;

        ejectionTemperature_ = newEjectionTemperature;
        releasePressure_ = newReleasePressure;
        switchPressure_ = newSwitchPressure;
        gateSealTime_ = newGateSealTime;
        gateSealRamp_ = newGateSealRamp;
        gateFreezeTemperature_ = newGateFreezeTemperature;
        ventSealAlpha_ = newVentSealAlpha;
        trapAirInterval_ = newTrapAirInterval;
        trapAirAlpha_ = newTrapAirAlpha;

        if (newViscousDissipation && !viscousDissipation_)
        {
            readDissipationCoeffs();
        }
        viscousDissipation_ = newViscousDissipation;
    }

    if (mesh.foundObject<moldingStage>(moldingStage::typeName))
    {
        mesh.lookupObjectRef<moldingStage>(moldingStage::typeName)
            .read(moldingDict);
    }

    moldingDictModTime_ = lastModified(moldingDictPath);
}


Foam::scalar Foam::solvers::moldingFoam::gatePressure() const
{
    scalar numer(0);
    scalar area(0);

    forAll(p_rgh.boundaryField(), pi)
    {
        if
        (
            p_rgh.boundaryField()[pi].type()
         == moldingPrghPressureFvPatchScalarField::typeName
        )
        {
            const scalarField& prghp = p_rgh.boundaryField()[pi];
            const tmp<vectorField> tSf(mesh.boundary()[pi].Sf());
            const scalarField magSf(mag(tSf()));

            numer += gSum(prghp*magSf);
            area += gSum(magSf);
        }
    }

    return area > small ? numer/area : 0;
}


Foam::scalar Foam::solvers::moldingFoam::gateTemperature() const
{
    scalar mSum(0);
    scalar mTSum(0);

    const volScalarField& T(mixture_.T());

    forAll(p_rgh.boundaryField(), pi)
    {
        if
        (
            p_rgh.boundaryField()[pi].type()
         == moldingPrghPressureFvPatchScalarField::typeName
        )
        {
            const fvPatch& fvp = mesh.boundary()[pi];

            const scalarField Ti
            (
                T.boundaryField()[pi].patchInternalField()
            );
            const scalarField ai
            (
                alpha1.boundaryField()[pi].patchInternalField()
            );
            const scalarField rhoi
            (
                mixture_.rho1().boundaryField()[pi].patchInternalField()
            );
            const scalarField magSf(fvp.magSf());

            forAll(Ti, i)
            {
                const scalar m(ai[i]*rhoi[i]*magSf[i]);

                mSum += m;
                mTSum += m*Ti[i];
            }
        }
    }

    reduce(mSum, sumOp<scalar>());
    reduce(mTSum, sumOp<scalar>());

    return mSum > small ? mTSum/mSum : 0;
}


void Foam::solvers::moldingFoam::readDissipationCoeffs()
{
    const fileName path(constantDictPath(mesh, "momentumTransport"));

    IFstream is(path);

    if (!is.good())
    {
        FatalIOErrorInFunction(path)
            << "Cannot open " << path << " for the viscous-dissipation "
            << "source" << exit(FatalIOError);
    }

    dictionary dict(is);

    const word simulationType(dict.lookup("simulationType"));

    if (simulationType != "laminar")
    {
        FatalErrorInFunction
            << "The viscous-dissipation source requires simulationType "
            << "laminar, but " << simulationType << " is selected"
            << exit(FatalError);
    }

    const dictionary& laminarDict(dict.subDict("laminar"));

    const word model(laminarDict.lookup<word>("model"));

    if (model != "generalisedNewtonian")
    {
        FatalErrorInFunction
            << "The viscous-dissipation source requires model "
            << "generalisedNewtonian, but " << model << " is selected"
            << exit(FatalError);
    }

    const word viscosityModel(laminarDict.lookup<word>("viscosityModel"));

    if (viscosityModel != "CrossWlf")
    {
        FatalErrorInFunction
            << "The viscous-dissipation source requires viscosityModel "
            << "CrossWlf, but " << viscosityModel << " is selected"
            << exit(FatalError);
    }

    dissipationCoeffs_ =
        laminarModels::generalisedNewtonianViscosityModels::CrossWlf::
        readCoeffs(laminarDict.subDict("CrossWlfCoeffs"));

    Info<< "moldingFoam: viscous dissipation enabled, using the CrossWlf "
        << "coefficients from " << path << endl;
}


Foam::tmp<Foam::volScalarField>
Foam::solvers::moldingFoam::viscousDissipationSource() const
{
    // Viscous dissipation Phi = tau : grad(U), with the same symmetric
    // stress as the momentum equation,
    //   tau = 2*eta*dev(symm(grad(U)))
    // and the CrossWlf eta evaluated at the same strain rate as the
    // generalisedNewtonian momentum model: sqrt(2)*mag(symm(grad(U)))
    const volTensorField gradU(fvc::grad(U));
    const volSymmTensorField S(symm(gradU));
    const volScalarField gammaDot(sqrt(2.0)*mag(S));

    tmp<volScalarField> teta
    (
        volScalarField::New
        (
            "etaDissipation",
            mesh,
            dimensionedScalar(dimDynamicViscosity, 0)
        )
    );
    volScalarField& eta = teta.ref();

    const volScalarField& T(mixture_.T());
    const volScalarField& p(mixture_.p());

    {
        scalarField& etac = eta.primitiveFieldRef();
        const scalarField& Tc = T.primitiveField();
        const scalarField& pc = p.primitiveField();
        const scalarField& gc = gammaDot.primitiveField();

        forAll(etac, i)
        {
            etac[i] =
                laminarModels::generalisedNewtonianViscosityModels::CrossWlf::
                eta(dissipationCoeffs_, pc[i], Tc[i], gc[i]);
        }
    }

    volScalarField::Boundary& etaBf = eta.boundaryFieldRef();
    forAll(etaBf, patchi)
    {
        scalarField& etap = etaBf[patchi];
        const scalarField& Tp = T.boundaryField()[patchi];
        const scalarField& pp = p.boundaryField()[patchi];
        const scalarField& gp = gammaDot.boundaryField()[patchi];

        forAll(etap, i)
        {
            etap[i] =
                laminarModels::generalisedNewtonianViscosityModels::CrossWlf::
                eta(dissipationCoeffs_, pp[i], Tp[i], gp[i]);
        }
    }

    // tau : grad(U) = 2*eta*dev(S) : grad(U); dev(S) and grad(U) share
    // the same double-dot form as the turbulence production term
    return teta*(2.0*(dev(S) && gradU));
}


void Foam::solvers::moldingFoam::resetCycle()
{
    ++cycle_;

    // Restore the flow fields to their initial state. The mould thermal
    // state (moldingMoldTemperature) is intentionally not touched, so the
    // next cycle starts from the accumulated mould temperature
    forAll(alpha1.boundaryFieldRef(), patchi)
    {
        alpha1.boundaryFieldRef()[patchi] =
            alpha1Initial_->boundaryField()[patchi];
        U_.boundaryFieldRef()[patchi] =
            UInitial_->boundaryField()[patchi];
        mixture_.T().boundaryFieldRef()[patchi] =
            TInitial_->boundaryField()[patchi];
        p.boundaryFieldRef()[patchi] =
            pInitial_->boundaryField()[patchi];
        p_rgh_.boundaryFieldRef()[patchi] =
            p_rghInitial_->boundaryField()[patchi];
    }

    alpha1.primitiveFieldRef() = alpha1Initial_->primitiveField();
    U_.primitiveFieldRef() = UInitial_->primitiveField();
    mixture_.T().primitiveFieldRef() = TInitial_->primitiveField();

    if (chi_.valid())
    {
        chi_->primitiveFieldRef() = chiInitial_->primitiveField();
        chi_->correctBoundaryConditions();
    }

    if (a_.valid())
    {
        a_->primitiveFieldRef() = aInitial_->primitiveField();
        a_->correctBoundaryConditions();
    }

    if (shrinkageField_.valid())
    {
        shrinkageField_->primitiveFieldRef() =
            shrinkageInitial_->primitiveField();
        shrinkageField_->correctBoundaryConditions();
    }

    if (fillTime_.valid())
    {
        fillTime_->primitiveFieldRef() = fillTimeInitial_->primitiveField();
        fillTime_->correctBoundaryConditions();
    }
    p.primitiveFieldRef() = pInitial_->primitiveField();
    p_rgh_.primitiveFieldRef() = p_rghInitial_->primitiveField();

    // Clear the surface fluxes: they belong to the finished cycle and a
    // stale flux would corrupt the Courant-limited time step and the
    // pressure correction of the first step of the new cycle
    {
        const dimensionedScalar zeroPhi("zero", phi_.dimensions(), 0);
        const dimensionedScalar zeroRhoPhi
        (
            "zero",
            alphaRhoPhi1.dimensions(),
            0
        );
        phi_ == zeroPhi;
        alphaPhi1 == zeroPhi;
        alphaPhi2 == zeroPhi;
        alphaRhoPhi1 == zeroRhoPhi;
        alphaRhoPhi2 == zeroRhoPhi;
        rhoPhi == zeroRhoPhi;
        K == dimensionedScalar("zero", K.dimensions(), 0);
    }

    // Restart the time-step ramp from the case's initial value. The Time
    // object is held const by the solver base, but the underlying object
    // in the registry is mutable
    const_cast<Time&>(runTime).setDeltaT(deltaTInitial_);

    // Clear the seals and return to the filling stage
    if (mesh.foundObject<moldingStage>(moldingStage::typeName))
    {
        mesh.lookupObjectRef<moldingStage>(moldingStage::typeName).resetCycle();
    }

    mixture_.correctThermo();
    mixture_.correct();

    Info<< "moldingFoam: cycle " << (cycle_ - 1) << " complete; starting"
        << " cycle " << cycle_ << "/" << nCycles_ << " at t = "
        << runTime.value() << " s (mould temperature kept)" << endl;
}



void Foam::solvers::moldingFoam::reportTrappedAir()
{
    const label nCells = mesh.nCells();
    const scalarField& alpha(alpha1.primitiveField());
    const scalarField& pc(mixture_.p().primitiveField());
    const scalarField& Tc(mixture_.T().primitiveField());
    const scalarField& Vc(mesh.V().primitiveField());
    const scalarField& rho2c(mixture_.thermo2().rho().primitiveField());

    boolList air(nCells, false);
    label nAir = 0;
    forAll(alpha, i)
    {
        air[i] = alpha[i] <= trapAirAlpha_;
        if (air[i])
        {
            ++nAir;
        }
    }

    if (airTrap_.valid())
    {
        airTrap_->primitiveFieldRef() = 0;
    }

    if (nAir == 0)
    {
        if (Pstream::master())
        {
            Info<< "moldingFoam: trapped air: none (no air cells)" << endl;
        }
        return;
    }

    // Seed from the open vent faces. A sealed vent cannot vent anything,
    // so every air cell counts as trapped then
    const bool ventSealed =
        mesh.foundObject<moldingStage>(moldingStage::typeName)
     && mesh.lookupObject<moldingStage>(moldingStage::typeName).ventSealed();

    boolList connected(nCells, false);
    DynamicList<label> stack;

    if (!ventSealed)
    {
        const volVectorField::Boundary& UBf = U_.boundaryField();

        forAll(UBf, patchi)
        {
            if
            (
                UBf[patchi].type()
             == moldingVentVelocityFvPatchVectorField::typeName
            )
            {
                const labelUList& faceCells =
                    mesh.boundary()[patchi].faceCells();

                forAll(faceCells, i)
                {
                    const label c = faceCells[i];

                    if (air[c] && !connected[c])
                    {
                        connected[c] = true;
                        stack.append(c);
                    }
                }
            }
        }
    }

    // Flood fill locally, then exchange the connected flags across the
    // processor patches and repeat until nothing new is reached
    const labelListList& cc = mesh.cellCells();

    while (true)
    {
        while (stack.size())
        {
            const label c = stack.remove();

            forAll(cc[c], j)
            {
                const label n = cc[c][j];

                if (air[n] && !connected[n])
                {
                    connected[n] = true;
                    stack.append(n);
                }
            }
        }

        scalarField connS(nCells, 0);
        forAll(connected, i)
        {
            connS[i] = connected[i] ? 1 : 0;
        }

        List<scalar> nbConn;
        syncTools::swapBoundaryCellList(mesh, connS, nbConn);

        bool changed = false;

        forAll(mesh.boundary(), patchi)
        {
            const fvPatch& fvp = mesh.boundary()[patchi];

            if (isA<processorFvPatch>(fvp))
            {
                const labelUList& faceCells = fvp.faceCells();

                forAll(faceCells, i)
                {
                    const label bFacei =
                        fvp.start() + i - mesh.nInternalFaces();
                    const label c = faceCells[i];

                    if (nbConn[bFacei] > 0.5 && air[c] && !connected[c])
                    {
                        connected[c] = true;
                        stack.append(c);
                        changed = true;
                    }
                }
            }
        }

        if (!returnReduce(changed, orOp<bool>()))
        {
            break;
        }
    }

    // Collect the trapped cells
    scalar vol = 0;
    scalar mass = 0;
    scalar mP = 0;
    scalar mT = 0;
    scalar Tmax = -great;
    label nTrap = 0;
    vector centroid(Zero);

    const vectorField& Cc = mesh.C().primitiveField();

    scalarField* airTrapc =
        airTrap_.valid() ? &airTrap_->primitiveFieldRef() : nullptr;

    forAll(air, i)
    {
        if (air[i] && !connected[i])
        {
            const scalar m = (1 - alpha[i])*rho2c[i]*Vc[i];

            if (airTrapc)
            {
                (*airTrapc)[i] = 1;
            }

            ++nTrap;
            vol += Vc[i];
            mass += m;
            mP += m*pc[i];
            mT += m*Tc[i];
            Tmax = max(Tmax, Tc[i]);
            centroid += Vc[i]*Cc[i];
        }
    }

    reduce(vol, sumOp<scalar>());
    reduce(mass, sumOp<scalar>());
    reduce(mP, sumOp<scalar>());
    reduce(mT, sumOp<scalar>());
    reduce(Tmax, maxOp<scalar>());
    reduce(nTrap, sumOp<label>());
    reduce(centroid, sumOp<vector>());

    if (Pstream::master())
    {
        Info<< "moldingFoam: trapped air: cells = " << nTrap
            << ", volume = " << vol << " m^3"
            << ", mass = " << mass << " kg";

        if (mass > small)
        {
            Info<< ", <p> = " << mP/mass << " Pa, <T> = " << mT/mass
                << " K";
        }

        if (nTrap > 0)
        {
            Info<< ", max(T) = " << Tmax
                << ", centroid = " << centroid/max(vol, small) << " m";
        }

        Info<< endl;
    }
}

void Foam::solvers::moldingFoam::thermophysicalPredictor()
{
    // As compressibleVoF::thermophysicalPredictor with one addition:
    // after the linear solve the temperature is clamped to a physical
    // range. The hMelt apparent Cv carries the latent-heat peak on the
    // matrix diagonal, and the Picard linearisation of the steep plateau
    // can overshoot; correctThermo's Newton needs a positive starting
    // temperature.

    // Explicit fibre-orientation update: the Folgar-Tucker equation is
    // advanced per cell with the local velocity gradient (RK2 + trace
    // normalisation). Advection of the orientation tensor is a planned
    // extension; the current model is local
    if (fiberOrientation_.valid())
    {
        const scalar dt(runTime.deltaTValue());
        const volTensorField gradU(fvc::grad(U));

        volSymmTensorField& a = *a_;
        symmTensorField& ac = a.primitiveFieldRef();
        const tensorField& gc = gradU.primitiveField();

        scalar maxA12 = 0;
        scalar maxTrErr = 0;

        forAll(ac, i)
        {
            ac[i] = fiberOrientation_->advance(ac[i], gc[i], dt);

            maxA12 = max(maxA12, mag(ac[i].xy()));
            maxTrErr = max(maxTrErr, mag(tr(ac[i]) - 1));
        }

        a.correctBoundaryConditions();

        if (runTime.timeIndex() % 50 == 0)
        {
            Info<< "moldingFoam: fiber orientation: max|a12| = " << maxA12
                << ", max|tr(a)-1| = " << maxTrErr << endl;
        }
    }

    const volScalarField& rho1(mixture_.rho1());
    const volScalarField& rho2(mixture_.rho2());
    const volScalarField& e1(mixture_.thermo1().he());
    const volScalarField& e2(mixture_.thermo2().he());

    const fvScalarMatrix e1Source(fvModels().source(alpha1, rho1, e1));
    const fvScalarMatrix e2Source(fvModels().source(alpha2, rho2, e2));

    volScalarField& T = mixture_.T();

    fvScalarMatrix TEqn
    (
        correction
        (
            mixture_.thermo1().Cv()()
           *(
                fvm::ddt(alpha1, rho1, T) + fvm::div(alphaRhoPhi1, T)
              - (
                    e1Source.hasDiag()
                  ? fvm::Sp(contErr1(), T) + fvm::Sp(e1Source.A(), T)
                  : fvm::Sp(contErr1(), T)
                )
            )
          + mixture_.thermo2().Cv()()
           *(
                fvm::ddt(alpha2, rho2, T) + fvm::div(alphaRhoPhi2, T)
              - (
                    e2Source.hasDiag()
                  ? fvm::Sp(contErr2(), T) + fvm::Sp(e2Source.A(), T)
                  : fvm::Sp(contErr2(), T)
                )
            )
        )

      + fvc::ddt(alpha1, rho1, e1) + fvc::div(alphaRhoPhi1, e1)
      - contErr1()*e1
      + fvc::ddt(alpha2, rho2, e2) + fvc::div(alphaRhoPhi2, e2)
      - contErr2()*e2

      - fvm::laplacian(thermophysicalTransport.kappaEff(), T)

      + (
            mixture_.totalInternalEnergy()
          ?
            fvc::div(fvc::absolute(phi, U), p)()()
          + (fvc::ddt(rho, K) + fvc::div(rhoPhi, K))()()
          - (U()&(fvModels().source(rho, U)&U)()) - (contErr1() + contErr2())*K
          :
            p*fvc::div(fvc::absolute(phi, U))()()
        )
     ==
        (e1Source&e1)
      + (e2Source&e2)
    );

    // Explicit crystallisation latent-heat source. The relative
    // crystallinity is advanced with the exact Avrami step over dt and
    // the released latent heat rho L dchi/dt heats the melt; the source
    // is added to the integrated source directly
    if (crystallization_.valid())
    {
        const scalar dt(runTime.deltaTValue());
        volScalarField& chi = *chi_;
        const volScalarField& T = mixture_.T();
        const volScalarField& p = mixture_.p();
        const volScalarField& rhoMelt = mixture_.rho1();

        scalarField& chic = chi.primitiveFieldRef();
        const scalarField& Tc = T.primitiveField();
        const scalarField& pc = p.primitiveField();
        const scalarField& ac = alpha1.primitiveField();
        const scalarField& rc = rhoMelt.primitiveField();
        scalarField& src = TEqn.source();
        const scalarField& Vc = mesh.V();

        scalar maxDchiDt = 0;

        forAll(chic, i)
        {
            const scalar chiOld = chic[i];
            const scalar chiNew =
                crystallization_->advance(chiOld, Tc[i], pc[i], dt);

            src[i] +=
                Vc[i]*crystallization_->latentHeat()
               *ac[i]*rc[i]*(chiNew - chiOld)/dt;

            maxDchiDt = max(maxDchiDt, mag(chiNew - chiOld)/dt);
            chic[i] = chiNew;
        }

        chi.correctBoundaryConditions();

        if (runTime.timeIndex() % 50 == 0)
        {
            Info<< "moldingFoam: crystallinity: max(chi) = "
                << gMax(chic) << ", max(dchi/dt) = " << maxDchiDt
                << " 1/s" << endl;
        }
    }

    // Explicit viscous-dissipation (shear heating) source. It is added
    // to the integrated source directly, so the positive sign is
    // unambiguous (Phi always heats the fluid)
    if (viscousDissipation_)
    {
        const tmp<volScalarField> tDiss(viscousDissipationSource());
        const volScalarField& diss = tDiss();

        TEqn.source() += mesh.V()*diss.primitiveField();

        if (runTime.timeIndex() % 50 == 0)
        {
            Info<< "moldingFoam: viscous dissipation: max Phi = "
                << gMax(diss.primitiveField()) << " W/m^3" << endl;
        }
    }

    TEqn.relax();

    fvConstraints().constrain(TEqn);

    TEqn.solve();

    // Safety clamp of the solved temperature: correctThermo's Newton
    // inversion needs a finite, physical starting temperature. The
    // energy matrix remains positive definite across the solidification
    // band (hMeltThermo::Cv is the base Cp plus the latent peak minus
    // the Cp - Cv coupling), so this only catches numerical overshoot
    // from the Picard linearisation of the steep latent-heat plateau
    T = max
    (
        min(T, dimensionedScalar("TMax", dimTemperature, 3000)),
        dimensionedScalar("TMin", dimTemperature, 250)
    );

    fvConstraints().constrain(T);

    mixture_.correctThermo();
    mixture_.correct();

    // Shrinkage indicator from the updated melt density
    if (shrinkage_.valid())
    {
        volScalarField& s = *shrinkageField_;
        scalarField& sc = s.primitiveFieldRef();
        const scalarField& rhoc = mixture_.rho1().primitiveField();

        forAll(sc, i)
        {
            sc[i] = shrinkage_->volumetricShrinkage(rhoc[i]);
        }

        s.correctBoundaryConditions();

        if (runTime.timeIndex() % 50 == 0)
        {
            Info<< "moldingFoam: shrinkage: max(S) = " << gMax(sc)
                << ", min(S) = " << gMin(sc) << endl;
        }
    }
}


void Foam::solvers::moldingFoam::preSolve()
{
    compressibleVoF::preSolve();

    // Record the fill time of cells that just became filled (alpha >= 0.5)
    if (fillTime_.valid())
    {
        volScalarField& ft = *fillTime_;
        scalarField& ftc = ft.primitiveFieldRef();
        const scalarField& ac = alpha1.primitiveField();
        const scalar t(runTime.value());

        forAll(ftc, i)
        {
            if (ac[i] >= 0.5 && ftc[i] > t)
            {
                ftc[i] = t;
            }
        }

        ft.correctBoundaryConditions();

        // Report the last-filled cell (the weld line) at the diagnostic
        // interval: the maximum finite fill time and its location
        if (runTime.timeIndex() % 50 == 0)
        {
            scalar maxFinite(-great);

            forAll(ftc, i)
            {
                if (ftc[i] < great)
                {
                    maxFinite = max(maxFinite, ftc[i]);
                }
            }

            reduce(maxFinite, maxOp<scalar>());

            label maxCell(-1);

            if (maxFinite > -great)
            {
                forAll(ftc, i)
                {
                    if (ftc[i] == maxFinite)
                    {
                        maxCell = i;
                        break;
                    }
                }

                reduce(maxCell, minOp<label>());
            }

            if (Pstream::master() && maxCell >= 0)
            {
                Info<< "moldingFoam: fill time: max = " << maxFinite
                    << " s at " << mesh.C()[maxCell] << " m" << endl;
            }
        }
    }

    // Runtime reload of constant/moldingDict: the runTimeModifiable
    // mechanism only monitors controlDict, so the moulding dictionary is
    // polled explicitly every step
    readMoldingDict();

    // Optional trapped-air diagnostic
    if
    (
        trapAirInterval_ > 0
     && runTime.timeIndex() % trapAirInterval_ == 0
    )
    {
        reportTrappedAir();
    }

    if (!mesh.foundObject<moldingStage>(moldingStage::typeName))
    {
        return;
    }

    moldingStage& stage =
        mesh.lookupObjectRef<moldingStage>(moldingStage::typeName);

    // Seal the vent once the melt front reaches it: the vent passes air
    // but not polymer (moldingVentPressure / moldingVentVelocity)
    if (!stage.ventSealed())
    {
        forAll(U.boundaryField(), pi)
        {
            if
            (
                U.boundaryField()[pi].type()
             == moldingVentVelocityFvPatchVectorField::typeName
            )
            {
                const scalarField& a1p = alpha1.boundaryField()[pi];

                if (gMax(a1p) >= ventSealAlpha_)
                {
                    stage.sealVent();

                    Info<< "moldingFoam: vent sealed by the melt front:"
                        << " max(alpha.melt) = " << gMax(a1p)
                        << " >= " << ventSealAlpha_
                        << " at t = " << runTime.value() << " s" << endl;
                }
            }
        }
    }

    // Stage M2: V/P switch. The injection switches from flow-rate to
    // pressure control once the filled cavity volume fraction reaches
    // switchFraction, or earlier when the gate pressure reaches
    // switchPressure, whichever happens first; the pressure criterion
    // avoids over-compressing the cavity after the vent has sealed
    if (!stage.packing())
    {
        // vTot_ is cached: the mesh is static, so the reduction would
        // return the same value every step
        const scalar filledFraction
        (
            fvc::domainIntegrate(alpha1).value()/max(vTot_, small)
        );
        const scalar pGate(gatePressure());

        if (runTime.timeIndex() % 50 == 0)
        {
            Info<< "moldingFoam: filling: t = " << runTime.value()
                << " s, filled fraction = " << filledFraction
                << ", p_gate = " << pGate << " Pa" << endl;
        }

        if
        (
            filledFraction >= stage.switchFraction()
         || pGate >= switchPressure_
        )
        {
            stage.switchToPacking(runTime.value());

            Info<< "moldingFoam: V/P switch: filled fraction = "
                << filledFraction << ", p_gate = " << pGate
                << " Pa (switchFraction = " << stage.switchFraction()
                << ", switchPressure = " << switchPressure_ << " Pa)"
                << ", at t = " << runTime.value() << " s" << nl
                << "moldingFoam: packing pressure target = "
                << stage.pressure(runTime.value()) << " Pa" << endl;
        }
    }
    else
    {
        const scalar pTarget(stage.pressure(runTime.value()));
        const scalar pGate(gatePressure());

        if (runTime.timeIndex() % 50 == 0)
        {
            Info<< "moldingFoam: packing: t = " << runTime.value()
                << " s, p_gate = " << pGate
                << " Pa, p_target = " << pTarget
                << " Pa" << endl;
        }

        // The gate freezes off at the end of packing, once the target
        // has fallen to the release pressure: it then holds zero flow,
        // so the part cannot drain during cooling
        // Physical gate freeze: seal once the gate melt reaches the
        // no-flow temperature. Optional: the contract's fixed 480 K
        // hot-runner inlet keeps the gate hot, so this requires a
        // configured threshold and a cooling gate
        if (!stage.gateSealed() && gateFreezeTemperature_ > -great)
        {
            const scalar Tgate(gateTemperature());

            if (Tgate <= gateFreezeTemperature_)
            {
                stage.sealGate(runTime.value());

                Info<< "moldingFoam: gate sealed at t = " << runTime.value()
                    << " s (gate temperature = " << Tgate
                    << " K <= gateFreezeTemperature = "
                    << gateFreezeTemperature_ << " K)" << endl;
            }
        }

        const scalar timeInPacking(runTime.value() - stage.switchTime());

        if
        (
            !stage.gateSealed()
         && (pTarget <= releasePressure_ || timeInPacking >= gateSealTime_)
        )
        {
            stage.sealGate(runTime.value());

            Info<< "moldingFoam: gate sealed at t = " << runTime.value()
                << " s (" << timeInPacking << " s after the V/P switch,"
                << " p_target = " << pTarget << " Pa)" << endl;
        }

        // Stage M3: once the packing pressure has been released (the
        // target is at or below cooling.releasePressure) the part
        // continues cooling; it is ready for ejection when the average
        // melt temperature falls below the ejection temperature
        if (pTarget <= releasePressure_ && !ejected_)
        {
            const volScalarField alphaRho1
            (
                alpha1*mixture_.thermo1().rho()
            );
            const scalar m(fvc::domainIntegrate(alphaRho1).value());

            if (m > small)
            {
                const scalar mT
                (
                    fvc::domainIntegrate(alphaRho1*mixture_.T()).value()
                );

                const scalar averageMeltTemperature(mT/m);

                if (averageMeltTemperature <= ejectionTemperature_)
                {
                    Info<< "moldingFoam: ejection criterion met: average melt"
                        << " temperature = " << averageMeltTemperature
                        << " K <= " << ejectionTemperature_
                        << " K at t = " << runTime.value() << " s (cycle "
                        << cycle_ << "/" << nCycles_ << ")" << endl;

                    if (cycle_ < nCycles_)
                    {
                        resetCycle();
                    }
                    else
                    {
                        ejected_ = true;
                        runTime.stopAt(Time::stopAtControl::writeNow);
                    }
                }
                else if (runTime.timeIndex() % 50 == 0)
                {
                    Info<< "moldingFoam: average melt temperature = "
                        << averageMeltTemperature << " K (ejection at "
                        << ejectionTemperature_ << " K)" << endl;
                }
            }
        }
    }
}


// ************************************************************************* //
