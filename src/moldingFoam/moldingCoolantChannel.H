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
    Foam::moldingCoolantChannel

Description
    One-dimensional plug-flow model of a cooling channel, used by the
    \c moldingMoldTemperature boundary condition.

    The wetted faces of a channel patch are grouped into cross-sections
    by their position along the flow direction. Within one cross-section
    the coolant is assumed well mixed, so all its faces see the same bulk
    temperature \c Tc and the section advances it by

    \verbatim
        Tc_out = Tc_in + Q_section/(mdot Cp)
        Q_section = sum_f htc A_f (Twall_f - Tc_in)
    \endverbatim

    which is the discrete counterpart of the steady plug-flow energy
    equation along the channel, and conserves the discrete channel energy
    balance exactly:

    \verbatim
        mdot Cp (Tc_out - Tc_in) = sum_f htc A_f (Twall_f - Tc_in)
    \endverbatim

    The coolant heat transfer coefficient is either prescribed or formed
    from a power-law Nusselt correlation, \c Nu = C Re^m Pr^n, and the
    fluid conductivity and hydraulic diameter, \c h = Nu k/D.

SourceFiles
    moldingCoolantChannel.C

\*---------------------------------------------------------------------------*/

#ifndef moldingCoolantChannel_H
#define moldingCoolantChannel_H

#include "scalar.H"
#include "UList.H"
#include "labelList.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                  Class moldingCoolantChannel Declaration
\*---------------------------------------------------------------------------*/

class moldingCoolantChannel
{
public:

    // Static Member Functions

        //- Nusselt number from the power-law correlation Nu = C Re^m Pr^n
        static scalar Nu
        (
            const scalar C,
            const scalar m,
            const scalar n,
            const scalar Re,
            const scalar Pr
        );

        //- Heat transfer coefficient h = Nu k/D [W/m^2/K]
        static scalar htcFromNu
        (
            const scalar Nu,
            const scalar k,
            const scalar D
        );

        //- Group wetted faces into cross-sections by axial position. The
        //  faces must be sorted by ascending axial position; faces closer
        //  than tol share one cross-section. Returns the total
        //  conductance htc*magSf of each cross-section [W/K] and the
        //  index of its first face
        static void group
        (
            const UList<scalar>& magSf,     //- Wetted face areas [m^2]
            const UList<scalar>& proj,      //- Axial positions [m]
            const scalar htc,               //- HTC [W/m^2/K]
            const scalar tol,               //- Position tolerance [m]
            List<scalar>& groupHA,          //- Section conductances [W/K]
            labelList& groupStart           //- First face of each section
        );

        //- March the cross-sections in axial order. Returns the total
        //  heat transferred to the coolant, sum_g groupHA[g]*groupTc[g]
        //  [W K], and fills the bulk temperature entering each section
        //  [K]. mdotCp is the coolant capacity flow rate [W/K]
        static scalar march
        (
            const scalar Tin,               //- Inlet temperature [K]
            const scalar mdotCp,            //- Capacity flow rate [W/K]
            const UList<scalar>& groupHA,   //- Section conductances [W/K]
            const UList<scalar>& groupTw,   //- Section wall temperatures [K]
            List<scalar>& groupTc           //- Entering bulk temperatures [K]
        );
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
