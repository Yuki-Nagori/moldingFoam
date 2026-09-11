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
    Foam::moldingShrinkage

Description
    PVT-consistent shrinkage and residual-stress indicators for the
    moulded part (task 013a).

    The volumetric shrinkage follows from the current melt density rho
    and a user-supplied reference (solid) density rhoRef,

    \verbatim
        S = 1 - rhoRef/rho
    \endverbatim

    which is the free (unconstrained) volumetric shrinkage of the
    material relative to the reference state. The residual thermal stress
    indicator of a constrained plate follows from the temperature
    difference to the reference temperature,

    \verbatim
        sigma = E/(1 - nu) alpha (Tref - T)
    \endverbatim

    These are engineering indicators of the in-mould shrinkage and
    residual stress; the structural warpage analysis itself requires a
    solid mechanics solver and is the subject of task 013b.

SourceFiles
    moldingShrinkage.C

\*---------------------------------------------------------------------------*/

#ifndef moldingShrinkage_H
#define moldingShrinkage_H

#include "dictionary.H"
#include "scalar.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                   Class moldingShrinkage Declaration
\*---------------------------------------------------------------------------*/

class moldingShrinkage
{
    // Private Data

        //- Reference (solid) density [kg/m^3]
        scalar rhoRef_;

        //- Reference temperature [K]
        scalar Tref_;

        //- Elastic modulus [Pa]
        scalar E_;

        //- Poisson ratio [-]
        scalar nu_;

        //- Linear thermal expansion coefficient [1/K]
        scalar alpha_;


public:

    // Constructors

        //- Construct from a dictionary
        moldingShrinkage(const dictionary& dict);


    // Member Functions

        //- Free volumetric shrinkage from the current density
        scalar volumetricShrinkage(const scalar rho) const
        {
            return 1 - rhoRef_/max(rho, small);
        }

        //- Constrained thermal stress indicator [Pa]
        scalar thermalStress(const scalar T) const
        {
            return E_/(1 - nu_)*alpha_*(Tref_ - T);
        }

        //- Reference density [kg/m^3]
        scalar rhoRef() const
        {
            return rhoRef_;
        }

        //- Reference temperature [K]
        scalar Tref() const
        {
            return Tref_;
        }
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
