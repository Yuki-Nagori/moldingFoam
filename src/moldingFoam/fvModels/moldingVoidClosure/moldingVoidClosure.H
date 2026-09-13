/*--------------------------------*- C++ -*----------------------------------*\
  =========                 |
  \\      /  F ield         | moldingFoam: injection molding solver modules
   \\      /   O peration   | https://openfoam.org
    \\      /    A nd       | Copyright (C) 2026 Yuki Lu
     \\/     M anipulation  |
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
    Foam::fv::moldingVoidClosure

Description
    Closure-constrained cavitation for the sealed cooling melt (task 039).

    The model assembles the same phase-continuity and pressure sources as
    the upstream \c compressible::VoFCavitation fvModel (same cavitation
    models and implicit linearisation), but caps the evaporation source at
    the isochoric PVT closure ceiling

        phiClosure = max(0, 1 - rhoRef/rhoMelt)

    where \c rhoRef is the mixture density captured when the cell first
    cavitates (its sealed bulk density, i.e. the melt density at the pin)
    and \c rhoMelt is the melt density at the current pressure. Without the
    cap the phase-change rate sets the void volume: the unconstrained model
    over-evaporates during the cavitation onset, which consumes melt mass
    (measured 25 % at Cv 0.1 and 88 % at Cv 0.3 for the voidCavitation
    case) and inflates the void far beyond the PVT prediction. With the cap
    the void tracks the closure, the sealed melt mass is conserved and the
    calibration window widens.

    Selection from \c constant/fvModels; the cavitation keys are forwarded
    to the upstream cavitation model (\c model, \c pSat, \c n, \c dNuc,
    \c Cv, \c Cc, ...):

    \verbatim
    voidClosure
    {
        type    moldingVoidClosure;

        model   SchnerrSauer;

        pSat
        {
            type    constant;
            value   1e3;
        }
        n       1e10;
        dNuc    1e-6;
        Cv      0.1;
        Cc      10;

        //- Evaporation-ceiling ramp width, as a fraction of the closure
        //  void [0, 1]
        band    0.2;

        //- Apply the closure ceiling; off reproduces the unconstrained
        //  upstream behaviour (ablation/diagnostics)
        closure true;
    }
    \endverbatim

    The ceiling makes the void insensitive to the cavitation coefficients
    (Cv 0.05-0.3, Cc 1-100 all give the closure void), but the ramp width
    is load bearing: the ablation matrix
    (\c scripts/cavitation-closure-matrix.py, task 039) shows band 0.05 and
    0.5 can crash the run and band 0.1 degrades the pressure pin at
    Cc = 100, while the default 0.2 is stable with the pin held at pSat
    throughout. Bands outside [0.1, 0.3] therefore warn.

    \verbatim
    \endverbatim

    The upstream cavitation library must be loadable; the case's
    \c fvModels entry lists \c libs ("libcompressibleVoFCavitation.so")
    for that.

SourceFiles
    moldingVoidClosure.C

\*---------------------------------------------------------------------------*/

#ifndef moldingVoidClosure_H
#define moldingVoidClosure_H

#include "fvModel.H"
#include "compressibleTwoPhaseVoFMixture.H"
#include "cavitationModel.H"
#include "volFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{

/*---------------------------------------------------------------------------*\
                   Class moldingVoidClosure Declaration
\*---------------------------------------------------------------------------*/

class moldingVoidClosure
:
    public fvModel
{
    // Private Data

        //- Reference to the mixture properties
        const compressibleTwoPhaseVoFMixture& mixture_;

        //- The upstream cavitation model
        autoPtr<Foam::compressible::cavitationModel> cavitation_;

        //- Per-cell sealed bulk density [kg/m^3]: captured when the cell
        //  first cavitates, re-armed when the void closes
        mutable scalarField rhoRef_;

        //- Evaporation-ceiling ramp width as a fraction of the closure
        //  void [0, 1]
        scalar band_;

        //- Apply the closure ceiling. Disabling it reproduces the
        //  unconstrained upstream behaviour (the onset over-evaporation),
        //  which is used for ablation and diagnostics
        Switch closure_;



    // Private Member Functions

        //- Element-wise limit scaling that keeps the dimensions of the
        //  unscaled field
        volScalarField::Internal scaledLimit
        (
            const volScalarField::Internal& limit,
            const volScalarField::Internal& field,
            const word& name
        ) const;


public:

    //- Runtime type information
    TypeName("moldingVoidClosure");


    // Constructors

        //- Construct from explicit source name and mesh
        moldingVoidClosure
        (
            const word& sourceName,
            const word& modelType,
            const fvMesh& mesh,
            const dictionary& dict
        );

        //- Disallow default bitwise copy construction
        moldingVoidClosure(const moldingVoidClosure&) = delete;


    //- Destructor
    virtual ~moldingVoidClosure() = default;


    // Member Functions

        //- Return the list of fields for which the model adds a source
        virtual wordList addSupFields() const;

        //- Add a source to the phase continuity/density equations
        virtual void addSup
        (
            const volScalarField& alpha,
            const volScalarField& rho,
            fvMatrix<scalar>& eqn
        ) const;

        //- Correct the cavitation model and refresh the closure reference
        virtual void correct();

        //- Read the model dictionary
        virtual bool read(const dictionary& dict);


        // Mesh changes

            //- Update for mesh motion
            virtual bool movePoints();

            //- Update topology using the given map
            virtual void topoChange(const polyTopoChangeMap&);

            //- Update from another mesh using the given map
            virtual void mapMesh(const polyMeshMap&);

            //- Redistribute or update using the given distribution map
            virtual void distribute(const polyDistributionMap&);


        //- Evaporation limit factor per cell: 1 below the closure ceiling,
        //  ramped to 0 at it, 0 where no void is admissible
        tmp<volScalarField::Internal> evaporationLimit() const;


    // Member Operators

        //- Disallow default bitwise assignment
        void operator=(const moldingVoidClosure&) = delete;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fv
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
