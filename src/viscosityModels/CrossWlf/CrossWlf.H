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
    Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf

Description
    Cross-WLF generalised Newtonian viscosity model for thermoplastic
    polymer melts, registered in the strain-rate dependent generalised
    Newtonian viscosity model family and selected with

    \verbatim
        viscosityModel  CrossWlf;
    \endverbatim

    within the \c laminar sub-dictionary of \c constant/momentumTransport
    (with \c model \c generalisedNewtonian).

    The dynamic viscosity is

    \f[
        \eta = \frac{\eta_0}{1 + \left(\frac{\eta_0 \dot{\gamma}}
               {\tau^*}\right)^{1-n}}
    \f]
    \f[
        \eta_0 = D_1 \exp\left(-\frac{A_1 (T - T^*)}{A_2 + (T - T^*)}\right),
        \qquad T^* = D_2 + D_3 p
    \f]

    Numerical safeguards:
    - the strain rate is floored at \c gammaDotMin (default 1e-6 1/s);
    - the exponent argument is capped so that \f$ \eta_0 \leq \etaMax \f$
      which prevents divergence of the exponential in the frozen region
      \f$ T \lesssim T^* \f$;
    - \f$ \eta \f$ is clamped to the configurable range
      \f$ [\eta_{min}, \eta_{max}] \f$.

    The model returns the kinematic viscosity \f$ \nu = \eta/\rho \f$. The
    temperature, pressure and density fields are looked up in the mesh
    object registry by the names \c T, \c p and \c rho; a missing field is
    a fatal error.

    Coefficients are read from the \c CrossWlfCoeffs sub-dictionary (all
    SI):

    - \c n           [-]       Cross law index
    - \c tauStar     [Pa]      Critical stress level at the onset of shear
                               thinning
    - \c D1          [Pa s]    Zero pressure, zero temperature offset
                               viscosity
    - \c D2          [K]       Temperature offset of the WLF part
    - \c D3          [K/Pa]    Pressure sensitivity of the temperature
                               offset
    - \c A1          [-]       WLF slope
    - \c A2          [K]       WLF shift
    - \c etaMin      [Pa s]    Lower viscosity clamp
    - \c etaMax      [Pa s]    Upper viscosity clamp
    - \c gammaDotMin [1/s]     Strain-rate floor (optional, default 1e-6)

    Example specification:
    \verbatim
        laminar
        {
            model           generalisedNewtonian;

            viscosityModel  CrossWlf;

            CrossWlfCoeffs
            {
                n           0.393539;
                tauStar     64568.9;
                D1          3.76174e15;
                D2          153.15;
                D3          0;
                A1          33.21;
                A2          51.6;
                etaMin      5;
                etaMax      1e6;
            }
        }
    \endverbatim

SourceFiles
    CrossWlf.C

See also
    Foam::laminarModels::generalisedNewtonianViscosityModels::CrossPowerLaw
    Foam::laminarModels::generalisedNewtonianViscosityModels::BirdCarreau

\*---------------------------------------------------------------------------*/

#ifndef CrossWlf_H
#define CrossWlf_H

#include "strainRateViscosityModel.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace laminarModels
{
namespace generalisedNewtonianViscosityModels
{

/*---------------------------------------------------------------------------*
                        Class CrossWlf Declaration
\*---------------------------------------------------------------------------*/

class CrossWlf
:
    public strainRateViscosityModel
{
public:

    //- Coefficients of the Cross-WLF model [SI]
    struct coeffs
    {
        //- Cross law index [-]
        scalar n;

        //- Critical stress level at the onset of shear thinning [Pa]
        scalar tauStar;

        //- Zero pressure, zero temperature offset viscosity [Pa s]
        scalar D1;

        //- Temperature offset of the WLF part [K]
        scalar D2;

        //- Pressure sensitivity of the temperature offset [K/Pa]
        scalar D3;

        //- WLF slope [-]
        scalar A1;

        //- WLF shift [K]
        scalar A2;

        //- Lower viscosity clamp [Pa s]
        scalar etaMin;

        //- Upper viscosity clamp [Pa s]
        scalar etaMax;

        //- Strain-rate floor [1/s]
        scalar gammaDotMin;
    };


    //- Runtime type information
    TypeName("CrossWlf");


    // Constructors

        //- Construct from components
        CrossWlf
        (
            const dictionary& viscosityProperties,
            const viscosity& viscosity,
            const volVectorField& U
        );

        //- Disallow default bitwise copy construction
        CrossWlf(const CrossWlf&) = delete;


    //- Destructor
    virtual ~CrossWlf() = default;


    // Member Functions

        //- Read the coefficients from the CrossWlfCoeffs sub-dictionary
        virtual bool read(const dictionary& viscosityProperties);

        //- Return the kinematic viscosity [m^2/s]
        virtual tmp<volScalarField> nu
        (
            const volScalarField& nu0,
            const volScalarField& strainRate
        ) const;

        //- Return the dynamic viscosity [Pa s] computed from a
        //  coefficients dictionary; used by the model tests
        static scalar eta
        (
            const dictionary& coeffsDict,
            scalar p,
            scalar T,
            scalar gammaDot
        );


    // Member Operators

        //- Disallow default bitwise assignment
        void operator=(const CrossWlf&) = delete;


private:

    // Private Data

        //- Cross-WLF coefficients [SI]
        coeffs coeffs_;


    // Private Member Functions

        //- Read the coefficients from a CrossWlfCoeffs-like dictionary
        static coeffs readCoeffs(const dictionary& coeffsDict);

        //- Return the dynamic viscosity [Pa s]; implements all the
        //  numerical safeguards in a single place
        static scalar etaValue
        (
            const coeffs& c,
            scalar p,
            scalar T,
            scalar gammaDot
        );
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace generalisedNewtonianViscosityModels
} // End namespace laminarModels
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
