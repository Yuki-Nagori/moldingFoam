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

#include "moldingStage.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(moldingStage, 0);
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingStage::moldingStage
(
    const fvMesh& mesh,
    const Time& runTime,
    scalar switchFraction,
    autoPtr<Function1<scalar>> pressure,
    scalar gateSealRamp,
    scalar pressureRamp
)
:
    regIOobject
    (
        IOobject
        (
            typeName,
            Time::timeName(runTime.value()),
            mesh,
            // Persisted into the time directory so a restart from
            // latestTime restores the V/P stage; the pressure curve is
            // always re-read from constant/moldingDict by the solver
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE,
            true
        )
    ),
    switchFraction_(switchFraction),
    stage_(stage::filling),
    switchTime_(-1),
    pressure_(std::move(pressure)),
    pressureRamp_(pressureRamp),
    gateSealed_(false),
    gateSealRamp_(gateSealRamp),
    gateSealTime_(-1),
    ventSealed_(false),
    history_
    (
        IOobject
        (
            "moldingCycleState", runTime.name(), mesh,
            IOobject::READ_IF_PRESENT, IOobject::AUTO_WRITE
        )
    )
{
    if (!history_.found("initialDeltaT"))
    {
        history_.set("initialDeltaT", runTime.deltaTValue());
    }

    // A negative value is the internal "auto" sentinel (the ramp adapts
    // to the switch time); an explicit negative entry is rejected when
    // the dictionary is read

    if (gateSealRamp_ < 0)
    {
        FatalIOErrorInFunction(mesh.time().controlDict())
            << "The gate seal ramp must be non-negative: gateSealRamp = "
            << gateSealRamp_ << exit(FatalIOError);
    }

    if (switchFraction_ <= 0 || switchFraction_ > 1)
    {
        FatalIOErrorInFunction(mesh.time().controlDict())
            << "The packing switchFraction must be in (0, 1]: switchFraction = "
            << switchFraction_
            << exit(FatalIOError);
    }

    // Restore the stage state on a restart from latestTime
    if (headerOk())
    {
        Istream& is = readStream(typeName);

        label packingFlag(0), gateFlag(0), ventFlag(0);
        is >> packingFlag >> switchTime_;
        if (is.good())
        {
            // Optional seal flags (absent in pre-v1.3 restart data)
            is >> gateFlag >> ventFlag;
        }
        if (is.good())
        {
            // Optional gate seal time (task 061): without it a restarted run
            // kept gateSealTime_ at its unset sentinel while gateSealed_ was
            // restored as true, so the seal ramp was treated as long finished
            // (the gate re-seals only on the transition, which no longer
            // happens). A missing field leaves the sentinel in place.
            is >> gateSealTime_;
        }

        // Snapshot the stream state and release the stream before it is
        // used: close() destroys the stream held by readStream
        const bool goodRead(!is.bad());
        close();

        if (goodRead)
        {
            stage_ = packingFlag ? stage::packing : stage::filling;
            gateSealed_ = gateFlag;
            ventSealed_ = ventFlag;

            if (packing())
            {
                Info<< "moldingStage: restart in packing stage"
                    << " (V/P switch time " << switchTime_ << " s)"
                    << ", gateSealed = " << gateSealed_
                    << ", ventSealed = " << ventSealed_
                    << endl;
            }
        }
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::moldingStage::ventSealed(const word& patchName) const
{
    if (!history_.found("sealedVents")) return ventSealed_;
    const wordList vents(history_.lookup<wordList>("sealedVents"));
    return findIndex(vents, patchName) >= 0;
}

void Foam::moldingStage::sealVent(const word& patchName)
{
    wordList vents(history_.lookupOrDefault<wordList>("sealedVents", wordList()));
    if (findIndex(vents, patchName) < 0)
    {
        vents.append(patchName);
        history_.set("sealedVents", vents);
    }
    ventSealed_ = true;
}

void Foam::moldingStage::resetCycle(const scalar t)
{
    history_.set("cycle", cycle() + 1);
    history_.set("cycleStartTime", t);
    history_.set("sealedVents", wordList());
    stage_ = stage::filling;
    switchTime_ = -1;
    gateSealed_ = false;
    gateSealTime_ = -1;
    ventSealed_ = false;
}


Foam::scalar Foam::moldingStage::pressure(scalar t) const
{
    if (!packing())
    {
        return 0;
    }

    return pressure_->value(t - switchTime_);
}


void Foam::moldingStage::switchToPacking(scalar t)
{
    stage_ = stage::packing;
    switchTime_ = t;
}


bool Foam::moldingStage::readData(Istream& is)
{
    label packingFlag(0), gateFlag(0), ventFlag(0);
    is >> packingFlag >> switchTime_;
    if (is.good())
    {
        is >> gateFlag >> ventFlag;
    }
    if (is.good())
    {
        is >> gateSealTime_;
    }

    stage_ = packingFlag ? stage::packing : stage::filling;
    gateSealed_ = gateFlag;
    ventSealed_ = ventFlag;

    return !is.bad();
}


bool Foam::moldingStage::writeData(Ostream& os) const
{
    os  << label(stage_ == stage::packing) << token::SPACE << switchTime_
        << token::SPACE << label(gateSealed_)
        << token::SPACE << label(ventSealed_)
        << token::SPACE << gateSealTime_;

    return os.good();
}


void Foam::moldingStage::read(const dictionary& moldingDict)
{
    const dictionary& packingDict(moldingDict.subDict("packing"));

    const scalar newSwitchFraction
    (
        packingDict.lookup<scalar>("switchFraction")
    );

    if (newSwitchFraction <= 0 || newSwitchFraction > 1)
    {
        FatalIOErrorInFunction(packingDict)
            << "The packing switchFraction must be in (0, 1]: switchFraction = "
            << newSwitchFraction
            << exit(FatalIOError);
    }

    autoPtr<Function1<scalar>> newPressure
    (
        Function1<scalar>::New
        (
            "pressure",
            dimTime,
            dimPressure,
            packingDict
        )
    );

    Info<< "moldingStage: packing parameters updated:"
        << " switchFraction = " << switchFraction_
        << " -> " << newSwitchFraction << nl
        << "moldingStage: pressure type "
        << packingDict.subDict("pressure").lookup<word>("type") << endl;

    const scalar newGateSealRamp
    (
        packingDict.lookupOrDefault<scalar>("gateSealRamp", gateSealRamp_)
    );

    if (newGateSealRamp < 0)
    {
        FatalIOErrorInFunction(packingDict)
            << "gateSealRamp must be non-negative" << exit(FatalIOError);
    }
    scalar newPressureRamp = pressureRamp_;

    if (packingDict.found("pressureRamp"))
    {
        newPressureRamp = packingDict.lookup<scalar>("pressureRamp");

        if (newPressureRamp < 0)
        {
            FatalIOErrorInFunction(packingDict)
                << "The packing pressure ramp must be non-negative: "
                << "pressureRamp = " << newPressureRamp
                << exit(FatalIOError);
        }
    }

    // Only the parameters are updated: the stage state and switch time
    // are preserved, so changes take effect from now on
    switchFraction_ = newSwitchFraction;
    pressure_ = std::move(newPressure);
    gateSealRamp_ = newGateSealRamp;
    pressureRamp_ = newPressureRamp;
}


// ************************************************************************* //
