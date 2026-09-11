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
    Foam::moldingRunnerNetwork

Description
    One-dimensional pressure-flow-thermal model of a runner system, used
    to couple the machine nozzle to the cavity gates.

    The network is a series feed (nozzle to the runner node) followed by
    parallel gate branches named in a \c gates sub-dictionary. Each
    segment is a circular tube whose pressure drop follows the
    generalised Hagen-Poiseuille relation

    \verbatim
        dp = 128 eta L Q/(pi D^4),  eta = eta(gammaDot, T)
        gammaDot = 32 Q/(pi D^3)
    \endverbatim

    with the melt viscosity from a constant, power-law or CrossWlf model.
    The melt temperature evolves along the segment by the steady 1D energy
    balance with an optional wall conductance,

    \verbatim
        Tout = Twall + (Tin - Twall) exp(-htc pi D L/(mdot cp))
    \endverbatim

    For parallel branches the split follows the equal pressure drop
    condition and is solved by a fixed-point iteration on the branch
    resistances; for a constant viscosity it reduces to the exact
    Hagen-Poiseuille split and for a common power-law index to the
    analytic resistance ratio Qi/Qj = (Dj/Di)^(3 + 1/n).

SourceFiles
    moldingRunnerNetwork.C

\*---------------------------------------------------------------------------*/

#ifndef moldingRunnerNetwork_H
#define moldingRunnerNetwork_H

#include "CrossWlf.H"
#include "dictionary.H"
#include "scalar.H"
#include "scalarList.H"
#include "word.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                  Class moldingRunnerNetwork Declaration
\*---------------------------------------------------------------------------*/

class moldingRunnerNetwork
{
public:

    //- Viscosity model
    enum class viscosityType
    {
        constant,
        powerLaw,
        crossWlf
    };

    //- One circular tube segment
    struct segment
    {
        word name;
        scalar length;
        scalar diameter;
        scalar wallTemperature;
        scalar htc;
    };


private:

    // Private Data

        //- Melt temperature at the machine nozzle [K]
        scalar inletTemperature_;

        //- Melt specific heat [J/kg/K]
        scalar cp_;

        //- Melt density [kg/m^3]
        scalar rho_;

        //- Viscosity model
        viscosityType viscosityType_;

        //- Constant viscosity [Pa s]
        scalar mu_;

        //- Power-law consistency index [Pa s^n]
        scalar K_;

        //- Power-law index [-]
        scalar n_;

        //- CrossWlf coefficients (when viscosityType_ == crossWlf)
        laminarModels::generalisedNewtonianViscosityModels::CrossWlf::
        coeffs crossWlfCoeffs_;

        //- Series feed segment
        segment feed_;

        //- Parallel gate branches
        List<segment> gates_;


    // Private Member Functions

        //- Melt viscosity at a wall shear rate and temperature [Pa s]
        scalar eta(const scalar gammaDot, const scalar T) const;

        //- Melt temperature after a segment
        scalar segmentTemperature
        (
            const segment& s,
            const scalar Tin,
            const scalar Q
        ) const;

        //- Equal-pressure-drop flow split of the parallel branches;
        //  returns the branch pressure drop and fills the branch flows
        scalar split
        (
            const scalar Q,
            const scalar Tnode,
            scalarList& gateQ
        ) const;


public:

    // Constructors

        //- Construct from a dictionary
        moldingRunnerNetwork(const dictionary& dict);


    // Static Member Functions

        //- Hagen-Poiseuille pressure drop of a circular tube [Pa]
        static scalar hagenPoiseuille
        (
            const scalar mu,
            const scalar L,
            const scalar D,
            const scalar Q
        );

        //- Wall shear rate of a circular tube 32Q/(pi D^3) [1/s]
        static scalar shearRate(const scalar Q, const scalar D);


    // Member Functions

        //- Total network pressure drop at the total flow rate [Pa]
        scalar pressureDrop(const scalar Q) const;

        //- Volumetric flow rate through gate g [m^3/s]
        scalar gateFlow(const label g, const scalar Q) const;

        //- Melt temperature at gate g [K]
        scalar gateTemperature(const label g, const scalar Q) const;

        //- Number of gates
        label nGates() const
        {
            return gates_.size();
        }

        //- Feed segment
        const segment& feed() const
        {
            return feed_;
        }

        //- Gate segment
        const segment& gate(const label g) const
        {
            return gates_[g];
        }
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
