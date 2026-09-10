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
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

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
#include "dimensionedScalar.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace solvers
{
    defineTypeNameAndDebug(moldingFoam, 0);

    addToRunTimeSelectionTable(solver, moldingFoam, fvMesh);
}
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::solvers::moldingFoam::moldingFoam(fvMesh& mesh)
:
    compressibleVoF(mesh),
    ejected_(false),
    ejectionTemperature_(great),
    releasePressure_(great),
    latentOn_(false),
    latentTt0_(0),
    latentB6_(0),
    latentBand_(0),
    latentHeat_(0),
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

        ejectionTemperature_ = ejectionTemperature;
        releasePressure_ = releasePressure;

        // The stage object registers itself on the mesh and is shared with
        // the molding boundary conditions
        if (!mesh.foundObject<moldingStage>(moldingStage::typeName))
        {
            new moldingStage(mesh, runTime, switchFraction, std::move(pressure));
        }

        Info<< "moldingFoam: read " << moldingDictPath << nl
            << "    packing:" << nl
            << "        switchFraction      = " << switchFraction << nl
            << "        pressure type       = "
            << packingDict.subDict("pressure").lookup<word>("type") << nl
            << "    cooling:" << nl
            << "        ejectionTemperature = " << ejectionTemperature << nl
            << "        releasePressure     = " << releasePressure
            << endl;
    }

    // Latent-heat linearisation parameters: read from the melt phase
    // physical properties so the energy predictor (see
    // thermophysicalPredictor) can add the apparent-Cp diagonal when
    // the hMelt thermodynamics with non-zero latentHeat are selected.
    // The Tait coefficients are intentionally duplicated from the phase
    // dictionary: the abstract thermo interface does not expose them.
    const fileName meltPropsPath
    (
        runTime.constant()/fileName("physicalProperties.melt")
    );

    if (isFile(meltPropsPath))
    {
        IFstream is(meltPropsPath);

        if (!is.good())
        {
            FatalIOErrorInFunction(meltPropsPath)
                << "Cannot open " << meltPropsPath
                << exit(FatalIOError);
        }

        dictionary meltDict(is);

        latentOn_ =
            meltDict.subDict("thermoType").lookup<word>("thermo")
         == "hMelt";

        if (latentOn_)
        {
            const dictionary& eqnDict
            (
                meltDict.subDict("mixture").subDict("equationOfState")
            );
            const dictionary& thermoDict
            (
                meltDict.subDict("mixture").subDict("thermodynamics")
            );

            latentTt0_ = eqnDict.lookup<scalar>("b5");
            latentB6_ = eqnDict.lookup<scalar>("b6");
            latentBand_ = eqnDict.lookupOrDefault<scalar>("smoothBand", 0);
            latentHeat_ =
                thermoDict.lookupOrDefault<scalar>("latentHeat", 0);

            latentOn_ = latentHeat_ > 0 && latentBand_ > 0;

            if (latentOn_)
            {
                Info<< "moldingFoam: latent-heat linearisation enabled:"
                    << " latentHeat = " << latentHeat_
                    << " J/kg, Tt0 = " << latentTt0_
                    << " K, band = " << latentBand_ << " K" << endl;
            }
        }
    }
    else
    {
        WarningInFunction
            << "No " << moldingDictPath << " found."
            << " The molding contract dictionary is optional in stage M1"
            << " and becomes mandatory in stages M2/M3." << endl;
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

    if
    (
        newEjectionTemperature != ejectionTemperature_
     || newReleasePressure != releasePressure_
    )
    {
        Info<< "moldingFoam: cooling parameters updated:"
            << " ejectionTemperature " << ejectionTemperature_
            << " -> " << newEjectionTemperature
            << ", releasePressure " << releasePressure_
            << " -> " << newReleasePressure << endl;

        ejectionTemperature_ = newEjectionTemperature;
        releasePressure_ = newReleasePressure;
    }

    if (mesh.foundObject<moldingStage>(moldingStage::typeName))
    {
        mesh.lookupObjectRef<moldingStage>(moldingStage::typeName)
            .read(moldingDict);
    }

    moldingDictModTime_ = modTime;
}


