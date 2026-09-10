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

Class
    Foam::solvers::moldingFoam

Description
    Solver module for injection molding of thermoplastic melts.

    Stage M1: equivalent to \c compressibleVoF; melt/air VoF cavity filling
    with the polymer modelled by the CrossWlf generalised Newtonian
    viscosity model and the Tait equation of state.

    Later stages:
    - M2: packing; V/P switch at a filled-volume fraction with a
      pressure-time profile, read from \c constant/moldingDict;
    - M3: cooling; mould wall temperature boundaries and an ejection
      criterion on the average melt temperature.

    Usage in \c system/controlDict:
    \verbatim
        application     foamRun;
        solver          moldingFoam;
        libs            ("libmoldingFoam.so");
    \endverbatim

SourceFiles
    moldingFoam.C

\*---------------------------------------------------------------------------*/

#ifndef moldingFoam_H
#define moldingFoam_H

#include "compressibleVoF.H"
#include <ctime>

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace solvers
{

/*---------------------------------------------------------------------------*\
                          Class moldingFoam Declaration
\*---------------------------------------------------------------------------*/

class moldingFoam
:
    public compressibleVoF
{
    // Private Data

        //- Has the ejection criterion been met and the run stopped?
        bool ejected_;

        //- Average melt temperature below which the part can be ejected
        //  [K], read from constant/moldingDict
        scalar ejectionTemperature_;

        //- Packing pressure target at or below which the packing pressure
        //  counts as released, enabling the ejection check [Pa], read
        //  from constant/moldingDict
        scalar releasePressure_;

        //- Gate pressure at which the V/P switch is triggered even if
        //  the filled fraction is below switchFraction [Pa]; defaults to
        //  great (fraction-only switching)
        scalar switchPressure_;

        //- Time after the V/P switch at which the gate freezes off [s];
        //  defaults to great (seal when the packing pressure is released)
        scalar gateSealTime_;

        //- Melt volume fraction on the vent patch above which the vent
        //  is sealed (polymer must not escape through the vent)
        scalar ventSealAlpha_;

        //- Total mesh volume [m^3], computed once in the constructor;
        //  the mould cavity mesh is static
        scalar vTot_;

        //- Modification time of constant/moldingDict, for the runtime
        //  reload in preSolve
        time_t moldingDictModTime_;




    // Private Member Functions

        //- Reload constant/moldingDict if its modification time changed:
        //  updates the stage packing parameters and the cooling
        //  thresholds, printing old -> new for anything that changed
        void readMoldingDict();

        //- Area-averaged gate pressure [Pa] over the moldingPrghPressure
        //  patches; zero when there is no such patch
        scalar gatePressure() const;


public:

    //- Runtime type information
    TypeName("moldingFoam");


    // Constructors

        //- Construct from region mesh
        moldingFoam(fvMesh& mesh);

        //- Disallow default bitwise copy construction
        moldingFoam(const moldingFoam&) = delete;


    //- Destructor
    virtual ~moldingFoam();


    // Member Functions

        //- Read controls
        virtual bool read();

        //- Advance the moulding stage and the ejection criterion
        //  before the PIMPLE loop
        virtual void preSolve();

        //- Solve the energy equation; clamps the solved temperature to
        //  a physical range after the linear solve
        virtual void thermophysicalPredictor();
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace solvers
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
