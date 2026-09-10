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
    Foam::moldingSlipVelocityFvPatchVectorField

Description
    Navier wall-slip velocity boundary for \c U:

    \verbatim
        u_w = b * du_t/dn
    \endverbatim

    with slip length \c b. The condition is applied to the tangential
    component only (the normal component is zero at a static wall), and is
    implemented through the upstream partial-slip machinery with the
    value fraction

    \verbatim
        valueFraction = 1/(1 + b*deltaCoeffs)
    \endverbatim

    so the slip length is a direct physical input [m] and no viscosity
    model lookup is needed. A zero slip length recovers the no-slip wall.

Usage
    \table
        Property   | Description                    | Req'd? | Default
        slipLength | Navier slip length [m]         | yes    |
        value      | Initial patch velocity         | no     |
    \endtable

    Example:
    \verbatim
    walls
    {
        type       moldingSlipVelocity;
        slipLength 1e-4;   // [m]
        value      uniform (0 0 0);
    }
    \endverbatim

See also
    Foam::partialSlipFvPatchVectorField

SourceFiles
    moldingSlipVelocityFvPatchVectorField.C

\*---------------------------------------------------------------------------*/

#ifndef moldingSlipVelocityFvPatchVectorField_H
#define moldingSlipVelocityFvPatchVectorField_H

#include "partialSlipFvPatchFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
         Class moldingSlipVelocityFvPatchVectorField Declaration
\*---------------------------------------------------------------------------*/

class moldingSlipVelocityFvPatchVectorField
:
    public partialSlipFvPatchVectorField
{
    // Private Data

        //- Navier slip length [m]
        scalar slipLength_;


public:

    //- Runtime type information
    TypeName("moldingSlipVelocity");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingSlipVelocityFvPatchVectorField
        (
            const fvPatch&,
            const DimensionedField<vector, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping onto a new patch
        moldingSlipVelocityFvPatchVectorField
        (
            const moldingSlipVelocityFvPatchVectorField&,
            const fvPatch&,
            const DimensionedField<vector, fvMesh>&,
            const fieldMapper&
        );

        //- Disallow copy without setting internal field reference
        moldingSlipVelocityFvPatchVectorField
        (
            const moldingSlipVelocityFvPatchVectorField&
        ) = delete;

        //- Copy constructor setting internal field reference
        moldingSlipVelocityFvPatchVectorField
        (
            const moldingSlipVelocityFvPatchVectorField&,
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
                new moldingSlipVelocityFvPatchVectorField(*this, iF)
            );
        }


    // Member Functions

        //- Slip length [m]
        scalar slipLength() const
        {
            return slipLength_;
        }

        //- Update the slip value fraction from the slip length
        virtual void updateCoeffs();

        //- Write
        virtual void write(Ostream&) const;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
