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

#include "moldingViscoelastic.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

//- Flat component index of a symmetric tensor from the (i, j) pair
static inline direction symmIndex(const direction i, const direction j)
{
    if (i == 0)
    {
        return j;
    }
    if (i == 1)
    {
        return j == 0 ? 1 : (j == 1 ? 3 : 4);
    }
    return j == 0 ? 2 : (j == 1 ? 4 : 5);
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingViscoelastic::moldingViscoelastic(const dictionary& dict)
:
    lambda_(dict.lookup<scalar>("relaxationTime")),
    eta0_(dict.lookup<scalar>("zeroShearViscosity")),
    alpha_(dict.lookupOrDefault<scalar>("mobilityFactor", 0))
{
    if (lambda_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The relaxation time must be positive: relaxationTime = "
            << lambda_ << exit(FatalIOError);
    }

    if (eta0_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The zero-shear viscosity must be positive: "
            << "zeroShearViscosity = " << eta0_ << exit(FatalIOError);
    }

    if (alpha_ < 0 || alpha_ > 1)
    {
        FatalIOErrorInFunction(dict)
            << "The Giesekus mobility factor must lie in [0, 1]: "
            << "mobilityFactor = " << alpha_ << exit(FatalIOError);
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::symmTensor Foam::moldingViscoelastic::dtauDt
(
    const symmTensor& tau,
    const tensor& L
) const
{
    const symmTensor D(symm(L));

    symmTensor result(symmTensor::zero);

    for (direction i = 0; i < 3; ++i)
    {
        for (direction j = 0; j < 3; ++j)
        {
            scalar Ltau = 0;
            scalar tauLT = 0;
            scalar tau2 = 0;

            for (direction k = 0; k < 3; ++k)
            {
                Ltau += L(i, k)*tau[symmIndex(k, j)];
                tauLT += tau[symmIndex(i, k)]*L(j, k);
                tau2 += tau[symmIndex(i, k)]*tau[symmIndex(k, j)];
            }

            result[symmIndex(i, j)] =
                (Ltau + tauLT)
              + (
                    2*eta0_*D[symmIndex(i, j)]
                  - (lambda_*alpha_/eta0_)*tau2
                  - tau[symmIndex(i, j)]
                )/lambda_;
        }
    }

    return symm(result);
}


Foam::symmTensor Foam::moldingViscoelastic::advance
(
    const symmTensor& tau,
    const tensor& L,
    const scalar dt
) const
{
    const symmTensor k1(dtauDt(tau, L));
    const symmTensor tauMid(symm(tau + 0.5*dt*k1));
    const symmTensor k2(dtauDt(tauMid, L));

    return symm(tau + dt*k2);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
