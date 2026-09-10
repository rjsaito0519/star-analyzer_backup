#include "KfParticleHelper.h"

#include "KFPTrack.h"
#include "cuts/KfParticleCutConfig.h"
#include "StEvent/StDcaGeometry.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoTrack.h"
#include "StPicoEvent/StPicoTrackCovMatrix.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StarClassLibrary/StPhysicalHelixD.hh"
#include "StarClassLibrary/StThreeVectorF.hh"
#include "StarClassLibrary/SystemOfUnits.h"

#include <cmath>

namespace {
template <class T> bool IsFiniteArray(const T* values, Int_t size) {
  for (Int_t i = 0; i < size; ++i)
    if (!std::isfinite(values[i])) return false;
  return true;
}
}

KfLambdaCandidate::KfLambdaCandidate()
    : x(0.f), y(0.f), z(0.f), px(0.f), py(0.f), pz(0.f),
      mass(0.f), massError(0.f), chi2Ndf(0.f), topoChi2Ndf(0.f),
      daughterDistance(0.f), distanceToPv(0.f), decayLength(0.f),
      decayLengthError(0.f), decayLengthSignificance(0.f),
      vertexLineLength(0.f), vertexLineLengthError(0.f),
      vertexLineLengthSignificance(0.f), cosPointing(-1.f),
      protonPidPull(0.f), pionPidPull(0.f), protonTofPull(0.f),
      pionTofPull(0.f), protonTofM2(0.f), pionTofM2(0.f),
      protonDcaToPv(0.), pionDcaToPv(0.),
      protonHelixPathLength(0.), pionHelixPathLength(0.), imp5PathValid(kFALSE),
      protonHasTof(kFALSE), pionHasTof(kFALSE), protonId(-1), pionId(-1),
      protonIndex(-1), pionIndex(-1), pdg(0) {}

KfXiCandidate::KfXiCandidate()
    : x(0.f), y(0.f), z(0.f), px(0.f), py(0.f), pz(0.f),
      mass(0.f), massError(0.f), chi2Ndf(0.f), topoChi2Ndf(0.f),
      daughterDistance(0.f), distanceToPv(0.f), decayLength(0.f),
      decayLengthError(0.f), decayLengthSignificance(0.f),
      vertexLineLength(0.f), vertexLineLengthError(0.f),
      vertexLineLengthSignificance(0.f), cosPointing(-1.f),
      lambdaMass(0.f), lambdaMassError(0.f),
      protonPidPull(0.f), pionPidPull(0.f), bachelorPidPull(0.f),
      protonTofPull(0.f), pionTofPull(0.f), bachelorTofPull(0.f),
      protonTofM2(0.f), pionTofM2(0.f), bachelorTofM2(0.f),
      protonHasTof(kFALSE), pionHasTof(kFALSE), bachelorHasTof(kFALSE),
      protonId(-1), pionId(-1), bachelorId(-1),
      protonIndex(-1), pionIndex(-1), bachelorIndex(-1),
      lambdaPdg(0), pdg(0) {}

KfParticleHelper::KfParticleHelper(const KfParticleCutConfig& cuts) : mCuts(cuts) {}

