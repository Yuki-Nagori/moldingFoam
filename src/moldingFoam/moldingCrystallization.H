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
    Foam::moldingCrystallization

Description
    Nakamura/Avrami crystallisation kinetics for semi-crystalline
    polymers, used to release the latent heat through a relative
    crystallinity field instead of the fixed hMelt latent-Cp platform.

    The relative crystallinity chi evolves with the Nakamura differential
    form of the Avrami equation,

    \verbatim
        dchi/dt = n K(T,p) (1 - chi) (-ln(1 - chi))^((n-1)/n)
    \endverbatim

    where the rate constant follows a Gaussian temperature window

    \verbatim
        K(T,p) = Kmax exp(-4 ln2 (T - Tmax(p))^2/W^2)
        Tmax(p) = Tmax0 + dTdp p
    \endverbatim

    so that K(Tmax) = Kmax and K(Tmax +/- W/2) = Kmax/2, with an optional
    pressure shift of the window.

    For a constant K the equation has the exact Avrami solution
    chi = 1 - exp(-(K t)^n). Writing the current state as the equivalent
    time xi = (-ln(1 - chi))^(1/n), one step of size dt is therefore
    integrated exactly and unconditionally bounded in [0, 1] by

    \verbatim
        chi_new = 1 - exp(-(xi + K dt)^n)
    \endverbatim

    The latent heat release rate rho L dchi/dt is returned for the energy
    equation source.

SourceFiles
    moldingCrystallization.C

\*---------------------------------------------------------------------------*/

#ifndef moldingCrystallization_H
#define moldingCrystallization_H

#include "dictionary.H"
#include "scalar.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                Class moldingCrystallization Declaration
\*---------------------------------------------------------------------------*/

class moldingCrystallization
{
    // Private Data

        //- Avrami exponent [-]
        scalar n_;

        //- Peak rate constant [1/s]
        scalar Kmax_;

        //- Peak temperature at atmospheric pressure [K]
        scalar Tmax_;

        //- Full width at half maximum of the rate window [K]
        scalar width_;

        //- Peak-temperature pressure shift [K/Pa]
        scalar dTdp_;

        //- Latent heat of crystallisation [J/kg]
        scalar latentHeat_;

        //- Melt density [kg/m^3]
        scalar rho_;


public:

    // Constructors

        //- Construct from a dictionary
        moldingCrystallization(const dictionary& dict);


    // Member Functions

        //- Peak temperature at pressure p [K]
        scalar Tmax(const scalar p) const
        {
            return Tmax_ + dTdp_*p;
        }

        //- Nakamura rate constant at T and p [1/s]
        scalar K(const scalar T, const scalar p) const;

        //- Crystallinity rate dchi/dt [1/s]
        scalar dchiDt(const scalar chi, const scalar T, const scalar p) const;

        //- Exact one-step advance of chi, unconditionally bounded
        scalar advance
        (
            const scalar chi,
            const scalar T,
            const scalar p,
            const scalar dt
        ) const;

        //- Latent heat release rate [W/m^3] (positive heats the melt)
        scalar latentSource
        (
            const scalar chi,
            const scalar T,
            const scalar p,
            const scalar dt
        ) const;

        //- Latent heat of crystallisation [J/kg]
        scalar latentHeat() const
        {
            return latentHeat_;
        }

        //- Avrami exponent
        scalar n() const
        {
            return n_;
        }
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
