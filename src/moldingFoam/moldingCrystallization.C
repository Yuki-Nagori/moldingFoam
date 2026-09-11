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

#include "moldingCrystallization.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingCrystallization::moldingCrystallization(const dictionary& dict)
:
    n_(dict.lookup<scalar>("avramiExponent")),
    Kmax_(dict.lookup<scalar>("rateConstant")),
    Tmax_(dict.lookup<scalar>("peakTemperature")),
    width_(dict.lookup<scalar>("windowWidth")),
    dTdp_(dict.lookupOrDefault<scalar>("peakTemperaturePressureShift", 0)),
    latentHeat_(dict.lookup<scalar>("latentHeat")),
    rho_(dict.lookup<scalar>("rho"))
{
    if (n_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The Avrami exponent must be positive: avramiExponent = "
            << n_ << exit(FatalIOError);
    }

    if (Kmax_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The peak crystallisation rate must be positive: "
            << "rateConstant = " << Kmax_ << exit(FatalIOError);
    }

    if (width_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The crystallisation window width must be positive: "
            << "windowWidth = " << width_ << exit(FatalIOError);
    }

    if (latentHeat_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The latent heat of crystallisation must be non-negative: "
            << "latentHeat = " << latentHeat_ << exit(FatalIOError);
    }

    if (rho_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The melt density must be positive: rho = " << rho_
            << exit(FatalIOError);
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::scalar Foam::moldingCrystallization::K
(
    const scalar T,
    const scalar p
) const
{
    const scalar dT = (T - Tmax(p))/width_;

    return Kmax_*exp(-4*log(scalar(2))*dT*dT);
}


Foam::scalar Foam::moldingCrystallization::dchiDt
(
    const scalar chi,
    const scalar T,
    const scalar p
) const
{
    const scalar c(min(max(chi, scalar(0)), scalar(1)));

    if (c >= 1)
    {
        return 0;
    }

    // (-ln(1 - chi))^((n-1)/n); at chi = 0 the limit is 0 for n > 1 and
    // infinite for n < 1, so use the equivalent-time form instead
    const scalar xi = pow(-log(max(1 - c, small)), 1/n_);

    return n_*K(T, p)*pow(xi, n_ - 1)*max(1 - c, small);
}


Foam::scalar Foam::moldingCrystallization::advance
(
    const scalar chi,
    const scalar T,
    const scalar p,
    const scalar dt
) const
{
    const scalar c(min(max(chi, scalar(0)), scalar(1)));

    if (c >= 1 || dt <= 0)
    {
        return c;
    }

    // Equivalent Avrami time of the current state, then the exact
    // constant-rate step
    const scalar xi = pow(-log(max(1 - c, small)), 1/n_);
    const scalar xiNew = xi + K(T, p)*dt;

    return min(1 - exp(-pow(xiNew, n_)), scalar(1));
}


Foam::scalar Foam::moldingCrystallization::latentSource
(
    const scalar chi,
    const scalar T,
    const scalar p,
    const scalar dt
) const
{
    if (dt <= 0 || latentHeat_ <= 0)
    {
        return 0;
    }

    const scalar chiNew = advance(chi, T, p, dt);

    return rho_*latentHeat_*(chiNew - min(max(chi, scalar(0)), scalar(1)))
      /dt;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
