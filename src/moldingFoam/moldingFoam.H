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
#include "CrossWlf.H"
#include <ctime>

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

class moldingCrystallization;
class moldingFiberOrientation;
class moldingShrinkage;

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

        //- Gate seal ramp duration [s]; zero seals instantaneously
        scalar gateSealRamp_;

        //- Optional time at which to switch to packing regardless of the
        //  fraction/pressure criteria [s]
        scalar forcedSwitchTime_;

        //- Gate mass-weighted average melt temperature at or below which
        //  the gate freezes off [K]; defaults to -great (disabled)
        scalar gateFreezeTemperature_;

        //- Melt volume fraction on the vent patch above which the vent
        //  is sealed (polymer must not escape through the vent)
        scalar ventSealAlpha_;

        //- Trapped-air diagnostic interval [time steps]; zero disables it
        label trapAirInterval_;

        //- Melt volume fraction at or below which a cell counts as air in
        //  the trapped-air diagnostic
        scalar trapAirAlpha_;

        //- Enable the explicit viscous-dissipation (shear heating) source
        //  in the energy equation; read from constant/moldingDict,
        //  default false
        bool viscousDissipation_;

        //- CrossWlf coefficients used by the dissipation source
        laminarModels::generalisedNewtonianViscosityModels::CrossWlf::coeffs
            dissipationCoeffs_;

        //- Optional crystallisation kinetics: relative crystallinity
        //  model and field, active when constant/moldingDict contains a
        //  crystallization sub-dictionary
        autoPtr<moldingCrystallization> crystallization_;

        //- Relative crystallinity field (0..1)
        autoPtr<volScalarField> chi_;

        //- Initial crystallinity field for the cycle reset
        autoPtr<volScalarField> chiInitial_;

        //- Optional fibre orientation: Folgar-Tucker model and second
        //  order orientation tensor field, active when
        //  constant/moldingDict contains a fiberOrientation
        //  sub-dictionary
        autoPtr<moldingFiberOrientation> fiberOrientation_;

        //- Second-order fibre orientation tensor field
        autoPtr<volSymmTensorField> a_;

        //- Initial orientation tensor field for the cycle reset
        autoPtr<volSymmTensorField> aInitial_;

        //- Optional shrinkage/residual-stress indicators: model and
        //  volumetric shrinkage field, active when constant/moldingDict
        //  contains a shrinkage sub-dictionary
        autoPtr<moldingShrinkage> shrinkage_;

        //- Volumetric shrinkage field
        autoPtr<volScalarField> shrinkageField_;

        //- Initial shrinkage field for the cycle reset
        autoPtr<volScalarField> shrinkageInitial_;

        //- Optional fill-time field (the time each cell fills); the last
        //  cells to fill are the weld lines
        bool writeFillTime_;

        autoPtr<volScalarField> fillTime_;

        autoPtr<volScalarField> fillTimeInitial_;

        //- Optional trapped-air field (1 in trapped air cells), written
        //  at the trapped-air diagnostic interval
        autoPtr<volScalarField> airTrap_;

        //- Number of moulding cycles to run [-]; each ejection criterion
        //  met resets the flow fields and starts the next cycle, keeping
        //  the mould thermal state
        label nCycles_;

        //- Current cycle number (1-based)
        label cycle_;

        //- Initial time step [s], restored at each cycle reset
        scalar deltaTInitial_;

        //- Initial fields for the cycle reset
        autoPtr<volScalarField> alpha1Initial_;
        autoPtr<volVectorField> UInitial_;
        autoPtr<volScalarField> TInitial_;
        autoPtr<volScalarField> pInitial_;
        autoPtr<volScalarField> p_rghInitial_;

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

        //- Mass-weighted average melt temperature [K] over the
        //  moldingPrghPressure patch cells; zero when there is no such
        //  patch
        scalar gateTemperature() const;

        //- Read the CrossWlf coefficients for the dissipation source
        //  from constant/momentumTransport
        void readDissipationCoeffs();

        //- Explicit viscous-dissipation source [W/m^3] for the energy
        //  equation: 2*eta*dev(symm(grad(U))) && grad(U), with eta from
        //  the same CrossWlf model and strain rate as the momentum
        //  equation
        tmp<volScalarField> viscousDissipationSource() const;

        //- Reset the flow fields for the next moulding cycle; the mould
        //  thermal state and the packing parameters are kept
        void resetCycle();

        //- Detect and report air regions that are not connected to the
        //  open vent (trapped air): cells, volume, mass, mean pressure,
        //  maximum temperature and centroid; parallel-safe
        void reportTrappedAir();


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
        //- Override the base pre-predictor to enforce the melt-gate flux
        //  consistency
        virtual void prePredictor();

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
