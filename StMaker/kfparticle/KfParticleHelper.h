#ifndef KF_PARTICLE_HELPER_H
#define KF_PARTICLE_HELPER_H

#include "Rtypes.h"

namespace star_analyzer_kfp { class KFPTrack; }
class KfParticleCutConfig;
class StPicoDst;

// Plain output only: no upstream KF types or ROOT dictionary are exposed.
// Mass and chi2Ndf are from the mass-unconstrained Finder candidate.
struct KfLambdaCandidate {
  KfLambdaCandidate();
  Float_t x, y, z, px, py, pz;
  Float_t mass, massError, chi2Ndf, topoChi2Ndf;
  Float_t daughterDistance, distanceToPv;
  Float_t decayLength, decayLengthError, decayLengthSignificance;
  Float_t vertexLineLength, vertexLineLengthError, vertexLineLengthSignificance;
  Float_t cosPointing;
  Float_t protonPidPull, pionPidPull;
  Float_t protonTofPull, pionTofPull, protonTofM2, pionTofM2;
  // Imp5-only diagnostics, zero/false for the default reference profile.
  Double_t protonDcaToPv, pionDcaToPv;
  Double_t protonHelixPathLength, pionHelixPathLength;
  Bool_t imp5PathValid;
  Bool_t protonHasTof, pionHasTof;
  Int_t protonId, pionId, protonIndex, pionIndex, pdg;
};

// Cascade Ξ → Λπ. Kinematics follow the parent Ξ; IDs recover Λ daughters + bachelor.
struct KfXiCandidate {
  KfXiCandidate();
  Float_t x, y, z, px, py, pz;
  Float_t mass, massError, chi2Ndf, topoChi2Ndf;
  Float_t daughterDistance, distanceToPv;
  Float_t decayLength, decayLengthError, decayLengthSignificance;
  Float_t vertexLineLength, vertexLineLengthError, vertexLineLengthSignificance;
  Float_t cosPointing;
  Float_t lambdaMass, lambdaMassError;
  Float_t protonPidPull, pionPidPull, bachelorPidPull;
  Float_t protonTofPull, pionTofPull, bachelorTofPull;
  Float_t protonTofM2, pionTofM2, bachelorTofM2;
  Bool_t protonHasTof, pionHasTof, bachelorHasTof;
  Int_t protonId, pionId, bachelorId;
  Int_t protonIndex, pionIndex, bachelorIndex;
  Int_t lambdaPdg, pdg;
};

class KfParticleHelper {
public:
  explicit KfParticleHelper(const KfParticleCutConfig& cuts);
  Bool_t BuildTrack(StPicoDst* picoDst, Int_t trackIndex,
                    star_analyzer_kfp::KFPTrack& output) const;
  // Original-pair numerical path guard only; never replaces the KF fit/mass.
  Bool_t FillImp5PathLengths(StPicoDst* picoDst, KfLambdaCandidate& candidate) const;
private:
  const KfParticleCutConfig& mCuts;
};

#endif
