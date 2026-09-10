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
    Foam::ventOrifice

Description
    Quasi-steady orifice relation between the mass flow through a vent and
    the pressure required to drive it:

    \verbatim
        pVent = p0 + sign(mDot) mDot^2 / (2 rho CdA^2)
    \endverbatim

    where \c p0 is the ambient pressure, \c rho the upstream density and
    \c CdA the effective discharge area (discharge coefficient times area).
    It models a restricted vent: the cavity back-pressure rises above the
    ambient pressure as the escaping gas flow increases, and a zero flow
    recovers the open-vent ambient pressure. Positive \c mDot is outward
    (venting), negative is inward (ingestion).

    The relation is the incompressible orifice law, adequate for the small
    Mach numbers of mould venting; a choked/isentropic extension is a
    future refinement (see ai-docs/tasks/003).

SourceFiles
    ventOrifice.C

\*---------------------------------------------------------------------------*/

#ifndef ventOrifice_H
#define ventOrifice_H

#include "scalar.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                           Class ventOrifice Declaration
\*---------------------------------------------------------------------------*/

class ventOrifice
{
public:

    // Static Member Functions

        //- Pressure required to drive mDot through an orifice of effective
        //  area CdA into an ambient at p0, with upstream density rho [Pa]
        static scalar pVent
        (
            const scalar p0,    //- Ambient pressure [Pa]
            const scalar rho,   //- Upstream density [kg/m^3]
            const scalar mDot,  //- Outward mass flow rate [kg/s]
            const scalar CdA    //- Effective discharge area [m^2]
        );
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
