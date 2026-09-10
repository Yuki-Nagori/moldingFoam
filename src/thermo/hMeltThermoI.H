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

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class EquationOfState>
inline Foam::hMeltThermo<EquationOfState>::hMeltThermo
(
    const EquationOfState& st,
    const scalar Cp,
    const scalar hf,
    const scalar latentHeat,
    const scalar Tref,
    const scalar hsRef
)
:
    EquationOfState(st),
    Cp_(Cp),
    hf_(hf),
    latentHeat_(latentHeat),
    Tref_(Tref),
    hsRef_(hsRef)
{}


template<class EquationOfState>
inline Foam::hMeltThermo<EquationOfState>::hMeltThermo
(
    const word& name,
    const hMeltThermo& ct
)
:
    EquationOfState(name, ct),
    Cp_(ct.Cp_),
    hf_(ct.hf_),
    latentHeat_(ct.latentHeat_),
    Tref_(ct.Tref_),
    hsRef_(ct.hsRef_)
{}


template<class EquationOfState>
inline Foam::autoPtr<Foam::hMeltThermo<EquationOfState>>
Foam::hMeltThermo<EquationOfState>::clone() const
{
    return autoPtr<hMeltThermo<EquationOfState>>
    (
        new hMeltThermo<EquationOfState>(*this)
    );
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::limit
(
    const scalar T
) const
{
    return T;
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::Cp
(
    const scalar p,
    const scalar T
) const
{
    // The sensible (base) heat capacity only: the latent-heat peak is a
    // linearisation device of the energy equation, not a material
    // property, and must stay out of the transport properties (the
    // thermal conductivity of constTransport is kappa = Cp*mu/Pr; a
    // 100x latent peak in Cp would make kappa spike across the 1 K
    // transition band and destroy the energy predictor)
    return Cp_ + EquationOfState::Cp(p, T);
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::hs
(
    const scalar p,
    const scalar T
) const
{
    // Integrating the latent peak from Tref gives L*(w(T) - w(Tref)),
    // i.e. the full latent heat is absorbed/released between the
    // solid and liquid sides of the transition band
    return
        Cp_*(T - Tref_)
      + hsRef_
      + latentHeat_*
        (
            EquationOfState::meltWeight(p, T)
          - EquationOfState::meltWeight(p, Tref_)
        )
      + EquationOfState::h(p, T);
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::ha
(
    const scalar p,
    const scalar T
) const
{
    return hs(p, T) + hf();
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::hf() const
{
    return hf_;
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::es
(
    const scalar p,
    const scalar T
) const
{
    return hs(p, T) - p*EquationOfState::vhat(p, T);
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::ea
(
    const scalar p,
    const scalar T
) const
{
    return es(p, T) + hf();
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::latentCp
(
    const scalar p,
    const scalar T
) const
{
    // Apparent-Cp latent-heat peak: d(hs)/dT - Cp
    return latentHeat_*EquationOfState::latentCpWeight(p, T);
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::Cv
(
    const scalar p,
    const scalar T
) const
{
    // Apparent heat capacity at constant volume used by the energy
    // equation: base Cp + latent-heat peak - (Cp - Cv) coupling. This
    // is d(es)/dT up to the neglected (small) p*dv/dT term, and stays
    // positive across the solidification band so the energy matrix
    // diagonal is positive definite
    return Cp(p, T) + latentCp(p, T) - EquationOfState::CpMCv(p, T);
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::s
(
    const scalar p,
    const scalar T
) const
{
    // The latent term L*integral(dw/dT/T) is evaluated to first order
    // around Tstd; entropy is not used by the solver
    return
        Cp_*log(T/constant::thermodynamic::Tstd)
      + latentHeat_*
        (
            EquationOfState::meltWeight(p, T)
          - EquationOfState::meltWeight(p, constant::thermodynamic::Tstd)
        )/constant::thermodynamic::Tstd
      + EquationOfState::sp(p, T);
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::gStd
(
    const scalar T
) const
{
    return Cp_*(T - Tref_) + hsRef_ + hf() - Cp_*T*log(T/constant::thermodynamic::Tstd);
}


template<class EquationOfState>
inline Foam::scalar Foam::hMeltThermo<EquationOfState>::dCpdT
(
    const scalar p,
    const scalar T
) const
{
    return 0;
}


// * * * * * * * * * * * * * * * Member Operators  * * * * * * * * * * * * * //

template<class EquationOfState>
inline void Foam::hMeltThermo<EquationOfState>::operator+=
(
    const hMeltThermo& ct
)
{
    scalar Y1 = this->Y();

    EquationOfState::operator+=(ct);

    if (mag(this->Y()) > small)
    {
        Y1 /= this->Y();
        const scalar Y2 = ct.Y()/this->Y();

        Cp_ = Y1*Cp_ + Y2*ct.Cp_;
        hf_ = Y1*hf_ + Y2*ct.hf_;
        latentHeat_ = Y1*latentHeat_ + Y2*ct.latentHeat_;
        hsRef_ = Y1*hsRef_ + Y2*ct.hsRef_;
    }
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
