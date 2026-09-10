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

#include "moldThermalState.H"
#include "error.H"

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::scalar Foam::moldThermalState::Tnew
(
    const scalar C,
    const scalar hA,
    const scalar Tw,
    const scalar Told,
    const scalar hFilm,
    const scalar Tfilm,
    const scalar dt,
    const scalar Q
)
{
    if (C <= 0)
    {
        FatalErrorInFunction
            << "The lumped mould heat capacity must be positive: C = "
            << C << exit(FatalError);
    }

    if (dt <= 0)
    {
        return Told;
    }

    const scalar Hs(C/dt);

    return
    (
        Hs*Told + hFilm*Tfilm + hA*Tw + Q
    )/(Hs + hFilm + hA);
}


// ************************************************************************* //
