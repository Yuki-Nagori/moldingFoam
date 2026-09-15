/*---------------------------------------------------------------------------*\
  moldingFoam: corrected solid displacement solver module
\*---------------------------------------------------------------------------*/

#include "moldingSolidDisplacement.H"
#include "addToRunTimeSelectionTable.H"
#include "fvmD2dt2.H"
#include "fvmLaplacian.H"
#include "fvcDiv.H"
#include "fvcGrad.H"

namespace Foam
{
namespace solvers
{

defineTypeNameAndDebug(moldingSolidDisplacement, 0);
addToRunTimeSelectionTable(solver, moldingSolidDisplacement, fvMesh);

moldingSolidDisplacement::moldingSolidDisplacement(fvMesh& mesh)
:
    solidDisplacement(mesh)
{}


void moldingSolidDisplacement::pressureCorrector()
{
    volVectorField& D(D_);
    const volScalarField& rho = thermo_.rho();

    int iCorr = 0;
    scalar residual = GREAT;
    scalar previousResidual = GREAT;

    do
    {
        fvVectorMatrix DEqn
        (
            fvm::d2dt2(rho, D)
         ==
            fvm::laplacian(2*mu + lambda, D, "laplacian(DD,D)")
          + divSigmaExp
          + rho*fvModels().d2dt2(D)
        );

        if (thermo_.thermalStress())
        {
            DEqn += fvc::grad(threeKalpha*T);
        }

        fvConstraints().constrain(DEqn);

        // Re-evaluate the residual for every corrector.  The upstream
        // implementation stores the first solve residual and reuses it in
        // the loop condition, so nCorrectors > 1 cannot converge correctly.
        residual = DEqn.solve().max().initialResidual();

        // The steady acceleration is an extrapolation of the displacement
        // increment.  Do not extrapolate after a residual increase: on fine
        // meshes that amplifies an under-resolved corrector and can produce
        // the terminal displacement jump seen in the convergence matrix.
        const bool residualDecreased = residual <= previousResidual;
        if (mesh.schemes().steady() && accFac > 1 && residualDecreased)
        {
            D += (accFac - 1)*(D - D.oldTime());
        }
        previousResidual = residual;

        if (!compactNormalStress)
        {
            divSigmaExp = fvc::div(DEqn.flux());
        }

        const volTensorField gradD(fvc::grad(D));
        sigmaD = mu*twoSymm(gradD) + (lambda*I)*tr(gradD);

        if (compactNormalStress)
        {
            divSigmaExp = fvc::div
            (
                sigmaD - (2*mu + lambda)*gradD,
                "div(sigmaD)"
            );
        }
        else
        {
            divSigmaExp += fvc::div(sigmaD);
        }

        ++iCorr;
    }
    while (residual > convergenceTolerance && iCorr < nCorr);
}

} // End namespace solvers
} // End namespace Foam
