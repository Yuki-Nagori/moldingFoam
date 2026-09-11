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

#include "moldingFiberOrientation.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

//- Flat component index of a symmetric tensor from the (i, j) pair;
//  OpenFOAM's SymmTensor has no (i, j) accessor
static inline Foam::direction symmIndex
(
    const Foam::direction i,
    const Foam::direction j
)
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

Foam::moldingFiberOrientation::moldingFiberOrientation(const dictionary& dict)
:
    lambda_(0),
    CI_(dict.lookup<scalar>("interactionCoefficient")),
    closure_(closureType::quadratic)
{
    if (dict.found("lambda"))
    {
        lambda_ = dict.lookup<scalar>("lambda");
    }
    else
    {
        lambda_ = shapeFactor(dict.lookup<scalar>("aspectRatio"));
    }

    if (mag(lambda_) >= 1)
    {
        FatalIOErrorInFunction(dict)
            << "The fibre shape factor must lie in (-1, 1): lambda = "
            << lambda_ << exit(FatalIOError);
    }

    if (CI_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "The fibre interaction coefficient must be non-negative: "
            << "interactionCoefficient = " << CI_ << exit(FatalIOError);
    }

    const word closureName(dict.lookupOrDefault<word>("closure", "quadratic"));

    if (closureName == "quadratic")
    {
        closure_ = closureType::quadratic;
    }
    else if (closureName == "hybrid")
    {
        closure_ = closureType::hybrid;
    }
    else
    {
        FatalIOErrorInFunction(dict)
            << "Unknown fibre closure " << closureName
            << "; expected quadratic or hybrid" << exit(FatalIOError);
    }
}


// * * * * * * * * * * * * * * * Static Member Functions * * * * * * * * * * //

Foam::scalar Foam::moldingFiberOrientation::shapeFactor(const scalar r)
{
    return (r*r - 1)/(r*r + 1);
}


// * * * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * //

Foam::symmTensor Foam::moldingFiberOrientation::AcolonD
(
    const symmTensor& a,
    const symmTensor& D
) const
{
    // Quadratic closure: A:D = a (a:D)
    const scalar aD(a && D);

    if (closure_ == closureType::quadratic)
    {
        return aD*a;
    }

    // Hybrid closure: A = f A_linear + (1 - f) A_quadratic,
    // f = 1 - 27 det(a)
    const scalar f(1 - 27*det(a));

    // Linear closure contraction:
    //   (A_lin:D)_ij = -1/35 (delta_ij trD + 2 D_ij)
    //                + 1/7 (a_ij trD + delta_ij (a:D)
    //                       + 2 (a D)_ij + 2 (D a)_ij)
    const scalar trD(tr(D));

    symmTensor result(symmTensor::zero);

    for (direction i = 0; i < 3; ++i)
    {
        for (direction j = 0; j < 3; ++j)
        {
            scalar aDij = 0;
            scalar Daij = 0;

            for (direction k = 0; k < 3; ++k)
            {
                aDij += a[symmIndex(i, k)]*D[symmIndex(k, j)];
                Daij += D[symmIndex(i, k)]*a[symmIndex(k, j)];
            }

            const scalar deltaij(i == j ? 1.0 : 0.0);

            const scalar linear =
                -trD*deltaij/35 - 2*D[symmIndex(i, j)]/35
              + (
                    a[symmIndex(i, j)]*trD + deltaij*aD
                  + 2*aDij + 2*Daij
                )/7;

            result[symmIndex(i, j)] =
                f*linear + (1 - f)*aD*a[symmIndex(i, j)];
        }
    }

    return symm(result);
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::symmTensor Foam::moldingFiberOrientation::dadt
(
    const symmTensor& a,
    const tensor& gradU
) const
{
    const tensor W(0.5*(gradU - gradU.T()));
    const symmTensor D(symm(gradU));
    const scalar gammaDot(sqrt(2.0*(D && D)));

    const symmTensor AD(AcolonD(a, D));

    symmTensor result(symmTensor::zero);

    for (direction i = 0; i < 3; ++i)
    {
        for (direction j = 0; j < 3; ++j)
        {
            scalar wa = 0;
            scalar aw = 0;
            scalar da = 0;
            scalar ad = 0;

            for (direction k = 0; k < 3; ++k)
            {
                wa += W(i, k)*a[symmIndex(k, j)];
                aw += a[symmIndex(i, k)]*W(k, j);
                da += D[symmIndex(i, k)]*a[symmIndex(k, j)];
                ad += a[symmIndex(i, k)]*D[symmIndex(k, j)];
            }

            const scalar deltaij(i == j ? 1.0 : 0.0);

            result[symmIndex(i, j)] =
                (wa - aw)
              + lambda_*((da + ad) - 2*AD[symmIndex(i, j)])
              + 2*CI_*gammaDot*(deltaij - 3*a[symmIndex(i, j)]);
        }
    }

    return symm(result);
}


Foam::symmTensor Foam::moldingFiberOrientation::advance
(
    const symmTensor& a,
    const tensor& gradU,
    const scalar dt
) const
{
    // Midpoint (RK2) step
    const symmTensor k1(dadt(a, gradU));
    const symmTensor aMid(symm(a + 0.5*dt*k1));
    const symmTensor k2(dadt(aMid, gradU));

    symmTensor aNew(symm(a + dt*k2));

    // tr(a) = 1 is an invariant of the Folgar-Tucker equation; the
    // normalisation removes the small RK2 drift
    const scalar trA(tr(aNew));

    if (trA > small)
    {
        aNew /= trA;
    }

    return aNew;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
