/*---------------------------------------------------------------------------*\

    DAFoam  : Discrete Adjoint with OpenFOAM
    Version : v2

\*---------------------------------------------------------------------------*/

#include "nutUSpaldingWallFunctionFvPatchScalarFieldDF.H"
#include "turbulenceModel.H"
#include "fvPatchFieldMapper.H"
#include "volFields.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

tmp<scalarField> nutUSpaldingWallFunctionFvPatchScalarFieldDF::calcNut() const
{
    const label patchi = patch().index();

    const turbulenceModel& turbModel = db().lookupObject<turbulenceModel>(
        IOobject::groupName(
            turbulenceModel::propertiesName,
            internalField().group()));
    const fvPatchVectorField& Uw = turbModel.U().boundaryField()[patchi];
    const scalarField magGradU(mag(Uw.snGrad()));
    const tmp<scalarField> tnuw = turbModel.nu(patchi);
    const scalarField& nuw = tnuw();

    // Calculate new viscosity
    tmp<scalarField> tnutw(
        max(
            scalar(0),
            sqr(calcUTau(magGradU)) / (magGradU + ROOTVSMALL) - nuw));

    if (tolerance_ != 1.e-14)
    {
        // User-specified tolerance. Find out if current nut already satisfies
        // eqns.

        // Run ut for one iteration
        scalarField err;
        tmp<scalarField> UTau(calcUTau(magGradU, 1, err));

        // Preserve nutw if the initial error (err) already lower than the
        // tolerance.

        scalarField& nutw = tnutw.ref();
        forAll(err, facei)
        {
            if (err[facei] < tolerance_)
            {
                nutw[facei] = this->operator[](facei);
            }
        }
    }
    return tnutw;
}

tmp<scalarField> nutUSpaldingWallFunctionFvPatchScalarFieldDF::calcUTau(
    const scalarField& magGradU) const
{
    scalarField err;
    return calcUTau(magGradU, maxIter_, err);
}

tmp<scalarField> nutUSpaldingWallFunctionFvPatchScalarFieldDF::calcUTau(
    const scalarField& magGradU,
    const label maxIter,
    scalarField& err) const
{
    const label patchi = patch().index();

    const turbulenceModel& turbModel = db().lookupObject<turbulenceModel>(
        IOobject::groupName(
            turbulenceModel::propertiesName,
            internalField().group()));
    const scalarField& y = turbModel.y()[patchi];

    const fvPatchVectorField& Uw = turbModel.U().boundaryField()[patchi];
    const scalarField magUp(mag(Uw.patchInternalField() - Uw));

    const tmp<scalarField> tnuw = turbModel.nu(patchi);
    const scalarField& nuw = tnuw();

    const scalarField& nutw = *this;

    tmp<scalarField> tuTau(new scalarField(patch().size(), Zero));
    scalarField& uTau = tuTau.ref();

    err.setSize(uTau.size());
    err = 0.0;

    forAll(uTau, facei)
    {
        scalar ut = sqrt((nutw[facei] + nuw[facei]) * magGradU[facei]);
        // Note: for exact restart seed with laminar viscosity only:
        //scalar ut = sqrt(nuw[facei]*magGradU[facei]);

        if (ROOTVSMALL < ut)
        {
            int iter = 0;

            do
            {
                scalar kUu = min(kappa_ * magUp[facei] / ut, 50);
                scalar fkUu = exp(kUu) - 1 - kUu * (1 + 0.5 * kUu);

                scalar f =
                    -ut * y[facei] / nuw[facei]
                    + magUp[facei] / ut
                    + 1 / E_ * (fkUu - 1.0 / 6.0 * kUu * sqr(kUu));

                scalar df =
                    y[facei] / nuw[facei]
                    + magUp[facei] / sqr(ut)
                    + 1 / E_ * kUu * fkUu / ut;

                scalar uTauNew = ut + f / df;
                err[facei] = mag((ut - uTauNew) / ut);
                ut = uTauNew;

                //iterations_++;

            } while (
                ut > ROOTVSMALL
                && err[facei] > tolerance_
                && ++iter < maxIter);

            uTau[facei] = max(0.0, ut);
        }
    }

    return tuTau;
}

