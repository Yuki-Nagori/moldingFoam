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

#include "moldingRunnerNetwork.H"
#include "mathematicalConstants.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::moldingRunnerNetwork::moldingRunnerNetwork(const dictionary& dict)
:
    inletTemperature_(dict.lookup<scalar>("inletTemperature")),
    cp_(dict.lookup<scalar>("cp")),
    rho_(dict.lookupOrDefault<scalar>("rho", 800.0)),
    viscosityType_(viscosityType::constant),
    mu_(0),
    K_(0),
    n_(1),
    crossWlfCoeffs_(),
    feed_(),
    gates_()
{
    if (cp_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The runner melt specific heat must be positive: cp = "
            << cp_ << exit(FatalIOError);
    }

    if (rho_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The runner melt density must be positive: rho = "
            << rho_ << exit(FatalIOError);
    }

    const dictionary& visc = dict.subDict("viscosity");
    const word type(visc.lookup<word>("type"));

    if (type == "constant")
    {
        viscosityType_ = viscosityType::constant;
        mu_ = visc.lookup<scalar>("mu");

        if (mu_ <= 0)
        {
            FatalIOErrorInFunction(visc)
                << "The constant runner viscosity must be positive: mu = "
                << mu_ << exit(FatalIOError);
        }
    }
    else if (type == "powerLaw")
    {
        viscosityType_ = viscosityType::powerLaw;
        K_ = visc.lookup<scalar>("K");
        n_ = visc.lookup<scalar>("n");

        if (K_ <= 0 || n_ <= 0)
        {
            FatalIOErrorInFunction(visc)
                << "The power-law runner viscosity requires positive K and "
                << "n: K = " << K_ << ", n = " << n_
                << exit(FatalIOError);
        }
    }
    else if (type == "CrossWlf")
    {
        viscosityType_ = viscosityType::crossWlf;
        crossWlfCoeffs_ =
            laminarModels::generalisedNewtonianViscosityModels::CrossWlf::
            readCoeffs(visc);
    }
    else
    {
        FatalIOErrorInFunction(visc)
            << "Unknown runner viscosity type " << type
            << "; expected constant, powerLaw or CrossWlf"
            << exit(FatalIOError);
    }

    const dictionary& feedDict = dict.subDict("feed");
    feed_.name = feedDict.lookupOrDefault<word>("name", "feed");
    feed_.length = feedDict.lookup<scalar>("length");
    feed_.diameter = feedDict.lookup<scalar>("diameter");
    feed_.wallTemperature =
        feedDict.lookupOrDefault<scalar>("wallTemperature", 0.0);
    feed_.htc = feedDict.lookupOrDefault<scalar>("htc", 0.0);

    const dictionary& gatesDict = dict.subDict("gates");
    const wordList names(gatesDict.toc());
    gates_.setSize(names.size());

    forAll(names, i)
    {
        const dictionary& g = gatesDict.subDict(names[i]);

        gates_[i].name = names[i];
        gates_[i].length = g.lookup<scalar>("length");
        gates_[i].diameter = g.lookup<scalar>("diameter");
        gates_[i].wallTemperature =
            g.lookupOrDefault<scalar>("wallTemperature", 0.0);
        gates_[i].htc = g.lookupOrDefault<scalar>("htc", 0.0);

        if (gates_[i].length <= 0 || gates_[i].diameter <= 0)
        {
            FatalIOErrorInFunction(g)
                << "Runner gate " << names[i]
                << " requires positive length and diameter: length = "
                << gates_[i].length << ", diameter = " << gates_[i].diameter
                << exit(FatalIOError);
        }
    }

    if (feed_.length <= 0 || feed_.diameter <= 0)
    {
        FatalIOErrorInFunction(feedDict)
            << "The runner feed requires positive length and diameter: "
            << "length = " << feed_.length
            << ", diameter = " << feed_.diameter
            << exit(FatalIOError);
    }

    if (gates_.empty())
    {
        FatalIOErrorInFunction(gatesDict)
            << "The runner network needs at least one gate"
            << exit(FatalIOError);
    }
}


// * * * * * * * * * * * * * * * Static Member Functions * * * * * * * * * * //

Foam::scalar Foam::moldingRunnerNetwork::hagenPoiseuille
(
    const scalar mu,
    const scalar L,
    const scalar D,
    const scalar Q
)
{
    return 128*mu*L*Q
      /(constant::mathematical::pi*D*D*D*D);
}


