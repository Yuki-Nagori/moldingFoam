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
    Foam::moldThermalState

Description
    Update rule of the lumped-parameter mould thermal state used by the
    \c moldingMoldTemperature boundary condition.

    The mould is represented by a single temperature \c T with heat
    capacity \c C, exchanging heat with the casting through the
    fluid-side film conductance \c hFilm (driving temperature \c Tfilm),
    with the cooling water through \c hA (temperature \c Tw) and with an
    optional power source \c Q:

    \verbatim
        C dT/dt = hFilm (Tfilm - T) + hA (Tw - T) + Q
    \endverbatim

    The boundary condition advances the state with the backward-Euler
    update of this equation,

    \verbatim
        Tnew = (C/dt Told + hFilm Tfilm + hA Tw + Q)
             / (C/dt + hFilm + hA)
    \endverbatim

    which is the discrete counterpart of the implicit (Euler) energy
    equation of the fluid, unconditionally stable for any time step, and
    conserves the discrete energy balance exactly:

    \verbatim
        C (Tnew - Told) = dt (hFilm (Tfilm - Tnew) + hA (Tw - Tnew) + Q)
    \endverbatim

    The conductance \c hFilm and temperature \c Tfilm are formed from the
    same face conductances \c kappaEff*deltaCoeffs*magSf as the fluid
    energy equation, so the mould exchange is consistent with the
    discretised casting-side flux.

    Note: the update is a pure function of \c Told and the current fluid
    state. The boundary condition always evaluates it from the previous
    time-step value (UniformDimensionedField oldTime), so re-evaluating
    the condition within a time step refines the coupling without
    advancing the state more than once.

SourceFiles
    moldThermalState.C

\*---------------------------------------------------------------------------*/

#ifndef moldThermalState_H
#define moldThermalState_H

#include "scalar.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                       Class moldThermalState Declaration
\*---------------------------------------------------------------------------*/

class moldThermalState
{
public:

    // Static Member Functions

        //- Conductance-weighted driving temperature of two heat paths:
        //  (hA*Ta + hB*Tb)/(hA + hB), or Ta when both conductances vanish
        static scalar Tdrv
        (
            const scalar hA,    //- Conductance of path A [W/K]
            const scalar Ta,    //- Temperature of path A [K]
            const scalar hB,    //- Conductance of path B [W/K]
            const scalar Tb     //- Temperature of path B [K]
        );

        //- Backward-Euler update of the lumped mould temperature
        static scalar Tnew
        (
            const scalar C,         //- Heat capacity [J/K]
            const scalar hA,        //- Coolant-side conductance [W/K]
            const scalar Tw,        //- Coolant temperature [K]
            const scalar Told,      //- Temperature at the previous time [K]
            const scalar hFilm,     //- Casting-side conductance [W/K]
            const scalar Tfilm,     //- Casting-side driving temperature [K]
            const scalar dt,        //- Time step [s]
            const scalar Q = 0      //- Power source [W]
        );
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
