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
    Foam::moldingViscoelastic

Description
    Single-mode viscoelastic constitutive model for polymer melts (stage
    one of task 027).

    The extra stress tau evolves with the upper-convected Maxwell (UCM)
    derivative, optionally with the Giesekus quadratic mobility term

    \verbatim
        lambda Dtau/Dt = lambda (L tau + tau L^T)
                       + 2 eta0 D
                       + (lambda alpha/eta0) tau.tau
                       - tau
    \endverbatim

    where L is the velocity gradient, D = symm(L) and alpha is the
    mobility factor (alpha = 0 is the UCM limit). The model is local
    (homogeneous flow); advection and the momentum coupling are the next
    stage of task 027.

    UCM analytic limits reproduced by the model tests:
    - start-up shear tau_xy = eta0 gammadot (1 - exp(-t/lambda));
    - relaxation from a steady state tau_xy = tau0 exp(-t/lambda);
    - steady first normal stress difference N1 = 2 eta0 lambda gammadot^2.

SourceFiles
    moldingViscoelastic.C

\*---------------------------------------------------------------------------*/

#ifndef moldingViscoelastic_H
#define moldingViscoelastic_H

#include "dictionary.H"
#include "symmTensor.H"
#include "tensor.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                 Class moldingViscoelastic Declaration
\*---------------------------------------------------------------------------*/

class moldingViscoelastic
{
    // Private Data

        //- Relaxation time [s]
        scalar lambda_;

        //- Zero-shear viscosity [Pa s]
        scalar eta0_;

        //- Giesekus mobility factor [-]
        scalar alpha_;


public:

    // Constructors

        //- Construct from a dictionary
        moldingViscoelastic(const dictionary& dict);


    // Member Functions

        //- Extra stress evolution Dtau/Dt [Pa/s]
        symmTensor dtauDt
        (
            const symmTensor& tau,
            const tensor& L
        ) const;

        //- Advance the extra stress over dt (RK2)
        symmTensor advance
        (
            const symmTensor& tau,
            const tensor& L,
            const scalar dt
        ) const;

        //- Relaxation time [s]
        scalar lambda() const
        {
            return lambda_;
        }

        //- Zero-shear viscosity [Pa s]
        scalar eta0() const
        {
            return eta0_;
        }

        //- Mobility factor [-]
        scalar alpha() const
        {
            return alpha_;
        }
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
