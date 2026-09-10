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
    Foam::moldingMoldTemperatureFvPatchScalarField

Description
    Lumped-parameter mould temperature boundary for the cavity walls.

    The patch bounds a mould which is represented as a single thermal mass
    of heat capacity \c heatCapacity [J/K] and temperature \c T. It
    exchanges heat with the casting through the fluid-side film
    conductance (formed from the same \c kappaEff*deltaCoeffs*magSf face
    conductances as the energy equation), with the cooling water through
    the conductance \c waterHTC*wettedArea [W/K] at \c waterTemperature,
    and with an optional power source \c Q [W]:

    \verbatim
        C dT/dt = Q_b + Q_w + Q
    \endverbatim

    where \c Q_b is the heat transferred across the boundary from the
    casting (positive into the mould) and \c Q_w = hA(waterTemperature-T)
    is the cooling-water exchange. The condition uses the backward-Euler
    update of this balance,

    \verbatim
        T = (C/dt T_old + sum(Hf T_cell) + hA T_water + Q)
          / (C/dt + sum(Hf) + hA)
    \endverbatim

    which is the discrete counterpart of the implicit energy equation of
    the casting, is unconditionally stable, and makes the mould state an
    implicit part of every energy solve (the condition is re-evaluated
    within each outer iteration and always advances from \c T_old, so it
    refines the coupling without over-integrating the state).

    The temperature persists across restarts through the written \c T
    entry, mirroring the upstream \c lumpedMassTemperature condition.

Usage
    \table
        Property          | Description                          | Req'd? | Default
        heatCapacity      | Mould heat capacity [J/K]            | yes    |
        waterHTC          | Cooling-water HTC [W/m^2/K]          | no     | 0
        wettedArea        | Wetted cooling area [m^2]            | no     | 0
        waterTemperature  | Cooling-water temperature [K]        | no     | 300
        Q                 | Power source [W]                     | no     | 0
        T                 | Initial mould temperature [K]        | yes    |
        value             | Initial patch temperature [K]        | yes    |
    \endtable

    Example:
    \verbatim
    walls
    {
        type             moldingMoldTemperature;
        heatCapacity     500;      // [J/K]
        waterHTC         2000;     // [W/m^2/K]
        wettedArea       2.7e-3;   // [m^2]
        waterTemperature 300;      // [K]
        T                353;      // [K] initial mould temperature
        value            uniform 353;
    }
    \endverbatim

SourceFiles
    moldingMoldTemperatureFvPatchScalarField.C

See also
    Foam::lumpedMassTemperatureFvPatchScalarField

\*---------------------------------------------------------------------------*/

#ifndef moldingMoldTemperatureFvPatchScalarField_H
#define moldingMoldTemperatureFvPatchScalarField_H

#include "fixedValueFvPatchFields.H"
#include "UniformDimensionedField.H"
#include "Function1.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
        Class moldingMoldTemperatureFvPatchScalarField Declaration
\*---------------------------------------------------------------------------*/

class moldingMoldTemperatureFvPatchScalarField
:
    public fixedValueFvPatchScalarField
{
    // Private Data

        //- Mould heat capacity [J/K]
        scalar C_;

        //- Cooling-water heat transfer coefficient [W/m^2/K]
        scalar waterHTC_;

        //- Wetted cooling area [m^2]
        scalar wettedArea_;

        //- Cooling-water temperature [K]
        scalar Tw_;

        //- Wall thermal resistance to the deep mould [m^2 K/W]; zero
        //  (default) disables the deep-mould path
        scalar wallResistance_;

        //- Deep mould temperature beyond the wall resistance [K]
        scalar deepMoldTemperature_;

        //- Temperature of the mould thermal mass [K]
        UniformDimensionedField<scalar> T_;

        //- Power source [W]
        autoPtr<Function1<scalar>> Q_;


public:

    //- Runtime type information
    TypeName("moldingMoldTemperature");


    // Constructors

        //- Construct from patch, internal field and dictionary
        moldingMoldTemperatureFvPatchScalarField
        (
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const dictionary&
        );

        //- Construct by mapping given field onto a new patch
        moldingMoldTemperatureFvPatchScalarField
        (
            const moldingMoldTemperatureFvPatchScalarField&,
            const fvPatch&,
            const DimensionedField<scalar, fvMesh>&,
            const fieldMapper&
        );

        //- Disallow copy without setting internal field reference
        moldingMoldTemperatureFvPatchScalarField
        (
            const moldingMoldTemperatureFvPatchScalarField&
        ) = delete;

        //- Copy constructor setting internal field reference
        moldingMoldTemperatureFvPatchScalarField
        (
            const moldingMoldTemperatureFvPatchScalarField&,
            const DimensionedField<scalar, fvMesh>&
        );

        //- Construct and return a clone setting internal field reference
        virtual tmp<fvPatchScalarField> clone
        (
            const DimensionedField<scalar, fvMesh>& iF
        ) const
        {
            return tmp<fvPatchScalarField>
            (
                new moldingMoldTemperatureFvPatchScalarField(*this, iF)
            );
        }


    // Member Functions

        //- Mould heat capacity [J/K]
        scalar C() const
        {
            return C_;
        }

        //- Cooling-water side conductance h*A [W/K]
        scalar hA() const
        {
            return waterHTC_*wettedArea_;
        }

        //- Cooling-water heat transfer coefficient [W/m^2/K]
        scalar waterHTC() const
        {
            return waterHTC_;
        }

        //- Wetted cooling area [m^2]
        scalar wettedArea() const
        {
            return wettedArea_;
        }

        //- Cooling-water temperature [K]
        scalar Tw() const
        {
            return Tw_;
        }

        //- Wall resistance to the deep mould [m^2 K/W]
        scalar wallResistance() const
        {
            return wallResistance_;
        }

        //- Deep mould temperature [K]
        scalar deepMoldTemperature() const
        {
            return deepMoldTemperature_;
        }

        //- Current mould temperature [K]
        scalar T() const
        {
            return T_.value();
        }


        // Mapping functions

            //- Map the given fvPatchField onto this fvPatchField
            virtual void map(const fvPatchScalarField&, const fieldMapper&);

            //- Reset the fvPatchField to the given fvPatchField
            virtual void reset(const fvPatchScalarField&);


        // Evaluation functions

            //- Update the coefficients associated with the patch field
            virtual void updateCoeffs();


        //- Write
        virtual void write(Ostream&) const;


    // Member Operators

        //- Inherit assignment
        using fixedValueFvPatchScalarField::operator=;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
