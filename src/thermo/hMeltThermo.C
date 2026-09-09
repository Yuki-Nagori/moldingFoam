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

#include "hMeltThermo.H"
#include "IOstreams.H"

#ifndef hMeltThermo_C
#define hMeltThermo_C

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class EquationOfState>
Foam::hMeltThermo<EquationOfState>::hMeltThermo
(
    const word& name,
    const dictionary& dict
)
:
    EquationOfState(name, dict),
    Cp_(dict.subDict("thermodynamics").lookup<scalar>("Cp")),
    hf_(dict.subDict("thermodynamics").lookup<scalar>("hf")),
    latentHeat_
    (
        dict
       .subDict("thermodynamics")
       .lookupOrDefault<scalar>("latentHeat", 0)
    ),
    Tref_
    (
        dict
       .subDict("thermodynamics")
       .lookupOrDefault<scalar>
            ("Tref", constant::thermodynamic::Tstd)
    ),
    hsRef_
    (
        dict
       .subDict("thermodynamics")
       .lookupOrDefault<scalar>("hsRef", 0)
    )
{
    const dictionary& thermoDict = dict.subDict("thermodynamics");

    if (latentHeat_ < 0)
    {
        FatalIOErrorInFunction(thermoDict)
            << "The latent heat of solidification must be non-negative: "
            << "latentHeat = " << latentHeat_
            << exit(FatalIOError);
    }
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class EquationOfState>
void Foam::hMeltThermo<EquationOfState>::write(Ostream& os) const
{
    EquationOfState::write(os);

    dictionary dict("thermodynamics");
    dict.add("Cp", Cp_);
    dict.add("hf", hf_);
    if (latentHeat_ != 0)
    {
        dict.add("latentHeat", latentHeat_);
    }
    if (Tref_ != constant::thermodynamic::Tstd)
    {
        dict.add("Tref", Tref_);
    }
    if (hsRef_ != 0)
    {
        dict.add("hsRef", hsRef_);
    }
    os  << indent << dict.dictName() << dict;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class EquationOfState>
Foam::Ostream& Foam::operator<<
(
    Ostream& os,
    const hMeltThermo<EquationOfState>& ct
)
{
    ct.write(os);
    return os;
}


#endif // hMeltThermo_C

// ************************************************************************* //
