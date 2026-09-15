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

private:

    // Initial residual from the previous displacement solve.  Persisting it
    // across pressureCorrector calls makes acceleration protection effective
    // when the normal configuration uses one corrector per time step.
    scalar previousInitialResidual_;

    // Raw displacement increment from the previous time step.  Used to
    // reject an extrapolation that suddenly amplifies an otherwise finite
    // linear solve on a refined mesh.
    scalar previousIncrement_;
};

} // End namespace solvers
} // End namespace Foam

#endif
