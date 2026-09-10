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

#include "moldingCoolantChannel.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * Static Member Functions * * * * * * * * * * //

Foam::scalar Foam::moldingCoolantChannel::Nu
(
    const scalar C,
    const scalar m,
    const scalar n,
    const scalar Re,
    const scalar Pr
)
{
    return C*pow(Re, m)*pow(Pr, n);
}


Foam::scalar Foam::moldingCoolantChannel::htcFromNu
(
    const scalar Nu,
    const scalar k,
    const scalar D
)
{
    return Nu*k/D;
}


void Foam::moldingCoolantChannel::group
(
    const UList<scalar>& magSf,
    const UList<scalar>& proj,
    const scalar htc,
    const scalar tol,
    List<scalar>& groupHA,
    labelList& groupStart
)
{
    groupHA.clear();
    groupStart.clear();

    label i = 0;
    while (i < magSf.size())
    {
        const scalar p = proj[i];

        scalar hA = 0;
        label j = i;
        for (; j < magSf.size() && mag(proj[j] - p) <= tol; ++j)
        {
            hA += htc*magSf[j];
        }

        groupStart.append(i);
        groupHA.append(hA);

        i = j;
    }
}


Foam::scalar Foam::moldingCoolantChannel::march
(
    const scalar Tin,
    const scalar mdotCp,
    const UList<scalar>& groupHA,
    const UList<scalar>& groupTw,
    List<scalar>& groupTc
)
{
    groupTc.setSize(groupHA.size());

    scalar Tc = Tin;
    scalar hATc = 0;

    forAll(groupHA, g)
    {
        groupTc[g] = Tc;
        hATc += groupHA[g]*Tc;

        Tc += groupHA[g]*(groupTw[g] - Tc)/mdotCp;
    }

    return hATc;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
