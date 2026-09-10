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

InClass
    Foam::hMeltThermo

Description
    Constant sensible heat capacity thermodynamics with a latent-heat
    peak at the solidification temperature of the Tait equation of state.

    The sensible enthalpy is

    \verbatim
        hs = Cp*(T - Tref) + latentHeat*(meltWeight(T) - meltWeight(Tref))
    \endverbatim

    where \c meltWeight is the C1 smoothstep melt weight of the Tait
    blend, which runs from 0 to 1 across the transition band and has
    unit-integral derivative, so crossing the band releases exactly
    \c latentHeat. The sensible enthalpy consequently develops the
    physically correct plateau while crossing the solidification
    temperature, which is what bounds the cooling rate of stage M3.

    The latent-heat peak (d(hs)/dT - Cp) is exposed through
    \c latentCp(p, T) and is folded into the apparent \c Cv(p, T)
    used by the energy equation, while \c Cp(p, T) keeps only the
    sensible value. This split is deliberate: the thermal conductivity
    of constTransport is \c kappa = Cp*mu/Pr, and feeding the narrow
    latent peak into \c Cp would make kappa spike by two orders of
    magnitude across the 1 K transition band, wrecking the energy
    predictor.

    The latent-heat peak is derived from the equation of state itself,
    so this class is only instantiable with Foam::Tait (the default
    \c latentHeat of 0 reproduces hConstThermo exactly).

Usage
    \table
        Property     | Description
        Cp           | Base heat capacity at constant pressure [J/kg/K]
        hf           | Heat of formation [J/kg]
        latentHeat   | Latent heat of solidification [J/kg] (default 0)
        Tref         | Reference temperature [K] (defaults to Tstd)
        hsRef        | Reference sensible enthalpy [J/kg] (defaults to 0)
    \endtable

SourceFiles
    hMeltThermoI.H
    hMeltThermo.C

\*---------------------------------------------------------------------------*/

#ifndef hMeltThermo_H
#define hMeltThermo_H

#include "scalar.H"
#include "dictionary.H"
#include "autoPtr.H"
#include "Ostream.H"
#include "thermodynamicConstants.H"
#include "IOstreams.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// Forward declaration of friend functions and operators

template<class EquationOfState> class hMeltThermo;

template<class EquationOfState>
inline hMeltThermo<EquationOfState> operator+
(
    const hMeltThermo<EquationOfState>&,
    const hMeltThermo<EquationOfState>&
);

template<class EquationOfState>
inline hMeltThermo<EquationOfState> operator*
(
    const scalar,
    const hMeltThermo<EquationOfState>&
);

template<class EquationOfState>
inline hMeltThermo<EquationOfState> operator==
(
    const hMeltThermo<EquationOfState>&,
    const hMeltThermo<EquationOfState>&
);

template<class EquationOfState>
Ostream& operator<<
(
    Ostream&,
    const hMeltThermo<EquationOfState>&
);


/*---------------------------------------------------------------------------*\
                         Class hMeltThermo Declaration
\*---------------------------------------------------------------------------*/

template<class EquationOfState>
class hMeltThermo
:
    public EquationOfState
{
    // Private Data

        //- Base heat capacity at constant pressure [J/kg/K]
        scalar Cp_;

        //- Heat of formation [J/kg]
        scalar hf_;

        //- Latent heat of solidification [J/kg]
        scalar latentHeat_;

        //- Reference temperature around which to linearise [K]
        scalar Tref_;

        //- Reference sensible enthalpy around which to linearise [J/kg]
        scalar hsRef_;


public:

    // Constructors

        //- Construct from components
        inline hMeltThermo
        (
            const EquationOfState& st,
            const scalar Cp,
            const scalar hf,
            const scalar latentHeat,
            const scalar Tref,
            const scalar hsRef
        );

        //- Construct from name and dictionary
        hMeltThermo(const word& name, const dictionary& dict);

        //- Construct as copy with a new name
        inline hMeltThermo(const word& name, const hMeltThermo&);

        //- Construct and return a clone
        inline autoPtr<hMeltThermo> clone() const;


    // Member Functions

        //- Return the instantiated type name
        static word typeName()
        {
            return "hMelt<" + EquationOfState::typeName() + '>';
        }

        //- Limit the temperature (no limiting)
        inline scalar limit(const scalar T) const;

        //- Return the sensible heat capacity at constant pressure
        //  [J/kg/K]; excludes the latent-heat peak so that transport
        //  properties (kappa = Cp*mu/Pr) stay smooth
        inline scalar Cp(const scalar p, const scalar T) const;

        //- Return the latent-heat contribution to the apparent heat
        //  capacity [J/kg/K]: d(hs)/dT - Cp, a peaked, non-negative
        //  function of unit integral across the transition band
        inline scalar latentCp(const scalar p, const scalar T) const;

        //- Return sensible enthalpy [J/kg]
        inline scalar hs(const scalar p, const scalar T) const;

        //- Return absolute enthalpy [J/kg]
        inline scalar ha(const scalar p, const scalar T) const;

        //- Return heat of formation [J/kg]
        inline scalar hf() const;

        //- Return sensible internal energy [J/kg]
        inline scalar es(const scalar p, const scalar T) const;

        //- Return absolute internal energy [J/kg]
        inline scalar ea(const scalar p, const scalar T) const;

        //- Return the apparent heat capacity at constant volume
        //  [J/kg/K]: base Cp + latentCp - CpMCv, the energy-equation
        //  coefficient (positive across the transition band)
        inline scalar Cv(const scalar p, const scalar T) const;

        //- Return entropy [J/kg/K]; the latent contribution is treated
        //  to first order around Tstd (entropy is not used by the solver)
        inline scalar s(const scalar p, const scalar T) const;

        //- Return Gibbs free energy change relative to Tstd [J/kg]
        inline scalar gStd(const scalar T) const;

        //- Return the temperature derivative of heat capacity
        //  [J/kg/K^2]; zero, as in upstream hConstThermo (the Newton
        //  temperature inversions use Cp directly)
        inline scalar dCpdT(const scalar p, const scalar T) const;

        //- Write to Ostream
        void write(Ostream& os) const;


    // Member Operators

        //- Mass-fraction-weighted blend with another hMeltThermo
        inline void operator+=(const hMeltThermo&);


    // Friend Operators

        friend hMeltThermo operator+ <EquationOfState>
        (
            const hMeltThermo&,
            const hMeltThermo&
        );

        friend hMeltThermo operator* <EquationOfState>
        (
            const scalar,
            const hMeltThermo&
        );

        friend hMeltThermo operator== <EquationOfState>
        (
            const hMeltThermo&,
            const hMeltThermo&
        );

        friend Ostream& operator<< <EquationOfState>
        (
            Ostream&,
            const hMeltThermo&
        );
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#include "hMeltThermoI.H"

#ifdef NoRepository
    #include "hMeltThermo.C"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
