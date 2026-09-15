/*---------------------------------------------------------------------------*\
  moldingFoam: corrected solid displacement solver module
\*---------------------------------------------------------------------------*/

#ifndef moldingSolidDisplacement_H
#define moldingSolidDisplacement_H

#include "solidDisplacement.H"

namespace Foam
{
namespace solvers
{

class moldingSolidDisplacement
:
    public solidDisplacement
{
public:

    TypeName("moldingSolidDisplacement");

    explicit moldingSolidDisplacement(fvMesh& mesh);

    virtual void pressureCorrector() override;
};

} // End namespace solvers
} // End namespace Foam

#endif
