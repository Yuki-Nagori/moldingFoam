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
    Foam::moldingWarpage

Description
    Analytical free-warpage (thermal bending) model from a through-thickness
    temperature profile (stage one of task 022).

    For an unconstrained linear-elastic plate with a temperature profile
    T(y) through the thickness h the free membrane strain and curvature are

    \verbatim
        eps0 = alpha/h  integral (T - Tref) dy
        kappa = alpha 12/h^3 integral (T - Tref) (y - h/2) dy
    \endverbatim

    which reduce to the classical results for the usual profiles:
    - a linear profile dT(y): eps0 = alpha mean(dT) and
      kappa = alpha (Ttop - Tbot)/h;
    - a parabolic profile c (y - h/2)^2: kappa = alpha c;
    - a uniform profile: kappa = 0.

    The free deflection of a strip of length L follows as delta = kappa L^2/2
    and the residual stress of a fully constrained plate is
    sigma = E/(1 - nu) alpha (Tref - T).

    The profile is integrated with the trapezoidal rule over uniformly
    spaced cell centres, which is exact for the linear and parabolic
    profiles used in the model tests.

SourceFiles
    moldingWarpage.C

\*---------------------------------------------------------------------------*/

#ifndef moldingWarpage_H
#define moldingWarpage_H

#include "dictionary.H"
#include "scalar.H"
#include "UList.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                    Class moldingWarpage Declaration
\*---------------------------------------------------------------------------*/

class moldingWarpage
{
    // Private Data

        //- Linear thermal expansion coefficient [1/K]
        scalar alpha_;

        //- Elastic modulus [Pa]
        scalar E_;

        //- Poisson ratio [-]
        scalar nu_;

        //- Reference (stress-free) temperature [K]
        scalar Tref_;


public:

    // Constructors

        //- Construct from a dictionary
        moldingWarpage(const dictionary& dict);


    // Member Functions

        //- Free membrane strain from a through-thickness profile
        scalar freeStrain(const UList<scalar>& T, const scalar h) const;

        //- Free curvature from a through-thickness profile [1/m]
        scalar freeCurvature(const UList<scalar>& T, const scalar h) const;

        //- Free deflection of a strip of length L [m]
        scalar deflection(const scalar kappa, const scalar L) const;

        //- Timoshenko bimetal curvature of two bonded layers with
        //  different expansion coefficients [1/m]
        static scalar bimetalCurvature
        (
            const scalar h1,      //- Layer 1 thickness [m]
            const scalar h2,      //- Layer 2 thickness [m]
            const scalar E1,      //- Layer 1 modulus [Pa]
            const scalar E2,      //- Layer 2 modulus [Pa]
            const scalar alpha1,  //- Layer 1 expansion [1/K]
            const scalar alpha2,  //- Layer 2 expansion [1/K]
            const scalar dT       //- Temperature change [K]
        );

        //- Out-of-plane deflection profile of a simply supported strip
        //  from a curvature distribution: w'' = -kappa with w(0) = w(L) = 0
        //  (the curvature and deflection samples are uniformly spaced)
        void deflectionProfile
        (
            const UList<scalar>& kappa,
            const scalar L,
            UList<scalar>& w
        ) const;

        //- Residual stress of a fully constrained plate [Pa]
        scalar constrainedStress(const scalar T) const;

        //- Self-equilibrated through-thickness residual stress of a plate
        //  constrained in-plane: sigma(y) = E/(1-nu) alpha ((T-Tref) - eps0)
        //  with eps0 = alpha mean(T-Tref). The profile is integrated with
        //  Simpson per cell (exact for linear profiles)
        void residualStress
        (
            const UList<scalar>& T,
            const scalar h,
            UList<scalar>& sigma
        ) const;

        //- Linear thermal expansion coefficient [1/K]
        scalar alpha() const
        {
            return alpha_;
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
