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

\*---------------------------------------------------------------------------*/

#include "moldingShrinkage.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingShrinkage::moldingShrinkage(const dictionary& dict)
:
    rhoRef_(dict.lookup<scalar>("referenceDensity")),
    Tref_(dict.lookup<scalar>("referenceTemperature")),
    E_(dict.lookup<scalar>("elasticModulus")),
    nu_(dict.lookup<scalar>("poissonRatio")),
    alpha_(dict.lookup<scalar>("thermalExpansion"))
{
    if (rhoRef_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The reference density must be positive: referenceDensity = "
            << rhoRef_ << exit(FatalIOError);
    }

    if (E_ < 0 || alpha_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The elastic modulus and thermal expansion must be "
            << "non-negative: elasticModulus = " << E_
            << ", thermalExpansion = " << alpha_
            << exit(FatalIOError);
    }

    if (nu_ <= -1 || nu_ >= 0.5)
    {
        FatalIOErrorInFunction(dict)
            << "The Poisson ratio must lie in (-1, 0.5): poissonRatio = "
            << nu_ << exit(FatalIOError);
    }
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
