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

Foam::solvers::moldingFoam::moldingFoam(fvMesh& mesh)
:
    compressibleVoF(mesh),
    ejected_(false),
    ejectionTemperature_(great),
    releasePressure_(great),
    switchPressure_(great),
    gateSealTime_(great),
    ventSealAlpha_(0.5),
    viscousDissipation_(false),
    dissipationCoeffs_(),
    vTot_(gSum(mesh.V().primitiveField())),
    moldingDictModTime_(0)
{
    // The molding dictionary is the external case-generation contract. The
    // packing group drives the V/P switch and the packing pressure curve
    // (stage M2); the cooling group drives the ejection criterion (stage
    // M3).
    const fileName moldingDictPath(runTime.constant()/fileName("moldingDict"));

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

        // Melt volume fraction on the vent patch above which the vent is
        // sealed (polymer must not escape through the vent)
        const scalar ventSealAlpha =
            moldingDict.lookupOrDefault<scalar>("ventSealAlpha", 0.5);

        // Optional viscous-dissipation (shear heating) source in the
        // energy equation; default false to preserve existing cases
        const bool viscousDissipation =
            moldingDict.lookupOrDefault<Switch>("viscousDissipation", false);

        ejectionTemperature_ = ejectionTemperature;
        releasePressure_ = releasePressure;
        switchPressure_ = switchPressure;
        gateSealTime_ = gateSealTime;
        ventSealAlpha_ = ventSealAlpha;
        viscousDissipation_ = viscousDissipation;

        if (viscousDissipation_)
        {
            readDissipationCoeffs();
        }

        // The stage object registers itself on the mesh and is shared with
        // the molding boundary conditions
        if (!mesh.foundObject<moldingStage>(moldingStage::typeName))
        {
            new moldingStage(mesh, runTime, switchFraction, std::move(pressure));
        }

        Info<< "moldingFoam: read " << moldingDictPath << nl
            << "    packing:" << nl
            << "        switchFraction      = " << switchFraction << nl
            << "        switchPressure      = " << switchPressure << nl
            << "        gateSealTime        = " << gateSealTime << nl
            << "        pressure type       = "
            << packingDict.subDict("pressure").lookup<word>("type") << nl
            << "    cooling:" << nl
            << "        ejectionTemperature = " << ejectionTemperature << nl
            << "        releasePressure     = " << releasePressure << nl
            << "    ventSealAlpha           = " << ventSealAlpha << nl
            << "    viscousDissipation      = " << viscousDissipation
            << endl;
    }

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
    const fileName moldingDictPath
    (
        runTime.constant()/fileName("moldingDict")
    );

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

    const scalar newVentSealAlpha
    (
        moldingDict.lookupOrDefault<scalar>("ventSealAlpha", 0.5)
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
     || newVentSealAlpha != ventSealAlpha_
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
            << ", ventSealAlpha " << ventSealAlpha_
            << " -> " << newVentSealAlpha
            << ", viscousDissipation " << viscousDissipation_
            << " -> " << newViscousDissipation << endl;

        ejectionTemperature_ = newEjectionTemperature;
        releasePressure_ = newReleasePressure;
        switchPressure_ = newSwitchPressure;
        gateSealTime_ = newGateSealTime;
        ventSealAlpha_ = newVentSealAlpha;

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


void Foam::solvers::moldingFoam::readDissipationCoeffs()
{
    const fileName path(runTime.constant()/fileName("momentumTransport"));

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


void Foam::solvers::moldingFoam::thermophysicalPredictor()
{
    // As compressibleVoF::thermophysicalPredictor with one addition:
    // after the linear solve the temperature is clamped to a physical
    // range. The hMelt apparent Cv carries the latent-heat peak on the
    // matrix diagonal, and the Picard linearisation of the steep plateau
    // can overshoot; correctThermo's Newton needs a positive starting
    // temperature.

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
}


void Foam::solvers::moldingFoam::preSolve()
{
    compressibleVoF::preSolve();

    // Runtime reload of constant/moldingDict: the runTimeModifiable
    // mechanism only monitors controlDict, so the moulding dictionary is
    // polled explicitly every step
    readMoldingDict();

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
        const scalar timeInPacking(runTime.value() - stage.switchTime());

        if
        (
            !stage.gateSealed()
         && (pTarget <= releasePressure_ || timeInPacking >= gateSealTime_)
        )
        {
            stage.sealGate();

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
                    ejected_ = true;

                    Info<< "moldingFoam: ejection criterion met: average melt"
                        << " temperature = " << averageMeltTemperature
                        << " K <= " << ejectionTemperature_
                        << " K at t = " << runTime.value() << " s" << endl;

                    runTime.stopAt(Time::stopAtControl::writeNow);
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
