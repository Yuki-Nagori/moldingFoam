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
    Foam::fv::moldingEigenstrain

Description
    Domain source for a non-uniform tensor eigenstrain in the solid
    displacement solver (task 040).

    Adds the divergence of the thermal eigenstress

        div(threeK*eigenstrain)

    to the displacement equation, the domain generalisation of the face
    traction -threeK*eigenstrain already delivered by the
    moldingTractionDisplacement boundary condition (task 034). threeK is
    built from the solid thermo exactly as the solver does (E/(1 - nu)
    under plane stress, E/(1 - 2*nu) otherwise).

    The solid solver assembles the hook as rho*fvModels().d2dt2(D), and
    d2dt2 is an ordinary source assembly (fvModelTemplates.C:
    sourceTerm(field, dimVolume/sqr(dimTime), field)), so the model adds
    the source **divided by rho** to compensate for that outer scaling.

    Selection from constant/fvModels, e.g.

    \verbatim
    eigenstrainSource
    {
        type            moldingEigenstrain;
        eigenstrain     eigenstrain;   // field name (default)
    }
    \endverbatim

SourceFiles
    moldingEigenstrain.C

\*---------------------------------------------------------------------------*/

#ifndef moldingEigenstrain_H
#define moldingEigenstrain_H

#include "fvModel.H"
#include "solidDisplacementThermo.H"
#include "volFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{

/*---------------------------------------------------------------------------*\
                   Class moldingEigenstrain Declaration
\*---------------------------------------------------------------------------*/

class moldingEigenstrain
:
    public fvModel
{
    // Private Data

        //- Reference to the solid thermo
        const solidDisplacementThermo& thermo_;

        //- Eigenstrain field name
        word eigenstrainName_;

        //- Displacement field name
        word DName_;


public:

    //- Runtime type information
    TypeName("moldingEigenstrain");


    // Constructors

        //- Construct from explicit source name and mesh
        moldingEigenstrain
        (
            const word& sourceName,
            const word& modelType,
            const fvMesh& mesh,
            const dictionary& dict
        );

        //- Disallow default bitwise copy construction
        moldingEigenstrain(const moldingEigenstrain&) = delete;


    //- Destructor
    virtual ~moldingEigenstrain() = default;


    // Member Functions

        //- Return the list of fields for which the model adds a source
        virtual wordList addSupFields() const;

        //- Does the model add a source to the given field?
        virtual bool addsSupToField(const word& fieldName) const;

        //- Add the eigenstress divergence to the displacement equation
        virtual void addSup
        (
            const volVectorField& D,
            fvMatrix<vector>& eqn
        ) const;

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


    // Member Operators

        //- Disallow default bitwise assignment
        void operator=(const moldingEigenstrain&) = delete;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fv
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
