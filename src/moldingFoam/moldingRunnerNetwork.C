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

// * * * * * * * * * * * * * * * Local Data  * * * * * * * * * * * * * * * * //

// Bounded fixed-point budget of one tree solve (see the header): the tree
// iteration is a single level over the whole network, so the cost of a run
// is O(maxTreeIterations_ * nodes) and cannot blow up with the depth the way
// the nested bisection of the first attempt did (ai-docs/tasks/058).
const label moldingRunnerNetwork::maxTreeIterations_ = 500;


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
    gates_(),
    treeMode_(false),
    treeSeg_(),
    treeParent_(),
    treeChildren_(),
    treeRoots_(),
    treeOrder_(),
    treeLeaves_(),
    treeLeafOf_(),
    gateOpenTime_(),
    gateCloseTime_(),
    anyValveTiming_(false),
    treeTolerance_(dict.lookupOrDefault<scalar>("treeConvergenceTolerance", 1e-14))
{
    if (cp_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "The runner melt specific heat must be positive: cp = "
            << cp_ << exit(FatalIOError);
    }

    if (treeTolerance_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "treeConvergenceTolerance must be positive: "
            << treeTolerance_ << exit(FatalIOError);
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

    // Topology: either the legacy flat feed + parallel gates, or an
    // arbitrary tree whose leaves are the gates (task 058). The legacy path
    // is left untouched so that existing cases stay bit-for-bit unchanged.
    if (dict.found("tree"))
    {
        treeMode_ = true;
        readTree(dict.subDict("tree"));
    }
    else
    {
        readGates(dict.subDict("gates"));
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
        FatalIOErrorInFunction(dict)
            << "The runner network needs at least one gate"
            << (treeMode_ ? " (a tree leaf)" : "")
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


Foam::scalar Foam::moldingRunnerNetwork::segmentResistance
(
    const segment& s,
    const scalar Q,
    const scalar T
) const
{
    return
        128*eta(shearRate(Q, s.diameter), T)*s.length
       /(constant::mathematical::pi
        *s.diameter*s.diameter*s.diameter*s.diameter);
}


Foam::scalar Foam::moldingRunnerNetwork::feedPressureDrop(const scalar Q) const
{
    const scalar TfeedOut = segmentTemperature(feed_, inletTemperature_, Q);

    return
        128*eta
        (
            shearRate(Q, feed_.diameter),
            0.5*(inletTemperature_ + TfeedOut)
        )
       *feed_.length*Q
       /(constant::mathematical::pi
        *feed_.diameter*feed_.diameter
        *feed_.diameter*feed_.diameter);
}


bool Foam::moldingRunnerNetwork::gateOpen(const label g, const scalar t) const
{
    return t >= gateOpenTime_[g] && t < gateCloseTime_[g];
}


void Foam::moldingRunnerNetwork::readValve
(
    const dictionary& gateDict,
    const label g
)
{
    gateOpenTime_[g] = gateDict.lookupOrDefault<scalar>("gateOpenTime", 0.0);
    gateCloseTime_[g] =
        gateDict.lookupOrDefault<scalar>("gateCloseTime", great);

    if (gateCloseTime_[g] <= gateOpenTime_[g])
    {
        FatalIOErrorInFunction(gateDict)
            << "A gate valve must close after it opens: gateOpenTime = "
            << gateOpenTime_[g] << ", gateCloseTime = " << gateCloseTime_[g]
            << exit(FatalIOError);
    }

    if (gateOpenTime_[g] != 0 || gateCloseTime_[g] != great)
    {
        anyValveTiming_ = true;
    }
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

        if (change < treeTolerance_*max(mag(Q), small))
        {
            break;
        }
    }

    return dp;
}


Foam::scalar Foam::moldingRunnerNetwork::splitValved
(
    const scalar Q,
    const scalar Tnode,
    const scalar t,
    scalarList& gateQ
) const
{
    const label n = gates_.size();

    gateQ.setSize(n);

    boolList open(n, false);

    label nOpen = 0;

    forAll(gates_, i)
    {
        open[i] = gateOpen(i, t);

        if (open[i])
        {
            ++nOpen;
        }
    }

    // All valves shut: nothing flows and the network develops no drop
    if (nOpen == 0)
    {
        forAll(gateQ, i)
        {
            gateQ[i] = 0;
        }

        return 0;
    }

    forAll(gates_, i)
    {
        gateQ[i] = (open[i] ? Q/scalar(nOpen) : 0);
    }

    // Same fixed-point iteration and under-relaxation as split(), over the
    // open branches only (1000 iterations mirrors split())
    scalar dp = 0;

    for (label iter = 0; iter < 1000; ++iter)
    {
        scalar sumInvR = 0;
        scalarList R(n, scalar(0));

        forAll(gates_, i)
        {
            if (!open[i])
            {
                continue;
            }

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
            if (!open[i])
            {
                continue;
            }

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


void Foam::moldingRunnerNetwork::readGates(const dictionary& gatesDict)
{
    const wordList names(gatesDict.toc());
    gates_.setSize(names.size());
    gateOpenTime_.setSize(names.size());
    gateCloseTime_.setSize(names.size());

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

        readValve(g, i);
    }
}


void Foam::moldingRunnerNetwork::readTree(const dictionary& treeDict)
{
    const wordList names(treeDict.toc());
    const label n = names.size();

    if (n == 0)
    {
        FatalIOErrorInFunction(treeDict)
            << "The runner tree needs at least one node"
            << exit(FatalIOError);
    }

    treeSeg_.setSize(n);
    treeParent_.setSize(n);

    forAll(names, i)
    {
        const dictionary& nd = treeDict.subDict(names[i]);
        segment& s = treeSeg_[i];

        s.name = names[i];
        s.length = nd.lookup<scalar>("length");
        s.diameter = nd.lookup<scalar>("diameter");
        s.wallTemperature = nd.lookupOrDefault<scalar>("wallTemperature", 0.0);
        s.htc = nd.lookupOrDefault<scalar>("htc", 0.0);

        if (s.length <= 0 || s.diameter <= 0)
        {
            FatalIOErrorInFunction(nd)
                << "Runner tree node " << names[i]
                << " requires positive length and diameter: length = "
                << s.length << ", diameter = " << s.diameter
                << exit(FatalIOError);
        }

        // Parent: the feed or another node of the tree. Requiring it
        // explicitly keeps the topology in the dictionary rather than
        // implied by nesting.
        const word p(nd.lookup<word>("parent"));

        treeParent_[i] = -1;

        forAll(names, j)
        {
            if (names[j] == p)
            {
                treeParent_[i] = j;
                break;
            }
        }

        if (p != "feed" && treeParent_[i] < 0)
        {
            FatalIOErrorInFunction(nd)
                << "Runner tree node " << names[i] << " has unknown parent "
                << p << "; expected feed or a node of the tree"
                << exit(FatalIOError);
        }
    }

    // Depth by resolving parents whose depth is already known; a node that
    // never resolves is part of a cycle (or of a chain that does not reach
    // the feed) and is reported rather than iterated on.
    labelList depth(n, label(-1));

    bool changed = true;

    for (label pass = 0; pass < n && changed; ++pass)
    {
        changed = false;

        forAll(treeParent_, i)
        {
            if (depth[i] >= 0)
            {
                continue;
            }

            const label p = treeParent_[i];

            if (p < 0)
            {
                depth[i] = 0;
                changed = true;
            }
            else if (depth[p] >= 0)
            {
                depth[i] = depth[p] + 1;
                changed = true;
            }
        }
    }

    label maxDepth = 0;

    forAll(depth, i)
    {
        if (depth[i] < 0)
        {
            FatalIOErrorInFunction(treeDict)
                << "Runner tree node " << names[i] << " has a parent chain "
                << "that does not reach feed (cycle?)"
                << exit(FatalIOError);
        }

        if (depth[i] > maxDepth)
        {
            maxDepth = depth[i];
        }
    }

    // Children lists (count first: labelList has no push_back)
    labelList nChildren(n, label(0));

    forAll(treeParent_, i)
    {
        if (treeParent_[i] >= 0)
        {
            ++nChildren[treeParent_[i]];
        }
    }

    treeChildren_.setSize(n);

    forAll(treeChildren_, i)
    {
        treeChildren_[i].setSize(nChildren[i]);
        nChildren[i] = 0;
    }

    forAll(treeParent_, i)
    {
        const label p = treeParent_[i];

        if (p >= 0)
        {
            treeChildren_[p][nChildren[p]++] = i;
        }
    }

    label nRoots = 0;
    label nLeaves = 0;

    forAll(depth, i)
    {
        if (depth[i] == 0)
        {
            ++nRoots;
        }
        if (treeChildren_[i].empty())
        {
            ++nLeaves;
        }
    }

    treeRoots_.setSize(nRoots);
    treeLeaves_.setSize(nLeaves);
    treeOrder_.setSize(n);
    treeLeafOf_.setSize(n, label(-1));

    label r = 0;
    label k = 0;

    // Parents before children: the solver walks this order forwards for the
    // flows/temperatures and backwards for the equivalent resistances
    for (label d = 0; d <= maxDepth; ++d)
    {
        forAll(depth, i)
        {
            if (depth[i] != d)
            {
                continue;
            }

            if (d == 0)
            {
                treeRoots_[r++] = i;
            }

            treeOrder_[k++] = i;
        }
    }

    // The leaves are the gates, numbered in dictionary order
    label l = 0;

    forAll(treeChildren_, i)
    {
        if (treeChildren_[i].empty())
        {
            treeLeaves_[l++] = i;
        }
    }

    // Fill the gate view the boundaries use, with the node lookup the
    // solver needs for the valve state of a leaf
    gates_.setSize(treeLeaves_.size());
    gateOpenTime_.setSize(treeLeaves_.size());
    gateCloseTime_.setSize(treeLeaves_.size());

    forAll(treeLeaves_, g)
    {
        const label i = treeLeaves_[g];

        gates_[g] = treeSeg_[i];
        treeLeafOf_[i] = g;

        readValve(treeDict.subDict(names[i]), g);
    }
}


Foam::scalar Foam::moldingRunnerNetwork::solveTree
(
    const scalar Q,
    const scalar t,
    scalarList& leafQ,
    scalarList& leafT
) const
{
    const label n = treeSeg_.size();
    const label nLeaves = treeLeaves_.size();

    leafQ.setSize(nLeaves);
    leafT.setSize(nLeaves);

    if (n == 0 || nLeaves == 0)
    {
        leafQ = 0;
        leafT = inletTemperature_;

        return 0;
    }

    // Melt temperature entering the tree from the feed
    const scalar TfeedOut = segmentTemperature(feed_, inletTemperature_, Q);

    // Uniform starting distribution; the iteration below redistributes the
    // flow by the branch resistances
    scalarList q(n, Q/scalar(n));
    scalarList Tin(n, TfeedOut);
    scalarList Tout(n, TfeedOut);
    scalarList Req(n, scalar(0));

    // A branch takes flow only when it carries an open gate: a leaf follows
    // its valve, a node is active when any of its descendants is
    boolList active(n, false);

    forAll(treeOrder_, k)
    {
        const label i = treeOrder_[n - 1 - k];

        if (treeChildren_[i].empty())
        {
            active[i] = gateOpen(treeLeafOf_[i], t);
        }
        else
        {
            forAll(treeChildren_[i], c)
            {
                if (active[treeChildren_[i][c]])
                {
                    active[i] = true;
                    break;
                }
            }
        }
    }

    forAll(q, i)
    {
        if (!active[i])
        {
            q[i] = 0;
        }
    }

    scalar dpChild = 0;
    bool converged = false;

    for (label iter = 0; iter < maxTreeIterations_; ++iter)
    {
        // Node temperatures, parents before children
        forAll(treeOrder_, k)
        {
            const label i = treeOrder_[k];

            Tin[i] = (treeParent_[i] < 0 ? TfeedOut : Tout[treeParent_[i]]);
            Tout[i] =
                (active[i]
              ? segmentTemperature(treeSeg_[i], Tin[i], q[i])
              : Tin[i]);
        }

        // Own and equivalent resistances, children before parents: a node's
        // equivalent resistance is its own segment in series with the
        // parallel combination of its children
        forAll(treeOrder_, k)
        {
            const label i = treeOrder_[n - 1 - k];

            if (!active[i])
            {
                Req[i] = 0;
                continue;
            }

            const scalar R = segmentResistance(treeSeg_[i], q[i], Tin[i]);

            scalar invSum = 0;

            forAll(treeChildren_[i], c)
            {
                const label j = treeChildren_[i][c];

                if (active[j])
                {
                    invSum += 1/Req[j];
                }
            }

            Req[i] = (invSum > 0 ? R + 1/invSum : R);
        }

        // Flow targets: the equal-pressure-drop split at every node, each
        // node's inflow shared by its children in inverse proportion to
        // their equivalent resistances
        scalarList qTgt(n, scalar(0));

        scalar invSumRoot = 0;

        forAll(treeRoots_, r)
        {
            const label i = treeRoots_[r];

            if (active[i])
            {
                invSumRoot += 1/Req[i];
            }
        }

        forAll(treeRoots_, r)
        {
            const label i = treeRoots_[r];

            qTgt[i] =
                (active[i] && invSumRoot > 0 ? Q*(1/Req[i])/invSumRoot : 0);
        }

        forAll(treeOrder_, k)
        {
            const label i = treeOrder_[k];

            if (treeChildren_[i].empty())
            {
                continue;
            }

            scalar invSum = 0;

            forAll(treeChildren_[i], c)
            {
                const label j = treeChildren_[i][c];

                if (active[j])
                {
                    invSum += 1/Req[j];
                }
            }

            forAll(treeChildren_[i], c)
            {
                const label j = treeChildren_[i][c];

                qTgt[j] =
                    (active[j] && invSum > 0 ? qTgt[i]*(1/Req[j])/invSum : 0);
            }
        }

        // Pressure drop from the feed node down to the leaves
        dpChild = (invSumRoot > 0 ? Q/invSumRoot : 0);

        // Under-relaxed update: the shear-thinning resistance feedback
        // needs the damping the flat-network split already uses
        scalar change = 0;

        forAll(q, i)
        {
            change = max(change, mag(qTgt[i] - q[i]));
            q[i] = 0.5*q[i] + 0.5*qTgt[i];
        }

        if (change < 1e-14*max(mag(Q), small))
        {
            converged = true;
            break;
        }
    }

    if (!converged)
    {
        WarningInFunction
            << "Runner network fixed-point iteration reached the limit of "
            << maxTreeIterations_ << " without meeting the flow tolerance"
            << nl << "    Q = " << Q << ", t = " << t << nl
            << "    returning the bounded last iterate" << endl;
    }

    forAll(treeLeaves_, g)
    {
        const label i = treeLeaves_[g];

        leafQ[g] = q[i];
        leafT[g] = (active[i] ? Tout[i] : TfeedOut);
    }

    return dpChild;
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::scalar Foam::moldingRunnerNetwork::pressureDrop
(
    const scalar Q,
    const scalar t
) const
{
    if (treeMode_)
    {
        scalarList leafQ;
        scalarList leafT;

        return feedPressureDrop(Q) + solveTree(Q, t, leafQ, leafT);
    }

    const scalar Tnode = segmentTemperature(feed_, inletTemperature_, Q);

    scalarList gateQ;

    if (anyValveTiming_)
    {
        return feedPressureDrop(Q) + splitValved(Q, Tnode, t, gateQ);
    }

    return feedPressureDrop(Q) + split(Q, Tnode, gateQ);
}


Foam::scalar Foam::moldingRunnerNetwork::gateFlow
(
    const label g,
    const scalar Q,
    const scalar t
) const
{
    if (treeMode_)
    {
        scalarList leafQ;
        scalarList leafT;

        solveTree(Q, t, leafQ, leafT);

        return leafQ[g];
    }

    const scalar Tnode = segmentTemperature(feed_, inletTemperature_, Q);

    scalarList gateQ;

    if (anyValveTiming_)
    {
        splitValved(Q, Tnode, t, gateQ);

        return gateQ[g];
    }

    split(Q, Tnode, gateQ);

    return gateQ[g];
}


Foam::scalar Foam::moldingRunnerNetwork::gateTemperature
(
    const label g,
    const scalar Q,
    const scalar t
) const
{
    if (treeMode_)
    {
        scalarList leafQ;
        scalarList leafT;

        solveTree(Q, t, leafQ, leafT);

        return leafT[g];
    }

    const scalar Tnode = segmentTemperature(feed_, inletTemperature_, Q);

    scalarList gateQ;

    if (anyValveTiming_)
    {
        splitValved(Q, Tnode, t, gateQ);

        return segmentTemperature(gates_[g], Tnode, gateQ[g]);
    }

    split(Q, Tnode, gateQ);

    return segmentTemperature(gates_[g], Tnode, gateQ[g]);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
