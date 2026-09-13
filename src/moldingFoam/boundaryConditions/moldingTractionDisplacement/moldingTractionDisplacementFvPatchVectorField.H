/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | moldingFoam: injection molding solver modules
   \\      /  Website        | https://openfoam.org
    \\  /   A nd           | Copyright (C) 2026 Yuki Lu
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
    Foam::moldingTractionDisplacementFvPatchVectorField

Description
    Fixed traction boundary condition for the linear elastic displacement
    equation with an optional **symmTensor eigenstrain** field (task 034
    follow-up): the free-surface traction accounts for
    sigma_th = -threeK*eigenstrain, generalising the upstream isotropic
    thermal-stress term -I*threeK*alphav*T. Reading the eigenstrain from a
    registered volSymmTensorField (default name `eigenstrain`) lets the
    mapped anisotropic shrinkage enter the solid constitutive directly.

    The boundary condition works with the upstream solidDisplacement
    solver: the traction/pressure entries are identical to
    tractionDisplacement.

SourceFiles
    moldingTractionDisplacementFvPatchVectorField.C

\*---------------------------------------------------------------------------*/

#ifndef moldingTractionDisplacementFvPatchVectorField_H
#define moldingTractionDisplacementFvPatchVectorField_H

#include "fixedGradientFvPatchFields.H"
#include "Function1.H"
#include "solidDisplacementThermo.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
        Class moldingTractionDisplacementFvPatchVectorField Declaration
\*---------------------------------------------------------------------------*/

class moldingTractionDisplacementFvPatchVectorField
:
    public fixedGradientFvPatchVectorField
{
    // Private Data

        //- Traction [Pa]
        vectorField traction_;

        //- Pressure [Pa]
        autoPtr<Function1<scalar>> pressure_;

        //- Name of the eigenstrain field [-]
        word eigenstrainName_;

        //- Eigenstrain field, loaded on first use (the solid solver does
        //  not read fields it does not use itself): the owner holds a
        //  field created by this condition, the pointer may also refer to
        //  a field already registered in the mesh
        autoPtr<volSymmTensorField> eigenstrainOwner_;
        const volSymmTensorField* eigenstrainPtr_;


public:

    //- Runtime type information
    TypeName("moldingTractionDisplacement");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingTractionDisplacementFvPatchVectorField
        (
            const fvPatch&,
            const DimensionedField<vector, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping given
        //  moldingTractionDisplacementFvPatchVectorField onto a new patch
        moldingTractionDisplacementFvPatchVectorField
        (
            const moldingTractionDisplacementFvPatchVectorField&,
            const fvPatch&,
            const DimensionedField<vector, fvMesh>&,
            const fieldMapper&
        );

        //- Construct as copy setting internal field reference
        moldingTractionDisplacementFvPatchVectorField
        (
            const moldingTractionDisplacementFvPatchVectorField&,
            const DimensionedField<vector, fvMesh>&
        );

        //- Construct and return a clone setting internal field reference
        virtual tmp<fvPatchVectorField> clone
        (
            const DimensionedField<vector, fvMesh>& iF
        ) const
        {
            return tmp<fvPatchVectorField>
            (
                new moldingTractionDisplacementFvPatchVectorField(*this, iF)
            );
        }


    // Member Functions

        //- Update the coefficients associated with the patch field
        virtual void updateCoeffs();

        //- Write
        virtual void write(Ostream&) const;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