Bool_t KfParticleHelper::BuildTrack(StPicoDst* picoDst, Int_t trackIndex,
                                  star_analyzer_kfp::KFPTrack& output) const {
  if (!picoDst || !picoDst->picoArray(StPicoArrays::Track) ||
      !picoDst->picoArray(StPicoArrays::TrackCovMatrix) || trackIndex < 0 ||
      static_cast<UInt_t>(trackIndex) >= picoDst->numberOfTracks() ||
      static_cast<UInt_t>(trackIndex) >= picoDst->numberOfTrackCovMatrices())
    return kFALSE;
  StPicoTrack* track = picoDst->track(trackIndex);
  StPicoTrackCovMatrix* covariance = picoDst->trackCovMatrix(trackIndex);
  if (!track || !covariance || !track->charge()) return kFALSE;
  if (mCuts.rejectBadCovariance && covariance->isBadCovMatrix()) return kFALSE;

  const Float_t* parameters = covariance->params();
  const Float_t* sigmas = covariance->sigmas();
  const Float_t* correlations = covariance->correlations();
  if (!parameters || !sigmas || !correlations ||
      !IsFiniteArray(parameters, 6) || !IsFiniteArray(sigmas, 5) ||
      !IsFiniteArray(correlations, 10)) return kFALSE;

  // SL24y StPicoTrackCovMatrix::dcaGeometry(), without defining the unrelated
  // __TFG__VERSION__ macro globally. Lower triangle: five fitted helix params.
  Float_t helixCovariance[15] = {0.f};
  Int_t diagonalIndex = 0;
  for (Int_t i = 0; i < 5; ++i) {
    if (sigmas[i] < 0.f) return kFALSE;
    helixCovariance[diagonalIndex] = sigmas[i] * sigmas[i];
    for (Int_t j = 0; j < i; ++j) {
      const Int_t matrixIndex = diagonalIndex - i + j;
      const Int_t correlationIndex = matrixIndex - i;
      helixCovariance[matrixIndex] =
          correlations[correlationIndex] * sigmas[i] * sigmas[j];
    }
    diagonalIndex += i + 2;
  }
  StDcaGeometry geometry;
  geometry.set(parameters, helixCovariance);
  Double_t xyzp[6] = {0.};
  Double_t cartesianCovariance[21] = {0.};
  geometry.GetXYZ(xyzp, cartesianCovariance);
  if (!IsFiniteArray(xyzp, 6) || !IsFiniteArray(cartesianCovariance, 21))
    return kFALSE;
  const Int_t diagonal[] = {0, 2, 5, 9, 14, 20};
  for (Int_t i = 0; i < 6; ++i) {
    const Double_t variance = cartesianCovariance[diagonal[i]];
    const Double_t maximum = i < 3 ? mCuts.maxPositionVariance
                                    : mCuts.maxMomentumVariance;
    if (variance < 0. || variance >= maximum) return kFALSE;
  }
  output.SetParameters(xyzp);
  output.SetCovarianceMatrix(cartesianCovariance);
  output.SetCharge(track->charge());
  output.SetID(track->id());
  // Match reference GetTrack (NDF=1, default chi2=0). Track quality was checked
  // separately; the helix fit's chi2 must not be added to the decay fit.
  output.SetChi2(0.f);
  output.SetNDF(1);
  return kTRUE;
}

Bool_t KfParticleHelper::FillImp5PathLengths(StPicoDst* picoDst, KfLambdaCandidate& c) const {
  c.imp5PathValid = kFALSE;
  c.protonHelixPathLength = c.pionHelixPathLength = 0.;
  if (!picoDst || !picoDst->event() || c.protonIndex < 0 || c.pionIndex < 0 ||
      static_cast<UInt_t>(c.protonIndex) >= picoDst->numberOfTracks() ||
      static_cast<UInt_t>(c.pionIndex) >= picoDst->numberOfTracks()) return kFALSE;
  const StPicoTrack* tracks[2] = {picoDst->track(c.protonIndex), picoDst->track(c.pionIndex)};
  StPhysicalHelixD helices[2];
  for (Int_t i = 0; i < 2; ++i) {
    if (!tracks[i]) return kFALSE;
    const TVector3 p = tracks[i]->gMom();
    const TVector3 origin = tracks[i]->origin();
    if (!std::isfinite(p.X()) || !std::isfinite(p.Y()) || !std::isfinite(p.Z()) ||
        !std::isfinite(origin.X()) || !std::isfinite(origin.Y()) ||
        !std::isfinite(origin.Z()) || p.Mag2() <= 0.) return kFALSE;
    const StThreeVectorF momentum(p.X(), p.Y(), p.Z());
    const StThreeVectorF position(origin.X(), origin.Y(), origin.Z());
    helices[i] = StPhysicalHelixD(momentum, position,
        picoDst->event()->bField() * units::kilogauss, (Float_t)tracks[i]->charge());
  }
  // Same two-helix path search and input states as StLambdaMaker. Its returned
  // physical path lengths are NOT KF dS (which has different units).
  const std::pair<Double_t, Double_t> paths = helices[0].pathLengths(helices[1]);
  if (!std::isfinite(paths.first) || !std::isfinite(paths.second)) return kFALSE;
  c.protonHelixPathLength = paths.first;
  c.pionHelixPathLength = paths.second;
  c.imp5PathValid = kTRUE;
  return kTRUE;
}
