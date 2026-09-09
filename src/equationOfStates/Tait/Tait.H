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
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    moldingFoam is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with moldingFoam.  If not, see <http://www.gnu.org/licenses/>.

Class
    Foam::Tait

Description
    Two-domain Tait equation of state for polymer melts and solids:

    \f[
        \hat{v} = v_0(T) \left(1 - C \ln\left(1 + \frac{p}{B(T)}\right)\right)
    \f]
    \f[
        v_0(T) = \left\{
            \begin{array}{cc}
                b_{1m} + b_{2m} (T - T_t) & T > T_t \quad \text{(melt)} \\
                b_{1s} + b_{2s} (T - T_t) & T \leq T_t \quad \text{(solid)}
            \end{array}
        \right.
    \f]
    \f[
        B(T) = b_3 \exp(-b_4 T), \qquad T_t = b_5 + b_6 p
    \f]

    where \f$ \hat{v} \f$ is the specific volume and
    \f$ \rho = 1/\hat{v} \f$.

    All of \f$ \rho \f$, \f$ \psi = \partial\rho/\partial p|_T \f$,
    \f$ \alpha_v = -(1/\rho)\partial\rho/\partial T|_p \f$ and
    \f$ C_p - C_v \f$ are evaluated analytically; the pressure equation and
    the energy equation depend on them.

    Across the solidification temperature \f$ T_t \f$ the melt and solid
    branches are blended with a cubic smoothstep over a band of width
    \f$ \pm \f$ \c smoothBand about \f$ T_t \f$ (default 0.5 K), which keeps
    the first derivatives continuous and avoids convergence chatter during
    packing.

    \c b2m (melt slope) defaults to 0 so that, with only the required keys,
    the melt branch reduces to \f$ v_0 = b_{1m} \f$. \c b3s and \c b4s
    (solid B(T) coefficients) default to \c b3 and \c b4.

    Coefficients are read from the \c equationOfState sub-dictionary of the
    \c mixture entry of the thermophysical properties dictionary. All
    coefficients are SI:

    - \c b1m   [m^3/kg]  specific volume of the melt at \f$ T_t \f$
    - \c b2m   [m^3/kg/K] melt slope of \f$ v_0 \f$ w.r.t. \f$ T - T_t \f$
    - \c b1s   [m^3/kg]  solid specific volume parameter
    - \c b2s   [m^3/kg/K] solid slope of \f$ v_0 \f$
    - \c b3    [Pa]      \f$ B(T) \f$ prefactor (melt)
    - \c b4    [1/K]     \f$ B(T) \f$ temperature sensitivity (melt)
    - \c b3s   [Pa]      \f$ B(T) \f$ prefactor (solid, default = b3)
    - \c b4s   [1/K]     \f$ B(T) \f$ sensitivity (solid, default = b4)
    - \c b5    [K]       \f$ T_t \f$ at atmospheric pressure
    - \c b6    [K/Pa]    pressure sensitivity of \f$ T_t \f$
    - \c C     [-]       Tait constant (default 0.0894)
    - \c smoothBand [K]  half-width of the solid/melt blending band
                         (default 0.5)

    Example specification:
    \verbatim
        mixture
        {
            specie
            {
                molWeight   1;
            }
            equationOfState
            {
                b1m         1.2495299e-3;
                b2m         1.026e-6;
                b1s         1.070046e-3;
                b2s         2.077e-7;
                b3          1.042149833644e8;
                b4          0.004941;
                b5          390.65;
                b6          1.543e-7;
            }
            thermodynamics
            {
                Cp          2400;
                hf          0;
            }
            transport
            {
                mu          100;
                Pr          5;
            }
        }
    \endverbatim

SourceFiles
    TaitI.H
    Tait.C

See also
    Foam::perfectGas
    Foam::rhoConst

\*---------------------------------------------------------------------------*/

#ifndef Tait_H
#define Tait_H

#include "autoPtr.H"
#include "specie.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

template<class Specie> class Tait;

template<class Specie>
inline Tait<Specie> operator+
(
    const Tait<Specie>&,
    const Tait<Specie>&
);

template<class Specie>
inline Tait<Specie> operator*
(
    const scalar,
    const Tait<Specie>&
);

template<class Specie>
inline Tait<Specie> operator==
(
    const Tait<Specie>&,
    const Tait<Specie>&
);

template<class Specie>
Ostream& operator<<
(
    Ostream&,
    const Tait<Specie>&
);

/*---------------------------------------------------------------------------*\
                             Class Tait Declaration
\*---------------------------------------------------------------------------*/

template<class Specie>
class Tait
:
    public Specie
{
    // Private Data

        //- Melt specific volume at Tt [m^3/kg]
        scalar b1m_;

        //- Melt slope of v0 w.r.t. (T - Tt) [m^3/kg/K]
        scalar b2m_;

        //- Solid specific volume parameter [m^3/kg]
        scalar b1s_;

        //- Solid slope of v0 w.r.t. (T - Tt) [m^3/kg/K]
        scalar b2s_;

        //- B(T) prefactor, melt [Pa]
        scalar b3m_;

        //- B(T) temperature sensitivity, melt [1/K]
        scalar b4m_;

        //- B(T) prefactor, solid [Pa]
        scalar b3s_;

        //- B(T) temperature sensitivity, solid [1/K]
        scalar b4s_;

        //- Solidification temperature at atmospheric pressure [K]
        scalar b5_;

        //- Pressure sensitivity of the solidification temperature [K/Pa]
        scalar b6_;

        //- Tait constant [-]
        scalar C_;

        //- Half-width of the solid/melt blending band [K]
        scalar band_;


    // Private Member Functions

        //- Solidification temperature at pressure p [K]
        inline scalar Tt(scalar p) const;

        //- Zero-pressure-offset specific volume of the melt branch [m^3/kg]
        inline scalar v0m(scalar p, scalar T) const;

        //- Zero-pressure-offset specific volume of the solid branch
        //  [m^3/kg]
        inline scalar v0s(scalar p, scalar T) const;

        //- B(T) of the melt branch [Pa]
        inline scalar Bm(scalar T) const;

        //- B(T) of the solid branch [Pa]
        inline scalar Bs(scalar T) const;

        //- Tait logarithmic compression factor of the melt branch [-]
        inline scalar fm(scalar p, scalar T) const;

        //- Tait logarithmic compression factor of the solid branch [-]
        inline scalar fs(scalar p, scalar T) const;

        //- Specific volume of the melt branch [m^3/kg]
        inline scalar vhatm(scalar p, scalar T) const;

        //- Specific volume of the solid branch [m^3/kg]
        inline scalar vhats(scalar p, scalar T) const;

        //- Normalised band coordinate in [0, 1]; 0 fully solid, 1 fully
        //  melted [-]
        inline scalar meltX(scalar p, scalar T) const;

        //- Melt weight in [0, 1] returned by the C1 smoothstep blending
        //  across the solidification temperature [-]
        inline scalar meltWeight(scalar p, scalar T) const;

        //- Derivative of the smoothstep weight w.r.t. the normalised band
        //  coordinate; vanishes at the band edges [-]
        inline scalar meltWeightPrime(scalar p, scalar T) const;

        //- Volumetric thermal expansion of the melt branch [1/K]
        inline scalar alphavm(scalar p, scalar T) const;

        //- Volumetric thermal expansion of the solid branch [1/K]
        inline scalar alphavs(scalar p, scalar T) const;

        //- d vhat/dp|T of the blended specific volume [m^3/kg/Pa]
        inline scalar vhatPrimeP(scalar p, scalar T) const;

        //- d vhat/dT|p of the blended specific volume [m^3/kg/K]
        inline scalar vhatPrimeT(scalar p, scalar T) const;


public:

    //- Runtime type information (the name used to select the equation of
    //  state in the thermoType dictionary entry)
    static word typeName()
    {
        return "Tait<" + word(Specie::typeName_()) + '>';
    }

        //- Is the equation of state incompressible i.e. is the density
        //  independent of pressure?
        static const bool incompressible = false;

        //- Is the equation of state isochoric i.e. is the density
        //  independent of temperature?
        static const bool isochoric = false;


    // Constructors

        //- Construct from components
        inline Tait
        (
            const Specie& sp,
            scalar b1m,
            scalar b2m,
            scalar b1s,
            scalar b2s,
            scalar b3m,
            scalar b4m,
            scalar b3s,
            scalar b4s,
            scalar b5,
            scalar b6,
            scalar C,
            scalar band
        );

        //- Construct from dictionary (reads the equationOfState sub-dict)
        Tait(const word& name, const dictionary& dict);

        //- Construct as copy
        inline Tait(const word& name, const Tait&);

        //- Construct and return a clone
        inline autoPtr<Tait> clone() const;


    // Member Functions

        //- Return the specific volume [m^3/kg]
        inline scalar vhat(scalar p, scalar T) const;

        //- Return density [kg/m^3]
        inline scalar rho(scalar p, scalar T) const;

        //- Return enthalpy contribution to the equation of state [J/kg]
        inline scalar h(scalar p, scalar T) const;

        //- Return Cp contribution to the equation of state [J/kg/K]
        inline scalar Cp(scalar p, scalar T) const;

        //- Return internal energy contribution to the equation of state
        //  [J/kg]
        inline scalar e(scalar p, scalar T) const;

        //- Return Cv contribution to the equation of state [J/kg/K]
        inline scalar Cv(scalar p, scalar T) const;

        //- Return entropy contribution to the equation of state derivative
        //  w.r.t. pressure [J/kg/K/Pa]
        inline scalar sp(scalar p, scalar T) const;

        //- Return entropy contribution to the equation of state derivative
        //  w.r.t. specific volume [J/K/m^3]
        inline scalar sv(scalar p, scalar T) const;

        //- Return the compressibility psi = drho/dp|T [kg/m^3/Pa]
        inline scalar psi(scalar p, scalar T) const;

        //- Return the compression factor [-]
        inline scalar Z(scalar p, scalar T) const;

        //- Return (Cp - Cv) [J/kg/K]
        inline scalar CpMCv(scalar p, scalar T) const;

        //- Return the volumetric coefficient of thermal expansion
        //  alphav = (1/vhat) d vhat/dT|p [1/K]
        inline scalar alphav(scalar p, scalar T) const;

        //- Write to Ostream
        void write(Ostream& os) const;


    // Member Operators

        inline void operator+=(const Tait&);
        inline void operator*=(const scalar);


    // Friend Operators

        friend Tait operator+ <Specie>
        (
            const Tait&,
            const Tait&
        );

        friend Tait operator* <Specie>
        (
            const scalar s,
            const Tait&
        );

        friend Tait operator== <Specie>
        (
            const Tait&,
            const Tait&
        );

        friend Ostream& operator<< <Specie>
        (
            Ostream&,
            const Tait&
        );
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#include "TaitI.H"

#ifdef NoRepository
    #include "Tait.C"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
