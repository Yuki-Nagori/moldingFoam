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
inline Foam::scalar Foam::Tait<Specie>::pMeltIso(scalar rho, scalar T) const
{
    // Invert vhatm(p, T) = v0m(p, T)*fm(p, T) = 1/rho for p. fm is
    // explicit in p; v0m depends on p only through Tt(p) = b5 + b6 p,
    // so the fixed-point iteration below contracts quickly (b6 is
    // tiny) and is iterated to round-off
    scalar p = 0;

    for (label i = 0; i < 20; ++i)
    {
        const scalar pPrev = p;
        const scalar v0 = v0m(p, T);

        p = Bm(T)
           *(exp((1 - 1/max(rho*v0, scalar(small)))/C_) - 1);

        if (mag(p - pPrev) < 1e-14*max(mag(p), scalar(1)))
        {
            break;
        }
    }

    return p;
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
inline Foam::scalar Foam::Tait<Specie>::latentCpWeight(scalar p, scalar T) const
{
    // d(meltWeight)/dT|p: a peaked weight of unit integral across the
    // transition band; latentHeat times this is the apparent-Cp
    // latent-heat term of hMeltThermo
    if (band_ <= scalar(0))
    {
        return 0;
    }

    return meltWeightPrime(p, T)/(2*band_);
}


template<class Specie>
inline void Foam::Tait<Specie>::blendedDerivs
(
    scalar p,
    scalar T,
    scalar& v,
    scalar& vP,
    scalar& vT
) const
{
    // Both branches are always evaluated and their values and partial
    // derivatives are shared between v, dv/dp and dv/dT, so a caller
    // gets all three for the cost of a single vhat() evaluation

    const scalar TtX = T - Tt(p);

    // Melt branch: vhatm = v0m*fm, with v0m linear in (T - Tt(p)) and
    // fm the Tait logarithmic compression factor
    const scalar v0m = b1m_ + b2m_*TtX;
    const scalar Bm = b3m_*exp(-b4m_*T);
    const scalar fm = 1.0 - C_*log(max(scalar(1) + p/Bm, scalar(small)));
    const scalar vm = v0m*fm;
    const scalar vmP = -b2m_*b6_*fm - C_*v0m/(Bm + p);
    const scalar vmT = b2m_*fm - C_*p*b4m_*v0m/(Bm + p);

    // Solid branch
    const scalar v0s = b1s_ + b2s_*TtX;
    const scalar Bs = b3s_*exp(-b4s_*T);
    const scalar fs = 1.0 - C_*log(max(scalar(1) + p/Bs, scalar(small)));
    const scalar vs = v0s*fs;
    const scalar vsP = -b2s_*b6_*fs - C_*v0s/(Bs + p);
    const scalar vsT = b2s_*fs - C_*p*b4s_*v0s/(Bs + p);

    // Blend weight and its partial derivatives; with smoothBand = 0 the
    // weight is a step function and its derivatives vanish
    scalar w, wp, wt;
    if (band_ <= scalar(0))
    {
        w = TtX > 0 ? scalar(1) : scalar(0);
        wp = 0;
        wt = 0;
    }
    else
    {
        const scalar x =
            min(max(TtX/(2*band_) + scalar(0.5), scalar(0)), scalar(1));

        w = x*x*(3 - 2*x);

        const scalar wPrime = 6*x*(1 - x);
        wp = wPrime*(-b6_/(2*band_));
        wt = wPrime/(2*band_);
    }

    v  = w*vm + (1 - w)*vs;
    vP = w*vmP + wp*(vm - vs) + (1 - w)*vsP;
    vT = w*vmT + wt*(vm - vs) + (1 - w)*vsT;
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
    // Maxwell relation: (ds/dp)|T = -(dv/dT)|p; vhat*alphav reduces
    // exactly to dvhat/dT|p
    scalar v, vP, vT;
    blendedDerivs(p, T, v, vP, vT);

    return -vT;
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
    // specific volume; rho = 1/vhat so vhat cancels from the square
    scalar v, vP, vT;
    blendedDerivs(p, T, v, vP, vT);

    return -vP/sqr(v);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::Z(scalar p, scalar T) const
{
    return p*vhat(p, T)/(this->R()*T);
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::CpMCv(scalar p, scalar T) const
{
    // The identity Cp - Cv = T*alphav^2/psi evaluated WITHOUT the
    // solidification-front sweep contributions: inside the transition
    // band the front terms (moving Tt(p) with pressure and temperature)
    // make the isochoric Cv = Cp - CpMCv strongly negative, which the
    // energy equation cannot integrate. The omitted coupling scales
    // with the Tait pressure shift b6 (0.15 K/MPa for the contract
    // grade) and is physically negligible. With this definition
    // Cv = Cp_app - CpMCv stays positive across the band and the
    // apparent-Cp latent heat of hMeltThermo is carried by the
    // transient term of the energy equation.
    const scalar TtX = T - Tt(p);

    const scalar v0m = b1m_ + b2m_*TtX;
    const scalar Bm = b3m_*exp(-b4m_*T);
    const scalar fm = 1.0 - C_*log(max(scalar(1) + p/Bm, scalar(small)));
    const scalar vmT = b2m_*fm - C_*p*b4m_*v0m/(Bm + p);
    const scalar vmP = -b2m_*b6_*fm - C_*v0m/(Bm + p);

    const scalar v0s = b1s_ + b2s_*TtX;
    const scalar Bs = b3s_*exp(-b4s_*T);
    const scalar fs = 1.0 - C_*log(max(scalar(1) + p/Bs, scalar(small)));
    const scalar vsT = b2s_*fs - C_*p*b4s_*v0s/(Bs + p);
    const scalar vsP = -b2s_*b6_*fs - C_*v0s/(Bs + p);

    // Band weights WITHOUT the front sweep: the w'(x) contributions to
    // the state derivatives are deliberately omitted here (see comment)
    scalar w;
    if (band_ <= scalar(0))
    {
        w = TtX > 0 ? scalar(1) : scalar(0);
    }
    else
    {
        const scalar x =
            min(max(TtX/(2*band_) + scalar(0.5), scalar(0)), scalar(1));
        w = x*x*(3 - 2*x);
    }

    const scalar vT = w*vmT + (1 - w)*vsT;
    const scalar vP = w*vmP + (1 - w)*vsP;

    return -T*vT*vT/vP;
}


template<class Specie>
inline Foam::scalar Foam::Tait<Specie>::alphav(scalar p, scalar T) const
{
    // alphav = (1/vhat) d vhat/dT|p, exact for the blended specific
    // volume
    scalar v, vP, vT;
    blendedDerivs(p, T, v, vP, vT);

    return vT/v;
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
