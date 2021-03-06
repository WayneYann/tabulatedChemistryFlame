rho = Rho(p,psi);
rho = max(rho, rhoMin);
rho = min(rho, rhoMax);
rho.relax();

volScalarField rAU(1.0/UEqn.A());
volScalarField rAtU(1.0/(1.0/rAU - UEqn.H1()));
volVectorField HbyA(constrainHbyA(rAU*UEqn.H(), U, p));

if (pimple.nCorrPISO() <= 1)
{
    tUEqn.clear();
}

if (pimple.transonic())
{
    surfaceScalarField phid
    (
        "phid",
        fvc::interpolate(psi)
       *(
            fvc::flux(HbyA)
          + fvc::interpolate(rho*rAU)*fvc::ddtCorr(rho, U, phi)
           /fvc::interpolate(rho)
        )
    );

    MRF.makeRelative(fvc::interpolate(psi), phid);

    surfaceScalarField phic
    (
        "phic",
        fvc::interpolate(rho*(rAtU - rAU))*fvc::snGrad(p)*mesh.magSf()
    );

    HbyA -= (rAU - rAtU)*fvc::grad(p);

    volScalarField rhorAtU("rhorAtU", rho*rAtU);

    while (pimple.correctNonOrthogonal())
    {
        fvScalarMatrix pEqn
        (
            fvm::ddt(psi, p)
          + fvm::div(phid, p)
          + fvc::div(phic)
          - fvm::laplacian(rhorAtU, p)
         ==
            fvOptions(psi, p, rho.name())
        );

        pEqn.solve(mesh.solver(p.select(pimple.finalInnerIter())));

        if (pimple.finalNonOrthogonalIter())
        {
            phi == phic + pEqn.flux();
        }
    }
}
else
{
    surfaceScalarField phiHbyA
    (
        "phiHbyA",
        (
            fvc::flux(rho*HbyA)
          + fvc::interpolate(rho*rAU)*fvc::ddtCorr(rho, U, phi)
        )
    );

    MRF.makeRelative(fvc::interpolate(rho), phiHbyA);

    phiHbyA += fvc::interpolate(rho*(rAtU - rAU))*fvc::snGrad(p)*mesh.magSf();
    HbyA -= (rAU - rAtU)*fvc::grad(p);

    volScalarField rhorAtU("rhorAtU", rho*rAtU);

    // Update the pressure BCs to ensure flux consistency
    constrainPressure(p, rho, U, phiHbyA, rhorAtU, MRF);

    while (pimple.correctNonOrthogonal())
    {
        fvScalarMatrix pEqn
        (
            fvm::ddt(psi, p)
          + fvc::div(phiHbyA)
          - fvm::laplacian(rhorAtU, p)
         ==
            fvOptions(psi, p, rho.name())
        );

        pEqn.solve(mesh.solver(p.select(pimple.finalInnerIter())));

        if (pimple.finalNonOrthogonalIter())
        {
            phi = phiHbyA + pEqn.flux();
        }
    }
}

#include "rhoEqn.H"
//#include "compressibleContinuityErrs.H"
{
    dimensionedScalar totalMass = fvc::domainIntegrate(rho);

    scalar sumLocalContErr = (fvc::domainIntegrate(mag(rho - Rho(p,psi)))/totalMass).value();

    scalar globalContErr = (fvc::domainIntegrate(rho - Rho(p,psi))/totalMass).value();

    cumulativeContErr += globalContErr;

    Info << "time step continuity errors : sum local = " << sumLocalContErr
         << ", global = " << globalContErr
         << ", cumulative = " << cumulativeContErr
         << endl;
}

// Explicitly relax pressure for momentum corrector
p.relax();

U = HbyA - rAtU*fvc::grad(p);
U.correctBoundaryConditions();
fvOptions.correct(U);
K = 0.5*magSqr(U);

/*
if (thermo.dpdt())
{
    dpdt = fvc::ddt(p);
}
*/

// Recalculate density from the relaxed pressure
rho = Rho(p,psi);
rho = max(rho, rhoMin);
rho = min(rho, rhoMax);

if (!pimple.transonic())
{
    rho.relax();
}

Info<< "rho max/min : " << max(rho).value() << " " << min(rho).value() << endl;