Foam::scalar Foam::moldingRunnerNetwork::shearRate
(
    const scalar Q,
    const scalar D
)
{
    return 32*Q/(constant::mathematical::pi*D*D*D);
}


// * * * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * //

Foam::scalar Foam::moldingRunnerNetwork::eta
(
    const scalar gammaDot,
    const scalar T
) const
{
    const scalar g(max(mag(gammaDot), small));

    switch (viscosityType_)
    {
        case viscosityType::constant:
        {
            return mu_;
        }
        case viscosityType::powerLaw:
        {
            return K_*pow(g, n_ - 1);
        }
        case viscosityType::crossWlf:
        {
            // Atmospheric reference pressure: the runner pressure level is
            // far below the packing pressure and only shifts the frozen
            // limit of the CrossWlf model
            return
                laminarModels::generalisedNewtonianViscosityModels::CrossWlf::
                eta(crossWlfCoeffs_, 1e5, T, g);
        }
    }

    return 0;
}


Foam::scalar Foam::moldingRunnerNetwork::segmentTemperature
(
    const segment& s,
    const scalar Tin,
    const scalar Q
) const
{
    if (s.htc <= 0)
    {
        return Tin;
    }

    const scalar mdot(rho_*mag(Q));

    if (mdot <= small)
    {
        return Tin;
    }

    return
        s.wallTemperature
      + (Tin - s.wallTemperature)
       *exp
        (
            -s.htc*constant::mathematical::pi*s.diameter*s.length
            /(mdot*cp_)
        );
}


Foam::scalar Foam::moldingRunnerNetwork::split
(
    const scalar Q,
    const scalar Tnode,
    scalarList& gateQ
) const
{
    const label n = gates_.size();

    gateQ.setSize(n);

    if (n == 1)
    {
        gateQ[0] = Q;

        const scalar gd = shearRate(Q, gates_[0].diameter);

        return 128*eta(gd, Tnode)*gates_[0].length*Q
          /(constant::mathematical::pi
           *gates_[0].diameter*gates_[0].diameter
           *gates_[0].diameter*gates_[0].diameter);
    }

    // Initial equal split, then fixed-point iteration on the branch
    // resistances until the pressure drop is equal (under-relaxed for the
    // shear-thinning resistance feedback)
    forAll(gateQ, i)
    {
        gateQ[i] = Q/n;
    }

    scalar dp = 0;

    for (label iter = 0; iter < 1000; ++iter)
    {
        scalar sumInvR = 0;
        scalarList R(n);

        forAll(gates_, i)
        {
            const scalar D = gates_[i].diameter;
            const scalar gd = shearRate(gateQ[i], D);

            R[i] = 128*eta(gd, Tnode)*gates_[i].length
              /(constant::mathematical::pi*D*D*D*D);

            sumInvR += 1/R[i];
        }

        dp = Q/sumInvR;

        scalar change = 0;

        forAll(gates_, i)
        {
            const scalar qNew = dp/R[i];

            change = max(change, mag(qNew - gateQ[i]));
            gateQ[i] = 0.5*gateQ[i] + 0.5*qNew;
        }

        if (change < 1e-14*max(mag(Q), small))
        {
            break;
        }
    }

    return dp;
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::scalar Foam::moldingRunnerNetwork::pressureDrop(const scalar Q) const
{
    const scalar TfeedOut = segmentTemperature(feed_, inletTemperature_, Q);

    const scalar dpFeed =
        128*eta
        (
            shearRate(Q, feed_.diameter),
            0.5*(inletTemperature_ + TfeedOut)
        )
       *feed_.length*Q
       /(constant::mathematical::pi
        *feed_.diameter*feed_.diameter
        *feed_.diameter*feed_.diameter);

    scalarList gateQ;

    return dpFeed + split(Q, TfeedOut, gateQ);
}


Foam::scalar Foam::moldingRunnerNetwork::gateFlow
(
    const label g,
    const scalar Q
) const
{
    const scalar Tnode = segmentTemperature(feed_, inletTemperature_, Q);

    scalarList gateQ;
    split(Q, Tnode, gateQ);

    return gateQ[g];
}


Foam::scalar Foam::moldingRunnerNetwork::gateTemperature
(
    const label g,
    const scalar Q
) const
{
    const scalar Tnode = segmentTemperature(feed_, inletTemperature_, Q);

    scalarList gateQ;
    split(Q, Tnode, gateQ);

    return segmentTemperature(gates_[g], Tnode, gateQ[g]);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
