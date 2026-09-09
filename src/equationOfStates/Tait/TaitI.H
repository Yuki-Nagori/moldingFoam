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
#include "specie.H"

// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::Tt(scalar p) const
{
    return b5_ + b6_*p;
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::v0m(scalar p, scalar T) const
{
    return b1m_ + b2m_*(T - Tt(p));
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::v0s(scalar p, scalar T) const
{
    return b1s_ + b2s_*(T - Tt(p));
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::Bm(scalar T) const
{
    return b3m_*exp(-b4m_*T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::Bs(scalar T) const
{
    return b3s_*exp(-b4s_*T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::fm(scalar p, scalar T) const
{
    // Guard against p < -B(T) which would make the logarithm undefined
    return 1.0 - C_*log(max(scalar(1) + p/Bm(T), scalar(small)));
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::fs(scalar p, scalar T) const
{
    return 1.0 - C_*log(max(scalar(1) + p/Bs(T), scalar(small)));
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::vhatm(scalar p, scalar T) const
{
    return v0m(p, T)*fm(p, T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::vhats(scalar p, scalar T) const
{
    return v0s(p, T)*fs(p, T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::meltX(scalar p, scalar T) const
{
    if (band_ <= scalar(0))
    {
        return T > Tt(p) ? scalar(1) : scalar(0);
    }

    return min(max((T - (Tt(p) - band_))/(2*band_), scalar(0)), scalar(1));
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::meltWeight(scalar p, scalar T) const
{
    // Cubic smoothstep of the normalised band coordinate; the value and
    // the first derivative match the constant branches at the band edges,
    // so the blended specific volume and its derivatives are continuous
    // across the transition
    const scalar x = meltX(p, T);

    return x*x*(3 - 2*x);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::meltWeightPrime(scalar p, scalar T)
const
{
    // d(meltWeight)/dx; vanishes at the band edges
    if (band_ <= scalar(0))
    {
        return 0;
    }

    const scalar x = meltX(p, T);

    return 6*x*(1 - x);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::vhatPrimeP(scalar p, scalar T) const
{
    // d vhat/dp|T of the blended specific volume, including the
    // melt-weight contribution which depends on pressure through Tt(p)
    const scalar vm = vhatm(p, T);
    const scalar vs = vhats(p, T);
    const scalar w = meltWeight(p, T);

    // With smoothBand = 0 the weight derivative vanishes; guard the
    // division so the 0-weight case never multiplies an infinity
    const scalar wp =
        band_ > scalar(0) ? meltWeightPrime(p, T)*(-b6_/(2*band_)) : 0;

    // v0 depends on p through Tt(p): d v0/dp = -b2*b6
    const scalar vmp = -b2m_*b6_*fm(p, T) - C_*v0m(p, T)/(Bm(T) + p);
    const scalar vsp = -b2s_*b6_*fs(p, T) - C_*v0s(p, T)/(Bs(T) + p);

    return w*vmp + wp*(vm - vs) + (1 - w)*vsp;
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::vhatPrimeT(scalar p, scalar T) const
{
    // d vhat/dT|p of the blended specific volume, including the
    // melt-weight contribution
    const scalar vm = vhatm(p, T);
    const scalar vs = vhats(p, T);
    const scalar w = meltWeight(p, T);
    const scalar wt =
        band_ > scalar(0) ? meltWeightPrime(p, T)/(2*band_) : 0;

    // vm/vs from above are reused: vhatm/vhats are the dominant cost of
    // this routine (exp + log each), so they are never computed twice
    const scalar vmt = vm*alphavm(p, T);
    const scalar vst = vs*alphavs(p, T);

    return w*vmt + wt*(vm - vs) + (1 - w)*vst;
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::alphavm(scalar p, scalar T) const
{
    // Volumetric thermal expansion of the melt branch,
    // (1/vhatm) d vhatm/dT|p = v0m'/v0m + fm'/fm, with fm' = -C*b4*p/(B+p)
    return b2m_/v0m(p, T) - C_*b4m_*p/((Bm(T) + p)*fm(p, T));
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::alphavs(scalar p, scalar T) const
{
    // Volumetric thermal expansion of the solid branch
    return b2s_/v0s(p, T) - C_*b4s_*p/((Bs(T) + p)*fs(p, T));
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class Specie>
inline Foam::Tait<Specie>::Tait
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
)
:
    Specie(sp),
    b1m_(b1m),
    b2m_(b2m),
    b1s_(b1s),
    b2s_(b2s),
    b3m_(b3m),
    b4m_(b4m),
    b3s_(b3s),
    b4s_(b4s),
    b5_(b5),
    b6_(b6),
    C_(C),
    band_(band)
{}


template<class Specie>
inline Foam::Tait<Specie>::Tait(const word& name, const Tait<Specie>& pt)
:
    Specie(name, pt),
    b1m_(pt.b1m_),
    b2m_(pt.b2m_),
    b1s_(pt.b1s_),
    b2s_(pt.b2s_),
    b3m_(pt.b3m_),
    b4m_(pt.b4m_),
    b3s_(pt.b3s_),
    b4s_(pt.b4s_),
    b5_(pt.b5_),
    b6_(pt.b6_),
    C_(pt.C_),
    band_(pt.band_)
{}


template<class Specie>
inline Foam::autoPtr<Foam::Tait<Specie>> Foam::Tait<Specie>::clone() const
{
    return autoPtr<Tait<Specie>>::New(*this);
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::vhat(scalar p, scalar T) const
{
    const scalar w = meltWeight(p, T);

    return w*vhatm(p, T) + (1 - w)*vhats(p, T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::rho(scalar p, scalar T) const
{
    return scalar(1)/vhat(p, T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::h(scalar p, scalar T) const
{
    return 0;
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::Cp(scalar p, scalar T) const
{
    return 0;
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::e(scalar p, scalar T) const
{
    return 0;
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::Cv(scalar p, scalar T) const
{
    return 0;
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::sp(scalar p, scalar T) const
{
    // Maxwell relation: (ds/dp)|T = -(dv/dT)|p = -vhat*alphav
    return -vhat(p, T)*alphav(p, T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::sv(scalar p, scalar T) const
{
    NotImplemented;
    return 0;
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::psi(scalar p, scalar T) const
{
    // psi = drho/dp|T = -rho^2 d vhat/dp|T, exact for the blended
    // specific volume
    return -sqr(rho(p, T))*vhatPrimeP(p, T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::Z(scalar p, scalar T) const
{
    return p*vhat(p, T)/(this->R()*T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::CpMCv(scalar p, scalar T) const
{
    // The identity Cp - Cv = T alphav^2/psi with alphav = vT/v and
    // psi = -vP/v^2 reduces exactly to -T vT^2/vP: the blended specific
    // volume cancels out and is never evaluated
    return -T*sqr(vhatPrimeT(p, T))/vhatPrimeP(p, T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::alphav(scalar p, scalar T) const
{
    // alphav = (1/vhat) d vhat/dT|p, exact for the blended specific
    // volume
    return vhatPrimeT(p, T)/vhat(p, T);
}


// * * * * * * * * * * * * * * * Member Operators  * * * * * * * * * * * * * //

template<class Specie>
inline void Foam::Tait<Specie>::operator+=(const Tait<Specie>& pt)
{
    Specie::operator+=(pt);
}


template<class Specie>
inline void Foam::Tait<Specie>::operator*=(const scalar s)
{
    Specie::operator*=(s);
}


// * * * * * * * * * * * * * * * Friend Operators  * * * * * * * * * * * * * //

template<class Specie>
inline Foam::Tait<Specie> Foam::operator+
(
    const Tait<Specie>& pt1,
    const Tait<Specie>& pt2
)
{
    return Tait<Specie>
    (
        static_cast<const Specie&>(pt1) + static_cast<const Specie&>(pt2),
        pt1.b1m_,
        pt1.b2m_,
        pt1.b1s_,
        pt1.b2s_,
        pt1.b3m_,
        pt1.b4m_,
        pt1.b3s_,
        pt1.b4s_,
        pt1.b5_,
        pt1.b6_,
        pt1.C_,
        pt1.band_
    );
}


template<class Specie>
inline Foam::Tait<Specie> Foam::operator*
(
    const scalar s,
    const Tait<Specie>& pt
)
{
    return Tait<Specie>
    (
        s*static_cast<const Specie&>(pt),
        pt.b1m_,
        pt.b2m_,
        pt.b1s_,
        pt.b2s_,
        pt.b3m_,
        pt.b4m_,
        pt.b3s_,
        pt.b4s_,
        pt.b5_,
        pt.b6_,
        pt.C_,
        pt.band_
    );
}


template<class Specie>
inline Foam::Tait<Specie> Foam::operator==
(
    const Tait<Specie>& pt1,
    const Tait<Specie>& pt2
    )
{
    return pt2 - pt1;
}


// ************************************************************************* //