void Foam::solvers::moldingFoam::thermophysicalPredictor()
{
    // Reproduces compressibleVoF::thermophysicalPredictor, plus a
    // semi-implicit latent-heat linearisation when the melt phase uses
    // the hMelt thermodynamics. Inside the Tait solidification band the
    // apparent Cv = Cp - CpMCv goes negative (the pressure-shifted front
    // term of CpMCv = T*alphav^2/psi dominates), which the T matrix
    // cannot tolerate. The extra SuSp term adds rho1*alpha1*latentCp/dt
    // to the diagonal: the implicit-Euler discretisation of the latent
    // storage rate rho*latentHeat*dw/dT*dT/dt (the standard effective
    // capacity treatment). It vanishes at steady state, so the converged
    // equation and its conservation properties are unchanged. Combined
    // with a limitTemperature fvConstraint (case side) the solved T
    // stays bounded while crossing the band.

    const volScalarField& rho1(mixture_.rho1());
    const volScalarField& rho2(mixture_.rho2());
    const volScalarField& e1(mixture_.thermo1().he());
    const volScalarField& e2(mixture_.thermo2().he());

    const fvScalarMatrix e1Source(fvModels().source(alpha1, rho1, e1));
    const fvScalarMatrix e2Source(fvModels().source(alpha2, rho2, e2));

    volScalarField& T = mixture_.T();

    const volScalarField::Internal& Cv1 = mixture_.thermo1().Cv()();
    const volScalarField::Internal& Cv2 = mixture_.thermo2().Cv()();

    fvScalarMatrix TEqn
    (
        correction
        (
            Cv1
           *(
                fvm::ddt(alpha1, rho1, T) + fvm::div(alphaRhoPhi1, T)
              - (
                    e1Source.hasDiag()
                  ? fvm::Sp(contErr1(), T) + fvm::Sp(e1Source.A(), T)
                  : fvm::Sp(contErr1(), T)
                )
            )
          + Cv2
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

    if (latentOn_)
    {
        // Peak of the apparent-Cp latent term of the melt phase:
        // latentHeat*1.5/(2*band) [J/kg/K] at the band centre
        const dimensionedScalar latentCpPeak
        (
            "latentCpPeak",
            (dimEnergy/dimMass)/dimTemperature,
            latentHeat_*1.5/(2*latentBand_)
        );

        volScalarField::Internal latentCpW
        (
            IOobject
            (
                "latentCpW",
                T.instance(),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh,
            latentCpPeak
        );

        const volScalarField::Internal& Ti = T();
        const volScalarField::Internal& pi = p();

        forAll(latentCpW, i)
        {
            const scalar Tt(latentTt0_ + latentB6_*pi[i]);
            const scalar x
            (
                min
                (
                    max((Ti[i] - Tt + latentBand_)/(2*latentBand_), 0),
                    1
                )
            );
            latentCpW[i] *= 6*x*(1 - x);   // normalised shape, peak 1
        }

        // Semi-implicit latent storage: at convergence (T = T.oldTime())
        // the term vanishes, so the solved equation remains the exact
        // energy equation
        TEqn +=
            fvm::SuSp
            (
                alpha1()*rho1()*latentCpW/runTime.deltaT(),
                T
            );
    }

    TEqn.relax();

    fvConstraints().constrain(TEqn);

    TEqn.solve();

    // Clamp the solved temperature: cells inside the solidification band
    // have a near-zero/negative apparent Cv and the linear solve can
    // overshoot; correctThermo's Newton needs a positive starting T
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

    // Stage M2: V/P switch. Once the filled cavity volume fraction
    // reaches switchFraction the injection boundary conditions switch
    // from flow-rate control (filling) to pressure control (packing)
    if (!stage.packing())
    {
        // vTot_ is cached: the mesh is static, so the reduction would
        // return the same value every step
        const scalar filledFraction
        (
            fvc::domainIntegrate(alpha1).value()/max(vTot_, small)
        );

        if (filledFraction >= stage.switchFraction())
        {
            stage.switchToPacking(runTime.value());

            Info<< "moldingFoam: V/P switch: filled fraction = "
                << filledFraction << " >= switchFraction = "
                << stage.switchFraction() << " at t = " << runTime.value()
                << " s" << nl
                << "moldingFoam: packing pressure target = "
                << stage.pressure(runTime.value()) << " Pa" << endl;
        }
    }
    else
    {
        const scalar pTarget(stage.pressure(runTime.value()));

        // Log the packing pressure target against the area-averaged gate
        // pressure every 50 time steps
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
                const scalar a(gSum(mag(tSf())));

                const scalar gatePressure
                (
                    gSum(prghp*mag(tSf()))/max(a, small)
                );

                if (runTime.timeIndex() % 50 == 0)
                {
                    Info<< "moldingFoam: packing: t = " << runTime.value()
                        << " s, p_gate = " << gatePressure
                        << " Pa, p_target = " << pTarget
                        << " Pa" << endl;
                }
            }
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
