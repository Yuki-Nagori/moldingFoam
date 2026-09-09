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

\*---------------------------------------------------------------------------*/

#include "Tait.H"
#include "IOstreams.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class Specie>
Foam::Tait<Specie>::Tait(const word& name, const dictionary& dict)
:
    Specie(name, dict),
    b1m_(dict.subDict("equationOfState").lookup<scalar>("b1m")),
    b2m_(dict.subDict("equationOfState").lookupOrDefault<scalar>("b2m", 0)),
    b1s_(dict.subDict("equationOfState").lookup<scalar>("b1s")),
    b2s_(dict.subDict("equationOfState").lookup<scalar>("b2s")),
    b3m_(dict.subDict("equationOfState").lookup<scalar>("b3")),
    b4m_(dict.subDict("equationOfState").lookup<scalar>("b4")),
    b3s_(dict.subDict("equationOfState").lookupOrDefault<scalar>("b3s", b3m_)),
    b4s_(dict.subDict("equationOfState").lookupOrDefault<scalar>("b4s", b4m_)),
    b5_(dict.subDict("equationOfState").lookup<scalar>("b5")),
    b6_(dict.subDict("equationOfState").lookup<scalar>("b6")),
    C_(dict.subDict("equationOfState").lookupOrDefault<scalar>("C", 0.0894)),
    band_(dict.subDict("equationOfState").lookupOrDefault<scalar>("smoothBand", 0.5))
{
    if (b1m_ <= 0 || b1s_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "b1m and b1s must be positive:"
            << " b1m = " << b1m_ << ", b1s = " << b1s_
            << exit(FatalIOError);
    }

    if (b3m_ <= 0 || b3s_ <= 0)
    {
        FatalIOErrorInFunction(dict)
            << "b3 and b3s must be positive:"
            << " b3 = " << b3m_ << ", b3s = " << b3s_
            << exit(FatalIOError);
    }

    if (band_ < 0)
    {
        FatalIOErrorInFunction(dict)
            << "smoothBand must not be negative: smoothBand = " << band_
            << exit(FatalIOError);
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class Specie>
void Foam::Tait<Specie>::write(Ostream& os) const
{
    Specie::write(os);

    dictionary dict("equationOfState");
    dict.add<scalar>("b1m", b1m_);
    dict.add<scalar>("b2m", b2m_);
    dict.add<scalar>("b1s", b1s_);
    dict.add<scalar>("b2s", b2s_);
    dict.add<scalar>("b3", b3m_);
    dict.add<scalar>("b4", b4m_);
    dict.add<scalar>("b3s", b3s_);
    dict.add<scalar>("b4s", b4s_);
    dict.add<scalar>("b5", b5_);
    dict.add<scalar>("b6", b6_);
    dict.add<scalar>("C", C_);
    dict.add<scalar>("smoothBand", band_);

    os  << indent << dict.dictName() << dict;
}


// * * * * * * * * * * * * * * * Ostream Operator  * * * * * * * * * * * * * //

template<class Specie>
Foam::Ostream& Foam::operator<<(Ostream& os, const Tait<Specie>& pt)
{
    pt.write(os);
    return os;
}


// ************************************************************************* //
