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

Class
    Foam::solvers::moldingFoam

Description
    Solver module for injection molding of thermoplastic melts.

    Stage M1: equivalent to \c compressibleVoF; melt/air VoF cavity filling
    with the polymer modelled by the CrossWlf generalised Newtonian
    viscosity model and the Tait equation of state.

    Later stages (roadmap):
    - M2: packing; V/P switch at a filled-volume fraction with a
      pressure-time profile, read from \c constant/moldingDict;
    - M3: cooling; mould wall temperature boundaries and an ejection
      criterion on the average melt temperature.

    Usage in \c system/controlDict:
    \verbatim
        application     foamRun;
        solver          moldingFoam;
        libs            ("libmoldingFoam.so");
    \endverbatim

    The case dictionary layout, including \c constant/moldingDict, is the
    external case-generation contract of the moldingFoam project; keyword
    names are frozen and changes are documented in the project README.

See also
    Foam::solvers::compressibleVoF

SourceFiles
    moldingFoam.C

\*---------------------------------------------------------------------------*/

#ifndef moldingFoam_H
#define moldingFoam_H

#include "compressibleVoF.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace solvers
{

/*---------------------------------------------------------------------------*\
                          Class moldingFoam Declaration
\*---------------------------------------------------------------------------*/

class moldingFoam
:
    public compressibleVoF
{
    // Private Data

        //- Has the ejection criterion been met and the run stopped?
        bool ejected_;

        //- Average melt temperature below which the part can be ejected
        //  [K], read from constant/moldingDict
        scalar ejectionTemperature_;

        //- Packing pressure target at or below which the packing pressure
        //  counts as released, enabling the ejection check [Pa], read
        //  from constant/moldingDict
        scalar releasePressure_;

        //- Total mesh volume [m^3], computed once in the constructor;
        //  the mould cavity mesh is static
        scalar vTot_;

        //- Latent-heat linearisation enabled? True when the melt phase
        //  selects the hMelt thermodynamics with non-zero latentHeat
        //  and a non-degenerate transition band
        bool latentOn_;

        //- Solidification temperature at atmospheric pressure [K]
        //  (hMelt/Tait coefficient b5)
        scalar latentTt0_;

        //- Pressure sensitivity of the solidification temperature
        //  [K/Pa] (hMelt/Tait coefficient b6)
        scalar latentB6_;

        //- Half-width of the solidification band [K]
        scalar latentBand_;

        //- Latent heat of solidification [J/kg]
        scalar latentHeat_;


public:

    //- Runtime type information
    TypeName("moldingFoam");


    // Constructors

        //- Construct from region mesh
        moldingFoam(fvMesh& mesh);

        //- Disallow default bitwise copy construction
        moldingFoam(const moldingFoam&) = delete;


    //- Destructor
    virtual ~moldingFoam();


    // Member Functions

        //- Read controls
        virtual bool read();

        //- Advance the moulding stage and the ejection criterion
        //  before the PIMPLE loop
        virtual void preSolve();

        //- Solve the energy equation. As compressibleVoF, plus a
        //  semi-implicit latent-heat linearisation: when the melt phase
        //  uses the hMelt thermodynamics, the T-matrix diagonal gets
        //  an additional alpha1*rho1*latentCp/dt term (the implicit-Euler
        //  discretisation of the latent storage rate rho*L*dw/dT*dT/dt).
        //  This compensates the negative apparent Cv of the Tait blend
        //  inside the solidification band, where the pressure-shifted
        //  front term of CpMCv = T*alphav^2/psi dominates
        virtual void thermophysicalPredictor();
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace solvers
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
