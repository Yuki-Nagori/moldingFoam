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

#include "moldingWarpage.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingWarpage::moldingWarpage(const dictionary& dict)
:
    alpha_(dict.lookup<scalar>("thermalExpansion")),
    E_(dict.lookup<scalar>("elasticModulus")),
    nu_(dict.lookup<scalar>("poissonRatio")),
    Tref_(dict.lookup<scalar>("referenceTemperature"))
{
    if (alpha_ < 0 || E_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The thermal expansion and elastic modulus must be "
            << "non-negative: thermalExpansion = " << alpha_
            << ", elasticModulus = " << E_ << exit(FatalIOError);
    }

    if (nu_ <= -1 || nu_ >= 0.5)
    {
        FatalIOErrorInFunction(dict)
            << "The Poisson ratio must lie in (-1, 0.5): poissonRatio = "
            << nu_ << exit(FatalIOError);
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::scalar Foam::moldingWarpage::freeStrain
(
    const UList<scalar>& T,
    const scalar h
) const
{
    const label n = T.size();

    if (n < 2 || h <= 0)
    {
        return 0;
    }

    // The cell-centre values represent a profile that is piecewise linear
    // between the interfaces and extrapolated linearly to the walls;
    // Simpson's rule per cell is then exact for the quadratic integrands
    // of a linear profile
    const auto Tface = [&](const label i)
    {
        if (i == 0)
        {
            return T[0] - 0.5*(T[1] - T[0]);
        }
        if (i == n)
        {
            return T[n - 1] + 0.5*(T[n - 1] - T[n - 2]);
        }
        return 0.5*(T[i - 1] + T[i]);
    };

    const scalar dy = h/n;

    scalar sum = 0;

    for (label i = 0; i < n; ++i)
    {
        sum += (dy/6)
          *(
                (Tface(i) - Tref_)
              + 4*(T[i] - Tref_)
              + (Tface(i + 1) - Tref_)
            );
    }

    return alpha_*sum/h;
}


Foam::scalar Foam::moldingWarpage::freeCurvature
(
    const UList<scalar>& T,
    const scalar h
) const
{
    const label n = T.size();

    if (n < 2 || h <= 0)
    {
        return 0;
    }

    const scalar dy = h/n;

    // Extrapolated wall values (the centres are dy/2 from the walls)
    const auto Tface = [&](const label i)
    {
        if (i == 0)
        {
            return T[0] - 0.5*(T[1] - T[0]);
        }
        if (i == n)
        {
            return T[n - 1] + 0.5*(T[n - 1] - T[n - 2]);
        }
        return 0.5*(T[i - 1] + T[i]);
    };

    scalar sum = 0;

    for (label i = 0; i < n; ++i)
    {
        const scalar yl = i*dy;
        const scalar yc = (i + 0.5)*dy;
        const scalar yr = (i + 1)*dy;

        sum += (dy/6)
          *(
                (Tface(i) - Tref_)*(yl - 0.5*h)
              + 4*(T[i] - Tref_)*(yc - 0.5*h)
              + (Tface(i + 1) - Tref_)*(yr - 0.5*h)
            );
    }

    return alpha_*12*sum/(h*h*h);
}


Foam::scalar Foam::moldingWarpage::deflection
(
    const scalar kappa,
    const scalar L
) const
{
    return kappa*L*L/2;
}


Foam::scalar Foam::moldingWarpage::bimetalCurvature
(
    const scalar h1,
    const scalar h2,
    const scalar E1,
    const scalar E2,
    const scalar alpha1,
    const scalar alpha2,
    const scalar dT
)
{
    const scalar num
    (
        6*E1*E2*h1*h2*(h1 + h2)*(alpha1 - alpha2)*dT
    );

    const scalar den
    (
        E1*E1*h1*h1*h1*h1
      + E2*E2*h2*h2*h2*h2
      + 2*E1*E2*h1*h2
       *(2*h1*h1 + 2*h2*h2 + 3*h1*h2)
    );

    return num/max(den, small);
}


Foam::scalar Foam::moldingWarpage::constrainedStress(const scalar T) const
{
    return E_/(1 - nu_)*alpha_*(Tref_ - T);
}


void Foam::moldingWarpage::deflectionProfile
(
    const UList<scalar>& kappa,
    const scalar L,
    UList<scalar>& w
) const
{
    const label n = kappa.size();

    if (n < 2 || L <= 0 || w.size() != n)
    {
        return;
    }

    const scalar dx = L/(n - 1);

    // Slope w'(x) = -integral kappa dx + C; w(0) = 0 gives the second
    // integration constant
    scalar slope = 0;
    scalar wLast = 0;

    w[0] = 0;

    for (label i = 1; i < n; ++i)
    {
        // Trapezoidal slope update over the interval
        slope -= 0.5*(kappa[i - 1] + kappa[i])*dx;
        w[i] = w[i - 1] + slope*dx;
        wLast = w[i];
    }

    // Enforce w(L) = 0 by adding a linear correction
    const scalar corr = wLast/L;

    for (label i = 0; i < n; ++i)
    {
        w[i] -= corr*(i*dx);
    }
}


void Foam::moldingWarpage::residualStress
(
    const UList<scalar>& T,
    const scalar h,
    UList<scalar>& sigma
) const
{
    const label n = T.size();

    if (n == 0 || sigma.size() != n)
    {
        return;
    }

    // Membrane strain of the constrained plate: the mean thermal strain
    const scalar eps0 = freeStrain(T, h);

    for (label i = 0; i < n; ++i)
    {
        sigma[i] =
            E_/(1 - nu_)*(alpha_*(T[i] - Tref_) - eps0);
    }
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
