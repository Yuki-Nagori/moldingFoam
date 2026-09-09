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
    vTot_(gSum(mesh.V().primitiveField()))
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

        ejectionTemperature_ = ejectionTemperature;

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
            << "        ejectionTemperature = " << ejectionTemperature
            << endl;
    }
    else
    {
        WarningInFunction
            << "No " << moldingDictPath << " found."
            << " The molding contract dictionary is optional in stage M1"
            << " and becomes mandatory in stages M2/M3." << endl;
    }
}


// * * * * * * * * * * * * * * * * * Destructors  * * * * * * * * * * * * * //

Foam::solvers::moldingFoam::~moldingFoam()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::solvers::moldingFoam::read()
{
    if (compressibleVoF::read())
    {
        return true;
    }
    else
    {
        return false;
    }
}


void Foam::solvers::moldingFoam::preSolve()
{
    compressibleVoF::preSolve();

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

        // Stage M3: once the packing pressure has been released to
        // atmospheric the part continues cooling; it is ready for ejection
        // when the average melt temperature falls below the ejection
        // temperature
        if (pTarget <= 1e5 && !ejected_)
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
