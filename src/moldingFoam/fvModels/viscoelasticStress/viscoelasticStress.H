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
    Foam::fv::viscoelasticStress

Description
    Momentum source coupling the single-mode viscoelastic extra stress into
    the flow: the model owns a \c tau field evolved locally with the
    moldingViscoelastic constitutive law and adds \c div(tau) to the
    momentum equation (stage two of task 027).

    The model is selected from \c constant/fvModels, e.g.

    \verbatim
    viscoelastic
    {
        type                viscoelasticStress;
        relaxationTime      0.1;
        zeroShearViscosity  100;
        mobilityFactor      0;
    }
    \endverbatim

    The stress is advanced once per time step with the local velocity
    gradient (RK2); advection of tau is the next stage.

SourceFiles
    viscoelasticStress.C

\*---------------------------------------------------------------------------*/

#ifndef viscoelasticStress_H
#define viscoelasticStress_H

#include "fvModel.H"
#include "moldingViscoelastic.H"
#include "volFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{

/*---------------------------------------------------------------------------*\
                    Class viscoelasticStress Declaration
\*---------------------------------------------------------------------------*/

class viscoelasticStress
:
    public fvModel
{
    // Private Data

        //- Constitutive model
        moldingViscoelastic model_;

        //- Velocity field name
        word UName_;

        //- Extra stress field [Pa]
        mutable volSymmTensorField tau_;

        //- Last time index at which tau was advanced
        mutable label timeIndex_;


public:

    //- Runtime type information
    TypeName("viscoelasticStress");


    // Constructors

        //- Construct from components
        viscoelasticStress
        (
            const word& name,
            const word& modelType,
            const fvMesh& mesh,
            const dictionary& dict
        );

        //- Disallow default bitwise copy construction
        viscoelasticStress(const viscoelasticStress&) = delete;


    //- Destructor
    virtual ~viscoelasticStress() = default;


    // Member Functions

        //- Return the list of fields for which the fvModel adds a source
        virtual wordList addSupFields() const;

        //- Does the model add a source to the given field?
        virtual bool addsSupToField(const word& fieldName) const;

        //- Add the stress divergence to the momentum equation
        virtual void addSup
        (
            const volScalarField& rho,
            const volVectorField& U,
            fvMatrix<vector>& eqn
        ) const;

        // Mesh changes

            virtual bool movePoints();

            virtual void topoChange(const polyTopoChangeMap&);

            virtual void mapMesh(const polyMeshMap&);

            virtual void distribute(const polyDistributionMap&);

        //- Read the model dictionary
        virtual bool read(const dictionary& dict);


    // Member Operators

        //- Disallow default bitwise assignment
        void operator=(const viscoelasticStress&) = delete;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fv
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
