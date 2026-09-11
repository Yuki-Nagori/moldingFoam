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

Class
    Foam::moldingStage

Description
    Shared moulding-stage state for the moldingFoam solver module and its
    boundary conditions, registered on the mesh object registry under the
    name \c moldingStage.

    The solver detects the V/P switch, i.e. the instant at which the
    filled cavity volume fraction reaches the \c switchFraction read from
    \c constant/moldingDict, and switches the stored stage from filling to
    packing. From that instant the stored packing pressure curve (a
    Function1 of time relative to the switch) provides the target gate
    pressure consumed by the moldingPrghPressure boundary condition.

    This is stage M2 of the moldingFoam roadmap; see the project README.

SourceFiles
    moldingStage.C

\*---------------------------------------------------------------------------*/

#ifndef moldingStage_H
#define moldingStage_H

#include "Function1.H"
#include "fvMesh.H"
#include "regIOobject.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                          Class moldingStage Declaration
\*---------------------------------------------------------------------------*/

class moldingStage
:
    public regIOobject
{
public:

    //- The moulding stage
    enum class stage
    {
        filling, //!< Filling: flow-rate controlled injection
        packing  //!< Packing: pressure controlled following the curve
    };


private:

    // Private Data

        //- Filled cavity volume fraction at which to switch to packing [-]
        scalar switchFraction_;

        //- Current stage
        stage stage_;

        //- Time at which the V/P switch occurred [s]
        scalar switchTime_;

        //- Packing pressure as a function of time relative to the switch
        //  [Pa]
        autoPtr<Function1<scalar>> pressure_;

        //- Has the gate frozen off at the end of packing?
        bool gateSealed_;

        //- Gate seal ramp duration [s]; zero seals instantaneously
        scalar gateSealRamp_;

        //- Time at which the gate seal was triggered [s]
        scalar gateSealTime_;

        //- Has the vent been sealed by the arriving melt front?
        bool ventSealed_;


public:

    //- Runtime type information
    TypeName("moldingStage");


    // Constructors

        //- Construct from mesh, switch fraction and packing pressure curve
        moldingStage
        (
            const fvMesh& mesh,
            const Time& runTime,
            scalar switchFraction,
            autoPtr<Function1<scalar>> pressure,
            scalar gateSealRamp = 0
        );

        //- Disallow default bitwise copy construction
        moldingStage(const moldingStage&) = delete;


    //- Destructor
    virtual ~moldingStage() = default;


    // Member Functions

        //- Is the injection pressure controlled, i.e. has the V/P switch
        //  occurred?
        bool packing() const
        {
            return stage_ == stage::packing;
        }

        //- Return the filled cavity volume fraction at which the switch
        //  occurs [-]
        scalar switchFraction() const
        {
            return switchFraction_;
        }

        //- Return the time at which the V/P switch occurred [s]
        scalar switchTime() const
        {
            return switchTime_;
        }

        //- Has the gate been sealed (frozen off) at the end of packing?
        bool gateSealed() const
        {
            return gateSealed_;
        }

        //- Seal the gate at time t: the gate flow ramps to zero over
        //  gateSealRamp seconds (zero ramp seals instantaneously)
        void sealGate(const scalar t)
        {
            gateSealed_ = true;
            gateSealTime_ = t;
        }

        //- Gate seal ramp factor in [0, 1]: 1 before the seal, linearly
        //  decreasing to 0 over gateSealRamp after the seal time
        scalar gateSealFactor(const scalar t) const
        {
            if (!gateSealed_)
            {
                return 1;
            }

            if (gateSealRamp_ <= 0)
            {
                return 0;
            }

            return max(scalar(0), min(scalar(1), 1 - (t - gateSealTime_)/gateSealRamp_));
        }

        //- Gate seal ramp duration [s]
        scalar gateSealRamp() const
        {
            return gateSealRamp_;
        }

        //- Has the vent been sealed by the arriving melt front?
        bool ventSealed() const
        {
            return ventSealed_;
        }

        //- Seal the vent: no further flow through the vent
        void sealVent()
        {
            ventSealed_ = true;
        }

        //- Reset for the next moulding cycle: back to the filling stage
        //  with the seal flags cleared; the packing parameters are kept
        void resetCycle()
        {
            stage_ = stage::filling;
            switchTime_ = -1;
            gateSealed_ = false;
            gateSealTime_ = -1;
            ventSealed_ = false;
        }

        //- Return the packing pressure target at time t [Pa]; zero before
        //  the switch
        scalar pressure(scalar t) const;

        //- Switch to the packing stage at time t
        void switchToPacking(scalar t);

        //- Read the persisted stage state (packing flag and switch time);
        //  the pressure curve is always taken from constant/moldingDict
        virtual bool readData(Istream& is);

        //- Persist the stage state for restart from latestTime
        virtual bool writeData(Ostream& os) const;

        //- Update the packing parameters (switchFraction and pressure
        //  curve) from the packing sub-dictionary of a moulding
        //  dictionary; the stage state and switch time are preserved, so
        //  parameter changes only affect future switching
        void read(const dictionary& moldingDict);
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
