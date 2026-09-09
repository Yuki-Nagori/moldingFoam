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

InClass
    Foam::rhoFluidThermo

Description
    Registers the Tait equation-of-state thermophysical model combinations
    into the basicThermo/fluidThermo/rhoFluidThermo run-time selection
    tables from this library, so that no upstream thermo make-files need to
    be modified. Select with

    \verbatim
        thermoType
        {
            type            heRhoThermo;
            mixture         pureMixture;
            transport       const;
            thermo          hMelt;
            equationOfState Tait;
            specie          specie;
            energy          sensibleInternalEnergy;
        }
    \endverbatim

    in the phase physical-properties dictionary (e.g.
    \c constant/physicalProperties.melt). The \c hMelt thermodynamics add
    an apparent-Cp latent-heat peak at the Tait solidification
    temperature; \c hConst is also registered for phases without
    solidification (e.g. the air phase).

\*---------------------------------------------------------------------------*/

#include "rhoFluidThermo.H"
#include "pureMixture.H"

#include "specie.H"
#include "thermo.H"
#include "constTransport.H"
#include "hConstThermo.H"
#include "sensibleInternalEnergy.H"
#include "sensibleEnthalpy.H"

#include "Tait.H"
#include "hMeltThermo.H"

#include "makeFluidThermo.H"
#include "forThermo.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
    forThermo
    (
        constTransport,
        sensibleInternalEnergy,
        hConstThermo,
        Tait,
        specie,
        makeFluidThermo,
        rhoFluidThermo,
        pureMixture
    );

    forThermo
    (
        constTransport,
        sensibleEnthalpy,
        hConstThermo,
        Tait,
        specie,
        makeFluidThermo,
        rhoFluidThermo,
        pureMixture
    );

    forThermo
    (
        constTransport,
        sensibleInternalEnergy,
        hMeltThermo,
        Tait,
        specie,
        makeFluidThermo,
        rhoFluidThermo,
        pureMixture
    );

    forThermo
    (
        constTransport,
        sensibleEnthalpy,
        hMeltThermo,
        Tait,
        specie,
        makeFluidThermo,
        rhoFluidThermo,
        pureMixture
    );
}

// ************************************************************************* //
