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
    either a flat set of parallel gate branches named in a \c gates
    sub-dictionary, or an arbitrary tree of runner nodes given in a \c tree
    sub-dictionary (task 058). Each segment is a circular tube whose
    pressure drop follows the generalised Hagen-Poiseuille relation

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

    In tree mode the same equal-pressure-drop condition is applied at every
    node: the flow a node receives is redistributed over its children in
    inverse proportion to their equivalent resistances (own segment in
    series with the parallel combination of the subtree), refreshed
    top-down while the equivalent resistances are rebuilt bottom-up. That
    is a single level of iteration over the whole tree - deliberately not a
    nested solve per level, which is what made the first attempt blow up
    (see ai-docs/tasks/058). The gates are the tree leaves, numbered in
    dictionary order and addressed by name in the boundaries.

SourceFiles
    moldingRunnerNetwork.C

\*---------------------------------------------------------------------------*/

#ifndef moldingRunnerNetwork_H
#define moldingRunnerNetwork_H

#include "CrossWlf.H"
#include "dictionary.H"
#include "labelList.H"
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

        //- Parallel gate branches (flat network), or the tree leaves (tree
        //  network) - either way the gates addressed by the boundaries
        List<segment> gates_;

        //- The \c tree sub-dictionary was supplied
        bool treeMode_;

        //- Tree node segments (empty unless treeMode_)
        List<segment> treeSeg_;

        //- Tree node parent index (-1: attached to the feed)
        labelList treeParent_;

        //- Tree node children indices
        List<labelList> treeChildren_;

        //- Nodes attached to the feed (depth 0)
        labelList treeRoots_;

        //- Node indices ordered parents before children
        labelList treeOrder_;

        //- Leaf node indices: the gates, in dictionary order
        labelList treeLeaves_;

        //- Gate index of each tree node (-1 for nodes that are not leaves)
        labelList treeLeafOf_;

        //- Gate opening time [s], one per gate
        scalarList gateOpenTime_;

        //- Gate closing time [s], one per gate
        scalarList gateCloseTime_;

        //- Any gate carries valve timing; false keeps both the flat split
        //  and the tree solve bit-for-bit unchanged
        bool anyValveTiming_;

        //- Fixed-point budget of one tree solve: the work is
        //  O(maxTreeIterations_ * nodes) with no nesting, so this bounds
        //  the cost of every boundary update
        static const label maxTreeIterations_;


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

        //- Hydraulic resistance of one segment at a flow and inlet
        //  temperature, dp/Q [Pa s/m^3]
        scalar segmentResistance
        (
            const segment& s,
            const scalar Q,
            const scalar T
        ) const;

        //- Pressure drop of the feed segment at the total flow rate [Pa]
        scalar feedPressureDrop(const scalar Q) const;

        //- Is gate g open at time t?
        bool gateOpen(const label g, const scalar t) const;

        //- Equal-pressure-drop flow split of the parallel branches;
        //  returns the branch pressure drop and fills the branch flows
        scalar split
        (
            const scalar Q,
            const scalar Tnode,
            scalarList& gateQ
        ) const;

        //- As split(), but over the open branches only: a closed gate
        //  carries no flow and the remaining branches share the total.
        //  Kept separate from split() so that cases without valve timing
        //  keep their exact results.
        scalar splitValved
        (
            const scalar Q,
            const scalar Tnode,
            const scalar t,
            scalarList& gateQ
        ) const;

        //- Read the flat \c gates sub-dictionary (legacy topology)
        void readGates(const dictionary& gatesDict);

        //- Read the \c tree sub-dictionary: nodes with a parent, the leaves
        //  becoming the gates
        void readTree(const dictionary& treeDict);

        //- Read the optional valve timing of one gate
        void readValve(const dictionary& gateDict, const label g);

        //- Tree network solve: one bounded fixed point over the whole tree,
        //  parents before children for the flow and temperatures, children
        //  before parents for the equivalent resistances; branches that
        //  carry no open gate are inactive and take no flow. Returns the
        //  pressure drop from the feed node to the leaves and fills the
        //  leaf flows and leaf temperatures
        scalar solveTree
        (
            const scalar Q,
            const scalar t,
            scalarList& leafQ,
            scalarList& leafT
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

        //- Total network pressure drop at the total flow rate [Pa]. A gate
        //  is open for gateOpenTime <= t < gateCloseTime; closed branches
        //  carry no flow and the open ones share the total.
        scalar pressureDrop(const scalar Q, const scalar t = 0) const;

        //- Volumetric flow rate through gate g [m^3/s] (zero while closed)
        scalar gateFlow(const label g, const scalar Q, const scalar t = 0) const;

        //- Melt temperature at gate g [K]
        scalar gateTemperature(const label g, const scalar Q, const scalar t = 0)
        const;

        //- Number of gates
        label nGates() const
        {
            return gates_.size();
        }

        //- Is the network an arbitrary tree (a \c tree sub-dictionary was
        //  supplied) rather than the flat feed + gates topology?
        bool treeMode() const
        {
            return treeMode_;
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
