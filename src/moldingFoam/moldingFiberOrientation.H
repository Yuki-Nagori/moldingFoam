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
    Foam::moldingFiberOrientation

Description
    Folgar-Tucker fibre orientation kinetics for short-fibre reinforced
    thermoplastics, used to evolve the second-order orientation tensor

    \verbatim
        a_ij = <p_i p_j>
    \endverbatim

    The evolution follows the Folgar-Tucker equation

    \verbatim
        Da/Dt = (W a - a W)
              + lambda (D a + a D - 2 A:D)
              + 2 CI gammaDot (I - 3 a)
    \endverbatim

    where W and D are the vorticity and strain-rate tensors, gammaDot is
    the strain rate, lambda = (r^2 - 1)/(r^2 + 1) is the fibre shape
    factor, CI the fibre-interaction coefficient and A:D the fourth-order
    closure contraction.

    Two closures are available:

    - quadratic, A = a a, which is exact for a single fibre and therefore
      reproduces the Jeffery orbit of a dilute suspension exactly;
    - hybrid, A = f A_linear + (1 - f) A_quadratic with
      f = 1 - 27 det(a).

    The advance is a midpoint (RK2) step followed by a trace
    normalisation, so tr(a) = 1 is preserved to machine precision and the
    tensor stays symmetric.

SourceFiles
    moldingFiberOrientation.C

\*---------------------------------------------------------------------------*/

#ifndef moldingFiberOrientation_H
#define moldingFiberOrientation_H

#include "dictionary.H"
#include "symmTensor.H"
#include "tensor.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
               Class moldingFiberOrientation Declaration
\*---------------------------------------------------------------------------*/

class moldingFiberOrientation
{
public:

    //- Closure approximation
    enum class closureType
    {
        quadratic,
        hybrid
    };


private:

    // Private Data

        //- Fibre shape factor lambda = (r^2 - 1)/(r^2 + 1) [-]
        scalar lambda_;

        //- Fibre interaction coefficient CI [-]
        scalar CI_;

        //- Closure approximation
        closureType closure_;


    // Private Member Functions

        //- Fourth-order closure contraction (A:D)_ij
        symmTensor AcolonD
        (
            const symmTensor& a,
            const symmTensor& D
        ) const;


public:

    // Constructors

        //- Construct from a dictionary
        moldingFiberOrientation(const dictionary& dict);


    // Static Member Functions

        //- Shape factor from the fibre aspect ratio r
        static scalar shapeFactor(const scalar r);


    // Member Functions

        //- Orientation evolution rate Da/Dt for a velocity gradient [1/s]
        symmTensor dadt
        (
            const symmTensor& a,
            const tensor& gradU
        ) const;

        //- Advance the orientation tensor over dt (RK2 + trace
        //  normalisation)
        symmTensor advance
        (
            const symmTensor& a,
            const tensor& gradU,
            const scalar dt
        ) const;

        //- Anisotropic thermal conductivity tensor (task 034):
        //      lambda = kappa (I + anisotropy (a - I/3))
        //  trace-preserving (the isotropic conductivity is recovered for
        //  an isotropic orientation tensor); the aligned direction
        //  conducts by kappa (1 + 2/3 anisotropy)
        static symmTensor conductivityTensor
        (
            const symmTensor& a,
            const scalar kappa,
            const scalar anisotropy
        )
        {
            const symmTensor I(1, 0, 0, 1, 0, 1);

            return kappa*(I + anisotropy*(a - I/3));
        }

        //- Lipscomb-type anisotropic viscosity factor (task 034): the
        //  apparent viscosity of a fibre suspension relative to its
        //  transverse value, driven by the alignment of the strain rate
        //  with the orientation,
        //      factor = 1 + (ratio - 1) * 3/2 * (A:D):D / (D:D)
        //  clamped to [1, ratio]. Fully aligned fibres give the axial
        //  value "ratio" for extension along them and 1 for shear across
        //  them; an isotropic orientation gives the base (transverse)
        //  value 1 (engineering model, see the task 034 notes)
        scalar lipscombFactor
        (
            const symmTensor& a,        //- Orientation tensor
            const symmTensor& D,        //- Strain rate tensor [1/s]
            const scalar axialRatio     //- eta_axial/eta_transverse >= 1
        ) const
        {
            const scalar DD = (D && D);

            if (DD < small)
            {
                return 1;
            }

            const scalar f = 1.5*(AcolonD(a, D) && D)/DD;

            return min(max(1 + (axialRatio - 1)*f, scalar(1)), axialRatio);
        }

        //- Fibre shape factor [-]
        scalar lambda() const
        {
            return lambda_;
        }

        //- Fibre interaction coefficient [-]
        scalar CI() const
        {
            return CI_;
        }
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