void Foam::nutUSpaldingWallFunctionFvPatchScalarFieldDF::writeLocalEntries(
    Ostream& os) const
{
    nutWallFunctionFvPatchScalarField::writeLocalEntries(os);

    os.writeEntryIfDifferent<label>("maxIter", 1000, maxIter_);
    os.writeEntryIfDifferent<scalar>("tolerance", 1.e-14, tolerance_);
}

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

nutUSpaldingWallFunctionFvPatchScalarFieldDF::
    nutUSpaldingWallFunctionFvPatchScalarFieldDF(
        const fvPatch& p,
        const DimensionedField<scalar, volMesh>& iF)
    : nutWallFunctionFvPatchScalarField(p, iF),
      maxIter_(1000),
      tolerance_(1.e-14)
{
}

nutUSpaldingWallFunctionFvPatchScalarFieldDF::
    nutUSpaldingWallFunctionFvPatchScalarFieldDF(
        const nutUSpaldingWallFunctionFvPatchScalarFieldDF& ptf,
        const fvPatch& p,
        const DimensionedField<scalar, volMesh>& iF,
        const fvPatchFieldMapper& mapper)
    : nutWallFunctionFvPatchScalarField(ptf, p, iF, mapper),
      maxIter_(ptf.maxIter_),
      tolerance_(ptf.tolerance_)
{
}

nutUSpaldingWallFunctionFvPatchScalarFieldDF::
    nutUSpaldingWallFunctionFvPatchScalarFieldDF(
        const fvPatch& p,
        const DimensionedField<scalar, volMesh>& iF,
        const dictionary& dict)
    : nutWallFunctionFvPatchScalarField(p, iF, dict),
      maxIter_(dict.lookupOrDefault<label>("maxIter", 1000)),
      tolerance_(dict.lookupOrDefault<scalar>("tolerance", 1.e-14))
{
}

nutUSpaldingWallFunctionFvPatchScalarFieldDF::
    nutUSpaldingWallFunctionFvPatchScalarFieldDF(
        const nutUSpaldingWallFunctionFvPatchScalarFieldDF& wfpsf)
    : nutWallFunctionFvPatchScalarField(wfpsf),
      maxIter_(wfpsf.maxIter_),
      tolerance_(wfpsf.tolerance_)
{
}

nutUSpaldingWallFunctionFvPatchScalarFieldDF::
    nutUSpaldingWallFunctionFvPatchScalarFieldDF(
        const nutUSpaldingWallFunctionFvPatchScalarFieldDF& wfpsf,
        const DimensionedField<scalar, volMesh>& iF)
    : nutWallFunctionFvPatchScalarField(wfpsf, iF),
      maxIter_(wfpsf.maxIter_),
      tolerance_(wfpsf.tolerance_)
{
}

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

tmp<scalarField> nutUSpaldingWallFunctionFvPatchScalarFieldDF::yPlus() const
{
    const label patchi = patch().index();

    const turbulenceModel& turbModel = db().lookupObject<turbulenceModel>(
        IOobject::groupName(
            turbulenceModel::propertiesName,
            internalField().group()));
    const scalarField& y = turbModel.y()[patchi];
    const fvPatchVectorField& Uw = turbModel.U().boundaryField()[patchi];
    const tmp<scalarField> tnuw = turbModel.nu(patchi);
    const scalarField& nuw = tnuw();

    return y * calcUTau(mag(Uw.snGrad())) / nuw;
}

void nutUSpaldingWallFunctionFvPatchScalarFieldDF::write(Ostream& os) const
{
    fvPatchField<scalar>::write(os);
    writeLocalEntries(os);
    writeEntry("value", os);
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

makePatchTypeField(
    fvPatchScalarField,
    nutUSpaldingWallFunctionFvPatchScalarFieldDF);

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
